#ifndef MCP_CIRCULAR_BUFFER_HEADER
#define MCP_CIRCULAR_BUFFER_HEADER

namespace base {

// This is a fixed-size, circular buffer of integer.
//
// The class is not thread-safe,
//
class CircularBuffer {
public:

  // Creates a buffer with 'slots' slots.
  explicit CircularBuffer(int slots);

  // Destructor.
  ~CircularBuffer();

  // Writes 'value' to the next available slot. It may overwrite
  // values that were not yet read out of the buffer.
  void write(int value);

  // Returns the next value available for reading, in the order they
  // were written, and marks slot as read.
  int read();

  // Removes all the elements from the buffer.
  void clear();

private:
  //
  // ADD PRIVATE STATE HERE
  //

  // Non-copyable, non-assignable.
  CircularBuffer(CircularBuffer&);
  CircularBuffer& operator=(const CircularBuffer&);
};

} // namespace base

#endif  // MCP_CIRCULAR_BUFFER_HEADER
