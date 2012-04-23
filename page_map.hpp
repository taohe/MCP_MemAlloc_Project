#ifndef MCP_PAGEMAP_HEADER
#define MCP_PAGEMAP_HEADER

#include <inttypes.h>  // For uintptr_t

// A three-level radix tree for page-mapping
// used for 64-bit virtual address space, 4k pages
//
#define BITS 52  // 64 - 12

template <int BITS>
class TCMalloc_PageMap {
private:
  // static const int LENGTH = 1 << BITS;

  // void** array_;

public:
  typedef uintptr_t Number;

  explicit TCMalloc_PageMap(void* (*allocator)(size_t));

  // Ensure that the map contains initialized entries "x .. x+n-1".
  // Returns true if successful, false if we could not allocate memory.
  bool Ensure(Number x, size_t n);

  void PreallocateMoreMemory();

  // Return the current value for KEY.  Returns NULL if not yet set,
  // or if k is out of range.
  void* get(Number k) const;

  // REQUIRES "k" is in range "[0,2^BITS-1]".
  // REQUIRES "k" has been ensured before.
  //
  // Sets the value 'v' for key 'k'.
  void set(Number k, void* v);

  // Return the first non-NULL pointer found in this map for
  // a page number >= k.  Returns NULL if no such number is found.
  void* Next(Number k) const;
};

#endif  // MCP_PAGEMAP_HEADER
