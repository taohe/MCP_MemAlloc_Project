#include <algorithm>
#include <iostream>
#include <iomanip>
#include <stdlib.h> // exit
#include <string.h> // strlen

#include "buffer.hpp"
#include "logging.hpp"

namespace base {

using std::cout;
using std::endl;
using std::min;

Buffer::Buffer()
  : wpos_(addChunk()), rpos_(wpos_) {}

Buffer::~Buffer() {
  for (Chunks::iterator it = chunks_.begin(); it != chunks_.end(); ++it) {
    delete [] *it;
  }
}

bool Buffer::reserve(size_t bytes) {
  if (bytes > BlockSize) {
    return false;
  }

  // current write chunk has place enough
  if (BlockSize - wpos_.off >= bytes ) {
    return true;
  }

  // if current chunk was fully consumed already, move reading pointer
  // to the chunk about to be allocated
  bool should_advance = false;
  if (rpos_ == wpos_) {
    should_advance = true;
  }

  // allocate a new chunk and skip to it
  wpos_ = addChunk();
  if (should_advance) {
    rpos_ = wpos_;
    dropChunks(1);
  }

  return true;
}

bool Buffer::advance(size_t bytes) {
  if (bytes <= 0) return false;

  if (bytes <= writeSize()) {
    wpos_.off += bytes;
    sizes_[wpos_.idx] += bytes;
    return true;
  }
  return false;
}

void Buffer::write(const MemPiece& data) {
  size_t bytes_left = data.len();
  const char *src = data.ptr();
  while (bytes_left > 0) {
    // calculate how much to write in this chunk
    char* dest = writePtr();
    const size_t bytes_in_chunk = min(writeSize(), bytes_left);
    bytes_left -= bytes_in_chunk;

    // write as much as possible in this chunk
    for (size_t i = bytes_in_chunk ; i > 0; i--, dest++, src++) {
      *dest = *src;
    }

    // the write pointer should be on the next free byte in the chunk
    sizes_[wpos_.idx] += bytes_in_chunk;
    wpos_.off += bytes_in_chunk;

    // allocate new chunk if necessary
    if ((bytes_left > 0) || (wpos_.off == BlockSize)) {
      wpos_ = addChunk();
    }
  }
}

char* Buffer::maybeRemoveLastChunk() {
  char* last_chunk = NULL;
  if (sizes_[wpos_.idx] == 0) {
    // wpos_ will become invalid, caller should fix it
    last_chunk = chunks_[wpos_.idx];
    chunks_.pop_back();
    sizes_.pop_back();
  }
  return last_chunk;
}

void Buffer::appendFrom(Buffer* other) {
  if (other->isConsumed()) {
    LOG(LogMessage::FATAL) << "Can't append from consumed buffer";
    return;
  }

  // Is there anything to append from?
  if (other->readSize() == 0) {
    return;
  }

  // avoid appending empty chunks or appending after one
  char* last_chunk = this->maybeRemoveLastChunk();
  char* other_last_chunk = other->maybeRemoveLastChunk();

  // append all ohter's chunks into 'this'
  for (size_t i = 0; i < other->chunks_.size(); i++) {
    chunks_.push_back(other->chunks_[i]);
    sizes_.push_back(other->sizes_[i]);
  }

  // if the last appended chunk is full, add a new (or the saved)
  // chunk
  if (sizes_.back() == BlockSize ) {
    if (last_chunk == NULL) {
      addChunk();
    } else {
      chunks_.push_back(last_chunk);
      sizes_.push_back(0);
    }
  } else {
    delete [] last_chunk;
  }

  // adjust pointer if it was on eob before
  if (readSize() == 0) {
    rpos_.idx +=1;
    rpos_.off = 0;
  }

  // adjust write pointer to new ending chunk
  wpos_.idx = chunks_.size() - 1;
  wpos_.off = sizes_.back();

  // adjust other's state
  other->chunks_.clear();
  other->sizes_.clear();
  if (other_last_chunk == NULL) {
    other->wpos_ = other->addChunk();
  } else {
    other->chunks_.push_back(other_last_chunk);
    other->sizes_.push_back(0);
  }
  other->rpos_ = other->wpos_;
}

void Buffer::copyFrom(Buffer* other) {
  if (other->isConsumed()) {
    LOG(LogMessage::FATAL) << "Can't copy from consummed buffer";
    return;
  }

  // is there anything to copy at all?
  if (other->readSize() == 0) {
    return;
  }

  for (size_t i = 0; i < other->chunks_.size(); i++) {
    // first chunk from 'other' fits in last chunk from 'this'?
    const size_t len = other->sizes_[i];
    if ((i != 0) || (len > writeSize())) {
      wpos_ = addChunk();
    }

    memcpy(writePtr(), other->chunks_[i], len);
    sizes_[wpos_.idx] += len;
    wpos_.off += len;
  }

  // if last chunk is full, allocate a new one
  if (wpos_.off == BlockSize) {
    wpos_ = addChunk();
  }
}

void Buffer::consume(size_t bytes_to_consume) {
  size_t chunks_to_drop = 0;

  // consuming blocks before the last one
  while ((bytes_to_consume > 0) && (rpos_.idx < wpos_.idx)) {
    const size_t bytes_in_chunk = min(bytes_to_consume, readSize());
    bytes_to_consume -= bytes_in_chunk;
    rpos_.off += bytes_in_chunk;

    // drop this chunk later if it was fully consumed
    if (readSize() == 0) {
      chunks_to_drop++;
      rpos_.idx++;
      rpos_.off = 0;
    }
  }

  // consuming the last block, i.e. rpos_.idx == wpos_.idx
  if (bytes_to_consume > 0) {
    const size_t bytes_in_chunk = min(bytes_to_consume, readSize());
    bytes_to_consume -= bytes_in_chunk;
    rpos_.off += bytes_in_chunk;
  }

  // drop consumed chunks
  dropChunks(chunks_to_drop);
}

size_t Buffer::byteCount() const {
  // count bytes in current (reading) chunk
  size_t count = 0;
  count += readSize();

  // count bytes on unread chunks
  for (size_t i = 1; i < sizes_.size(); i++) {
    count += sizes_[i];
  }
  return count;
}

Buffer::Position Buffer::addChunk() {
  char* chunk = new char[BlockSize];
  chunks_.push_back(chunk);
  sizes_.push_back(0);
  return Position(chunks_.size()-1, 0);
}

void Buffer::dropChunks(size_t n) {
  if (n == 0) return;
  wpos_.idx -= n;
  rpos_.idx -= n;
  while (n-- > 0) {
    delete [] chunks_.front();
    chunks_.pop_front();
    sizes_.pop_front();
  }
}

bool Buffer::isConsumed() const {
  if ((rpos_.idx != 0) || (rpos_.off != 0)) {
    return true;
  } else {
    return false;
  }
}

// ************************************************************
// Iterator support
//

Buffer::Iterator::Iterator(Buffer* buffer)
  : buffer_(buffer),
    pos_(buffer_->rpos_),
    bytes_read_(0),
    bytes_total_(buffer->byteCount()),
    budget_(getBudget()),
    chunk_start_(buffer_->chunks_[pos_.idx]) {
}

Buffer::Iterator Buffer::begin() {
  Iterator iter(this);
  iter.pos_ = this->rpos_;
  return iter;
}

Buffer::Iterator Buffer::end() {
  Buffer::Iterator iter(this);
  iter.pos_ = this->wpos_;
  return iter;
}

size_t Buffer::Iterator::getBudget() const {
  // can read as many positions as are there until the end of this chunk
  return buffer_->sizes_[pos_.idx] - pos_.off;
}

void Buffer::Iterator::slowNext() {
  // try advancing in the current chunk first
  if (pos_.off < buffer_->sizes_[pos_.idx]) {
    pos_.off++;
    bytes_read_++;

    // fell off chunk or remainder of the chunk is empty?
    if ((pos_.off == buffer_->sizes_[pos_.idx]) &&
        (pos_.idx < buffer_->wpos_.idx)) {
      pos_.idx++;
      pos_.off = 0;
      chunk_start_ = buffer_->chunks_[pos_.idx];
    }

    budget_ = getBudget();
  }
}

bool Buffer::Position::operator==(const Buffer::Position& other) {
  return (idx == other.idx) && (off == other.off);
}

bool Buffer::Iterator::operator==(const Iterator& other) {
  return (pos_ == other.pos_);
}

bool Buffer::Iterator::operator!=(const Iterator& other) {
  return !(pos_ == other.pos_);
}

} // namespace base
