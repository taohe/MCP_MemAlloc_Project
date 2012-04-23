#ifndef MCP_BASE_MEM_HEADER
#define MCP_BASE_MEM_HEADER

#include <cstring>

#include <string>

namespace base {

// MemPiece is a convenience class that converts automatically from a
// null-terminated char* or a string, and can also be initialized with
// a piece of memory.
//
// This allows a single method to take as argument a string or a
// null-terminated char* (which are converted automatically) without
// needing separate overloaded methods, one for each type.
//
class MemPiece {
public:
  MemPiece(const char* ptr, int len) : ptr_(ptr), length_(len) {}
  MemPiece(const char* ptr) : ptr_(ptr), length_(strlen(ptr)) {}
  MemPiece(const std::string& s) : ptr_(s.data()), length_(s.size()) {}

  size_t len() const { return length_; }
  const char *ptr() const { return ptr_; }

private:
  const char* ptr_;
  size_t      length_;

  // Non-copyable, non-assignable
  MemPiece(const MemPiece&);
  MemPiece& operator=(const MemPiece&);
};

} // namespace base

#endif // MCP_BASE_MEM_HEADER
