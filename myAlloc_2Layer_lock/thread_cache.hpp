#ifndef THREAD_CACHE_HEADER_
#define THREAD_CACHE_HEADER_

#include "lock.hpp"

namespace myalloc {

#define NUMOFSIZECLASSES 65

using base::Mutex;

enum { ObjFree = 0, ObjAllocated = 1 };

// Header of an object. Used both when the object is allocated and freed
struct ObjHeader {     // Footer is the same structure
  int _flags;          // flags == ObjFree or flags = ObjAllocated
  size_t _objectSize;  // Size of the object. Used when allocated/freed
};

class Allocator;

class ThreadCache {
public:
  ThreadCache() : _heapSize(0), _initialized(0), _verbose(0) { }
  explicit ThreadCache(Allocator* pcentheap) : _cent_heap(pcentheap),
                                      _heapSize(0),
                                      _initialized(0),
                                      _verbose(0) { }
  ~ThreadCache() { }

  //Initializes the heap
  void initialize();
  void setCentralHeap(Allocator* pheap) { _cent_heap = pheap; }

  // Allocates an object 
  void* allocateObject(size_t size);
  // Frees an object
  void freeObject(void* ptr);
  // Gets memory from the OS
  void* getMemoryFromCentHeap(size_t size);

  // Returns the size of an object
  size_t objectSize(void* ptr);
  // At exit handler
  void atExitHandler();

  struct DualLnkNode {
    DualLnkNode* next_;
    DualLnkNode* prev_;
    DualLnkNode() : next_(NULL), prev_(NULL) { }
    DualLnkNode(DualLnkNode* nn, DualLnkNode* pp)
      : next_(nn), prev_(pp) { }
  };

  // Prints the heap size and other information about the allocator
  void print();
  void getHeadFootInfo(const DualLnkNode* node) const;
  // For debugging
  void checkFreeLsConsist(int index) const;  // check list freels_[index]
  void checkDualLnkList(int index) const;
  void checkALL() const;
  size_t sumFreeListSize() const;
  size_t getFreeNodeSize(const DualLnkNode* node) const;
  bool isInitialized() const { return _initialized; }

private:
  DualLnkNode* freels_[NUMOFSIZECLASSES];
  Allocator*   _cent_heap;     // Central shared heap (in 4k allocates)
  Mutex        _m;
  size_t       _heapSize;      // Size of the heap
  int          _initialized;   // True if heap has been initialized
  int          _verbose;       // Verbose mode

  // Insert to [pos] of the free-list
  bool insertFreeBlock(DualLnkNode* toinsert, int pos);
  DualLnkNode* rmFromFreeLs(int pos, size_t totsize);

  // Non-copyable, non-assignable
  ThreadCache(const ThreadCache&);
  ThreadCache& operator=(const ThreadCache&);
};

}  // namespace myalloc

#endif  // End of THREAD_CACHE_HEADER_
