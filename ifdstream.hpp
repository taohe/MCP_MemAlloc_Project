#ifndef MCP_BASE_IFD_STREAM_HEADER
#define MCP_BASE_IFD_STREAM_HEADER

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <istream>
#include <streambuf>

namespace std {

// The fdistream is a stream that reads on a file descriptor.
//
// (C) Copyright Nicolai M. Josuttis 2001.
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
class fdinbuf : public std::streambuf {
protected:
  int fd;    // file descriptor
protected:
  /* data buffer:
   * - at most, pbSize characters in putback area plus
   * - at most, bufSize characters in ordinary read buffer
   */
  static const int pbSize = 4;        // size of putback area
  static const int bufSize = 1024;    // size of the data buffer
  char buffer[bufSize+pbSize];        // data buffer

public:
  /* constructor
   * - initialize file descriptor
   * - initialize empty data buffer
   * - no putback area
   * => force underflow()
   */
  fdinbuf (int _fd) : fd(_fd) {
    setg (buffer+pbSize,     // beginning of putback area
          buffer+pbSize,     // read position
          buffer+pbSize);    // end position
  }

protected:
  // insert new characters into the buffer
  virtual int_type underflow () {
#ifndef _MSC_VER
    using std::memmove;
#endif

    // is read position before end of buffer?
    if (gptr() < egptr()) {
      return traits_type::to_int_type(*gptr());
    }

    /* process size of putback area
     * - use number of characters read
     * - but at most size of putback area
     */
    int numPutback;
    numPutback = gptr() - eback();
    if (numPutback > pbSize) {
      numPutback = pbSize;
    }

    /* copy up to pbSize characters previously read into
     * the putback area
     */
    memmove (buffer+(pbSize-numPutback), gptr()-numPutback,
             numPutback);

    // read at most bufSize new characters
    int num;
    num = read (fd, buffer+pbSize, bufSize);
    if (num <= 0) {
      // ERROR or EOF
      return EOF;
    }

    // reset buffer pointers
    setg (buffer+(pbSize-numPutback),   // beginning of putback area
          buffer+pbSize,                // read position
          buffer+pbSize+num);           // end of buffer

    // return next character
    return traits_type::to_int_type(*gptr());
  }
};

class fdistream : public std::istream {
protected:
  fdinbuf buf;
public:
  fdistream (int fd) : std::istream(0), buf(fd) {
    rdbuf(&buf);
  }
};

} // namespace std

#endif // MCP_BASED_IFD_STREAM_HEADER
