#include <iostream>

#include "buffer.hpp"
#include "timer.hpp"

namespace {

using base::Timer;
using base::Buffer;

struct FixtureBuffer {
  Buffer buf;

  FixtureBuffer(int len) {
    for (int i=0; i<len; i++) {
      buf.write("x");
    }
  }
};

FixtureBuffer fixture_buffer1(1500);
FixtureBuffer fixture_buffer2(Buffer::BlockSize*2);

void SingleChunk() {
  Timer timer;
  timer.start();

  for (int i=0; i<100000; i++) {
    Buffer::Iterator it = fixture_buffer1.buf.begin();
    while (!it.eob()) {
      it.next();
    }
  }

  timer.end();
  std::cout << "Single Chunk:\t" << timer.elapsed() << std::endl;
}

void MultipleChunk() {
  Timer timer;
  timer.start();

  for (int i=0; i<100000; i++) {
    Buffer::Iterator it = fixture_buffer2.buf.begin();
    while (!it.eob()) {
      it.next();
    }
  }

  timer.end();
  std::cout << "Multiple Chunk:\t" << timer.elapsed() << std::endl;
}

}  // unnamed namespace

int main(int argc, char* argv[]) {
  SingleChunk();
  MultipleChunk();

  return 0;
}
