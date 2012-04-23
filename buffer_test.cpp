#include <string>

#include "buffer.hpp"
#include "test_unit.hpp"

namespace {

using base::Buffer;
using std::string;

TEST(SingleChunk, Empty) {
  Buffer buf;
  EXPECT_EQ(buf.writeSize(), Buffer::BlockSize);
  EXPECT_EQ(buf.readSize(), 0);
}

TEST(SingleChunk, FullReserve) {
  Buffer buf;
  EXPECT_TRUE(buf.reserve(Buffer::BlockSize));
  EXPECT_EQ(buf.writeSize(), Buffer::BlockSize);
  EXPECT_EQ(buf.numChunks(), 1);
}

TEST(SingleChunk, DirectReadWrite) {
  Buffer buf;
  const string test_string = "TEST";
  size_t size = test_string.size();

  EXPECT_TRUE(buf.reserve(size));
  buf.write(test_string.c_str());
  EXPECT_EQ(buf.readSize(), size);

  string read_string(buf.readPtr(), buf.readSize());
  EXPECT_EQ(test_string, read_string);

  buf.consume(size);
  EXPECT_EQ(buf.writeSize(), Buffer::BlockSize - size);
  EXPECT_EQ(buf.readSize(), 0)
}

TEST(SingleChunk, Advance) {
  const size_t NUM_CHARS = 5;

  Buffer buf;
  buf.reserve(NUM_CHARS*2);  // reserve more than we'll use
  string read_empty(buf.readPtr(), buf.readSize());
  EXPECT_EQ(read_empty.size(), 0);

  char* p = buf.writePtr();
  EXPECT_GT(buf.writeSize(), NUM_CHARS);
  for (size_t i=0; i<NUM_CHARS; i++) {
    *p++ = 'X';
  }
  buf.advance(NUM_CHARS);
  string xs_string(buf.readPtr(), buf.readSize());
  EXPECT_EQ(xs_string.size(), NUM_CHARS);
  EXPECT_EQ(xs_string, string(NUM_CHARS, 'X'));
}

TEST(SingleChunk, IteratorEmpty) {
  Buffer buf;
  Buffer::Iterator it = buf.begin();
  EXPECT_TRUE(it == buf.end());
  EXPECT_TRUE(it.eob());
  EXPECT_EQ(it.bytesTotal(), 0);

  // trying to read past the end should be a no-op
  it.next();
  EXPECT_TRUE(it.eob());
}

TEST(SingleChunk, IteratorReadWrite) {
  Buffer buf;
  const string test_string = "TEST";
  size_t size = test_string.size();
  buf.write(test_string.c_str());

  Buffer::Iterator it = buf.begin();
  EXPECT_EQ(it.bytesTotal(), size);
  for (size_t i = 0; i < size; i++) {
    EXPECT_FALSE(it.eob());
    EXPECT_TRUE(it.getChar() == test_string[i]);
    it.next();
  }
  EXPECT_TRUE(it.eob());

  // Iterator peeks only, does not consume
  EXPECT_EQ(buf.readSize(), size);
  buf.consume(size);

  it = buf.begin();
  EXPECT_TRUE(it.eob());
}

TEST(SingleChunk, ByteCount) {
  Buffer buf;

  // nothing to read
  Buffer::Iterator it = buf.begin();
  EXPECT_EQ(it.bytesRead(), 0);
  it.next();
  EXPECT_EQ(it.bytesRead(), 0);

  // read the very begining of the buffer
  buf.write("XYZ");
  Buffer::Iterator another_it = buf.begin();
  EXPECT_EQ(another_it.bytesRead(), 0);
  another_it.next();
  EXPECT_EQ(another_it.bytesRead(), 1);

  // read partially consummed buffer
  buf.consume(1);
  Buffer::Iterator yet_another_it= buf.begin();
  EXPECT_EQ(yet_another_it.bytesRead(), 0);
  yet_another_it.next();
  EXPECT_EQ(yet_another_it.bytesRead(), 1);
}

TEST(MultiChunk, PerfectFit) {
  Buffer buf;
  const string test_string(Buffer::BlockSize, 'X');

  buf.write(test_string.c_str());
  EXPECT_EQ(buf.numChunks(), 2);
  EXPECT_EQ(buf.readSize(), Buffer::BlockSize);
  string read_string(buf.readPtr(), buf.readSize());
  EXPECT_EQ(test_string, read_string);
  buf.consume(Buffer::BlockSize);
  EXPECT_EQ(buf.readSize(), 0);
}

TEST(MultiChunk, ReadWriteCompact) {
  Buffer buf;
  size_t size = Buffer::BlockSize / 2 - 10 /* avoid exact fit */;
  const string test_string(size, 'X');
  const string alt_string(size, 'Y');

  // write first chunk
  buf.write(test_string.c_str());
  buf.write(alt_string.c_str());
  EXPECT_EQ(buf.numChunks(), 1);
  EXPECT_EQ(buf.byteCount(), 2*size);
  EXPECT_EQ(buf.writeSize(), Buffer::BlockSize - 2*size);

  // write second chunk's prefix
  buf.write(test_string.c_str()); // first 20 chars will be in first chunk
  EXPECT_EQ(buf.numChunks(), 2);
  EXPECT_EQ(buf.byteCount(), 3*size);
  EXPECT_EQ(buf.writeSize(), 2*Buffer::BlockSize - 3*size);

  // read first chunk
  size_t to_read = buf.readSize();
  EXPECT_EQ(to_read, Buffer::BlockSize);
  string read_string(buf.readPtr(), to_read);
  EXPECT_EQ(read_string, test_string + alt_string + string(20, 'X'));

  // skip to next chunk
  buf.consume(to_read);
  EXPECT_EQ(buf.byteCount(), 3*size - Buffer::BlockSize);

  // read second chunk
  to_read = buf.readSize();
  EXPECT_EQ(to_read, size - 20);
  string another_string(buf.readPtr(), to_read);
  EXPECT_EQ(another_string, string(size - 20, 'X'));
  buf.consume(to_read);

  EXPECT_EQ(buf.readSize(), 0);
}

TEST(MultiChunk, ReadWriteHoles) {
  Buffer buf;
  const string test_string = "TEST";
  const size_t size = test_string.size();

  // leave a hole in between two writes
  buf.write(test_string.c_str());
  EXPECT_TRUE(buf.reserve(Buffer::BlockSize)); // overallocated
  EXPECT_EQ(buf.numChunks(), 2);
  buf.write(test_string.c_str());
  EXPECT_EQ(buf.byteCount(), 2*size);

  // read strings
  for (int i = 0; i < 2; i++) {
    size_t to_read = buf.readSize();
    EXPECT_EQ(to_read, size);
    string read_string(buf.readPtr(), to_read);
    EXPECT_EQ(read_string, test_string);
    buf.consume(to_read);
    EXPECT_EQ(buf.byteCount(), size*(1-i));
  }

  EXPECT_EQ(buf.readSize(), 0);
}

TEST(MultiChunk, IterateCompactChunks) {
  Buffer buf;
  size_t size = Buffer::BlockSize / 2 - 10 /* leave chunk tail empty */;
  const string test_string(size, 'X');
  const string alt_string(size, 'Y');

  buf.write(test_string.c_str());
  buf.write(alt_string.c_str());
  buf.write(test_string.c_str());

  size_t count = 0;
  Buffer::Iterator it = buf.begin();
  while (!it.eob()) {
    char c = it.getChar();
    if ((count <= size-1) || (count >= 2*size)) {
      EXPECT_EQ(c, 'X');
    } else {
      EXPECT_EQ(c, 'Y');
    }
    count++;
    it.next();
  }
  buf.consume(count);
  EXPECT_EQ(buf.numChunks(), 1);
  EXPECT_TRUE(buf.begin() == buf.end());
}

TEST(MultiChunk, IterateChunksWithHoles) {
  Buffer buf;
  const string test_string = "TEST";
  const size_t size = test_string.size();

  // leave a hole in between two writes
  buf.write(test_string.c_str());
  EXPECT_TRUE(buf.reserve(Buffer::BlockSize)); // overallocated
  buf.write(test_string.c_str());

  size_t count = 0;
  Buffer::Iterator it = buf.begin();
  while (!it.eob()) {
    char c = it.getChar();
    EXPECT_EQ(c, test_string[count % size]);
    count++;
    it.next();
  }
  buf.consume(count);
  EXPECT_TRUE(buf.begin() == buf.end());
}

TEST(MultiChunk, ReverveAfterReading) {
  Buffer buf;
  buf.write("X");
  buf.consume(1);
  Buffer::Iterator it_before = buf.begin();
  EXPECT_TRUE(it_before.eob());

  EXPECT_TRUE(buf.reserve(Buffer::BlockSize));
  EXPECT_EQ(buf.byteCount(), 0);
  EXPECT_EQ(buf.numChunks(), 1);
  EXPECT_EQ(buf.writeSize(), Buffer::BlockSize);

  Buffer::Iterator it_after = buf.begin();
  EXPECT_TRUE(it_after.eob());
}

// Used to catch an error where byteCount() isn't returning correct values
TEST(MultiChunk, CheckByteCount) {
  Buffer buf;
  buf.write(string(Buffer::BlockSize / 2, 'A').c_str());
  buf.consume(Buffer::BlockSize / 2);
  EXPECT_EQ(buf.byteCount(), 0);
  buf.reserve(Buffer::BlockSize - 1); // goes to next chunk
  EXPECT_EQ(buf.byteCount(), 0);
  buf.write(string(100, 'A').c_str());
  EXPECT_EQ(buf.byteCount(), 100);
}

TEST(AppendChunk, EmptyBuffer) {
  Buffer buf1, buf2;

  // empty from empty
  buf1.appendFrom(&buf2);
  EXPECT_EQ(buf1.numChunks(), 1);
  EXPECT_EQ(buf2.numChunks(), 1);
  EXPECT_EQ(buf1.byteCount(), 0);
  EXPECT_EQ(buf2.byteCount(), 0);

  // non-empty from empty
  buf1.write("X");
  buf1.appendFrom(&buf2);
  EXPECT_EQ(buf1.numChunks(), 1);
  EXPECT_EQ(buf2.numChunks(), 1);
  EXPECT_EQ(buf1.byteCount(), 1);
  EXPECT_EQ(buf2.byteCount(), 0);
  string read_string(buf1.readPtr(), buf1.readSize());
  EXPECT_EQ(read_string, "X");
  buf1.consume(1);
  EXPECT_EQ(buf1.byteCount(), 0);

  // empty from non-empty
  Buffer buf3;
  buf2.write("Y");
  buf3.appendFrom(&buf2);
  EXPECT_EQ(buf2.numChunks(), 1);
  EXPECT_EQ(buf3.numChunks(), 1);
  EXPECT_EQ(buf2.byteCount(), 0);
  EXPECT_EQ(buf3.byteCount(), 1);
  string another_string(buf3.readPtr(), buf3.readSize());
  EXPECT_EQ(another_string, "Y");
  buf3.consume(1);
  EXPECT_EQ(buf3.byteCount(), 0);
}

TEST(AppendChunk, FilledBuffers) {
  Buffer buf1, buf2;

  buf1.write("X");
  buf1.reserve(Buffer::BlockSize);
  buf1.write("Y");
  buf2.write("Z");

  buf1.appendFrom(&buf2);
  string read_string;
  for (Buffer::Iterator it = buf1.begin(); it != buf1.end(); it.next()) {
    read_string.push_back(it.getChar());
  }
  EXPECT_EQ(read_string, "XYZ");
  EXPECT_EQ(buf1.numChunks(), 3);
  EXPECT_EQ(buf2.numChunks(), 1);
  EXPECT_EQ(buf1.byteCount(), 3);
  EXPECT_EQ(buf2.byteCount(), 0);

  buf1.consume(3);
  EXPECT_EQ(buf1.byteCount(), 0);
}

TEST(AppendChunk, ToReadBuffer) {
  Buffer buf1, buf2;

  buf1.write("X");
  string read_string(buf1.readPtr(), buf1.readSize());
  buf1.consume(read_string.size());

  buf2.write("Y");
  buf1.appendFrom(&buf2);
  string another_string(buf1.readPtr(), buf1.readSize());
  EXPECT_EQ(another_string, "Y");
}

TEST(CopyChunk, ToEmpty) {
  Buffer buf1, buf2;

  buf2.write("X");
  buf1.copyFrom(&buf2);

  string read_string(buf1.readPtr(), buf1.readSize());
  EXPECT_EQ(read_string, "X");
  EXPECT_EQ(buf1.numChunks(), 1);
  EXPECT_EQ(buf2.numChunks(), 1);
  EXPECT_EQ(buf1.byteCount(), 1);
  EXPECT_EQ(buf2.byteCount(), 1);
}

TEST(CopyChunk, ToReadBuffer) {
  Buffer buf1, buf2;

  buf1.write("X");
  string read_string(buf1.readPtr(), buf1.readSize());
  buf1.consume(read_string.size());

  buf2.write("Y");
  buf1.copyFrom(&buf2);
  string another_string(buf1.readPtr(), buf1.readSize());
  EXPECT_EQ(another_string, "Y");
}

TEST(CopyChunk, FillChunkOnCopy) {
  Buffer buf1, buf2;

  buf2.write("Y");
  string fill(Buffer::BlockSize-1, 'X');
  buf1.write(fill.c_str());
  buf1.copyFrom(&buf2);

  string read_string(buf1.readPtr(), buf1.readSize());
  EXPECT_EQ(read_string, fill + "Y");
  EXPECT_EQ(buf1.numChunks(), 2);
  EXPECT_EQ(buf2.numChunks(), 1);
  EXPECT_EQ(buf1.byteCount(), Buffer::BlockSize);
  EXPECT_EQ(buf2.byteCount(), 1);
}

TEST(FailureCases, CantReserve) {
  Buffer buf;
  EXPECT_FALSE(buf.reserve(Buffer::BlockSize+1));
}

} // unnamed namespace

int main(int argc, char *argv[]) {
  return RUN_TESTS(argc, argv);
}
