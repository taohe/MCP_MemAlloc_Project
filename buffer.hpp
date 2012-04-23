#ifndef MCP_BASE_BUFFER_HEADER
#define MCP_BASE_BUFFER_HEADER

#include <deque>
#include <string>

#include "mem_piece.hpp"

namespace base {

using std::deque;

// This is a streaming buffer designed to be shared between a producer
// and a consumer of its data. It has little thread-safety guarantees
// (see below) but it fits well in cases where:
//
//   + the producer is writing data to the buffer that came from a
//   socket and the consumer is reading that data
//
//   + the producer is writing data to the buffer that came from a
//   file and the consumer is reading it to send down a socket
//
// A Buffer is an array of fixed-sized chunks that grows and shrinks
// according to how much data is left to read. The producer is always
// writing to the latest added chunk. New chunks are added if need
// be. The consumer is always reading from the opposite end. As chunks
// are read, they are discarded.
//
//           write point (first empty byte)
//           v
//  AAAAA_AAA__
//  ^
//  read point (first filled byte)
//
//
//  Usage:
//
//    See buffer_test.cpp cases.
//
//
//  Thread-safety:
//
//  Strictly speaking, there is none. So all access to this class
//  should be synchronized. There is however one supported opportunity
//  for concurrency:
//
//  + readPtr() returns a pointer to the next byte to be read. That
//  area has readSize() bytes. After calling these methods in a
//  synchronized way, a consumer can read from that area while a
//  producer is writing new data. Note that the consumer would have to
//  call consume() to signal when it finished reading that area -- and
//  consume() is *not* thread safe.
//
//
//  Caveats:
//
//  The largest piece of data that can be written or read to/from the
//  Buffer has at most the size of a chunk. We're using 4k chunks here.
//
//  Buffer is not the fastest buffer scheme on earth, but it is easy
//  to debug, if need be. The iterator, in particular, can be
//  improved.
//

class Buffer {
public:
  class Iterator;

  Buffer();
  ~Buffer();

  //
  // Writing Support
  //

  // If we know upfront the size of the piece of data we want to add,
  // the following methods should be used.

  // Returns true and makes sure that there is at least 'bytes'
  // available in the current chunk. If 'bytes' is bigger than the
  // pre-defined chunk size, returns false. reserve() adds a new chunk
  // if it needs to. Calling reserve() before writing is optional,
  // provided that the producer won't write more than writeSize()
  // bytes. (see below)
  bool reserve(size_t bytes);

  // Returns the size of the available writing area.
  size_t writeSize() const    { return BlockSize - wpos_.off; }

  // Returns a pointer to the available writing area.
  char* writePtr() const      { return chunks_[wpos_.idx] + wpos_.off; }

  // Returns true and moves the write pointer 'bytes'
  // positions. advance() is called after a producer finished writing
  // a piece of data.
  bool advance(size_t bytes);

  // If we don't know upfront how much data needs to be written (but
  // are sure they fit in chunks), the following methods can be used.

  // write() is equivalent to calling reserve() for the size of
  // data to be written, writing to the resulting WritePtr() area,
  // and calling advance() for that size.
  void write(const MemPiece& data);  // allocates more chunks if needed

  // Takes all chunks from 'other' and moves them into 'this. 'other' 
  // will be empty after that.
  // REQUIRES: can only append from a buffer that was not consumed yet
  void appendFrom(Buffer* other);

  // Takes all chunks from 'other' and copies them into
  // 'this'. 'other' is not changed at all.
  // REQUIRES: can only copy from a buffer that was not yet consumed
  void copyFrom(Buffer* other);  // allocates more chunks if needed

  //
  // Reading Support
  //

  // Returns the size of the amount of data that can be read.
  size_t readSize() const     { return sizes_[rpos_.idx] - rpos_.off; }

  // Returns the pointer to the initial data position.
  const char* readPtr() const { return chunks_[rpos_.idx] + rpos_.off; }

  // Signal that the consumer just ingested 'bytes' bytes.
  void consume(size_t bytes);

  //
  // Iterator Support
  //

  // An iterator that allows to peek on content as if it were
  // contiguous.
  Iterator begin();
  Iterator end();

  // Accessors (of interest mainly to testing)

  enum { BlockSize = 4096 };

  size_t numChunks() const    { return chunks_.size(); }
  size_t byteCount() const;

private:
  typedef deque<char *> Chunks;
  typedef deque<size_t> Sizes;

  struct Position {
    Position(size_t chunk, size_t offset) : idx(chunk), off(offset) {}
    bool operator==(const Position& other);
    size_t idx;
    size_t off;
  };

  // Deque of chunks of 'BlockSize' bytes. Memory is owned by this class.
  Chunks chunks_;

  // Deque of # of bytes the corresponding chunk has filled.
  Sizes sizes_;

  // Writing and reading pointers. A pointer is a pair <chunk num,
  // offset>.
  Position wpos_;
  Position rpos_;

  // Chunk manipulation

  Position addChunk();
  void dropChunks(size_t n);
  char* maybeRemoveLastChunk(); // invalidates wpos_

  bool isConsumed() const;

  // Non-copyable, non-assignable
  Buffer(const Buffer&);
  Buffer& operator=(const Buffer&);
};

//
// Iterator support
//

class Buffer::Iterator {
public:
  ~Iterator() {}

  void next();
  char getChar() const;
  bool eob() const;

  size_t bytesRead() const { return bytes_read_; }
  size_t bytesTotal() const { return bytes_total_; }

  bool operator==(const Iterator& other);
  bool operator!=(const Iterator& other);

private:
  friend class Buffer;

  Buffer*          buffer_;
  Buffer::Position pos_;
  size_t           bytes_read_;
  size_t           bytes_total_;

  size_t           budget_;        // at least this many positions left
  char*            chunk_start_;   // budget_ refers to this base

  explicit Iterator(Buffer* buffer);
  void slowNext();
  size_t getBudget() const;
};

inline bool Buffer::Iterator::eob() const {
  if (budget_) {
    return false;
  }

  return (pos_.idx == buffer_->wpos_.idx) && (pos_.off == buffer_->wpos_.off);
}

inline void Buffer::Iterator::next() {
  // fast path: there's certainly contents left to read in this chunk
  if (budget_ > 1) {
    budget_--;
    pos_.off++;
    bytes_read_++;
    return;
  }

  slowNext();
}

inline char Buffer::Iterator::getChar() const {
  return *(chunk_start_ + pos_.off);
}

}  // namespace base

#endif // MCP_BASE_BUFFER_HEADER

//  LocalWords:  num
