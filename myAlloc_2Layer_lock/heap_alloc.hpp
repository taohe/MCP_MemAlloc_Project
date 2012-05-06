#ifndef HEAP_ALLOC_HEADER_
#define HEAP_ALLOC_HEADER_

#include "lock.hpp"
#include "thread_cache.hpp"  // For ThreadCache heap

namespace myalloc {

#define NUMOFSIZECLASSES 65
#define BASICALLOCSIZE 4096   // Common Page size
// if <= this size,alloc from threadCache
#define CENTHEAPALLOCTHRESHOLD (1UL << 14)
#define NUMOFTHREADCACHES 17

using base::Mutex;

// This is the base allocator, It allocate/dealloc in Pages (4k)
// chunks
class Allocator {
public:
  // This is the only instance of the allocator.
  static Allocator TheAllocator;
  Allocator() : _initialized(0) { }
  ~Allocator() { }

  //Initializes the heap
  void initialize();

  // Allocates an object, return "Head of 'usable' space
  void* allocateObject(size_t size);
  // Frees an object
  void freeObject(void* ptr);
  // Gets memory from the OS
  void* getMemoryFromOS(size_t size);
  void* assignMalloc(size_t size);

  // At exit handler
  void atExitHandler();
  // Returns the size of an object
  size_t objectSize(void* ptr);
  ThreadCache* getThrCaches(int ind) {
    return (_thr_caches + ind);
  }

  void increaseMallocCalls() {
    __sync_add_and_fetch(&_mallocCalls, 0x1);
  }
  void increaseReallocCalls() {
    __sync_add_and_fetch(&_reallocCalls, 0x1);
  }
  void increaseCallocCalls() {
    __sync_add_and_fetch(&_callocCalls, 0x1);
  }
  void increaseFreeCalls() {
    __sync_add_and_fetch(&_freeCalls, 0x1);
  }

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

private:
  DualLnkNode*        freels_[NUMOFSIZECLASSES];
  ThreadCache         _thr_caches[NUMOFTHREADCACHES];
  Mutex               _m;
  size_t              _heapSize;      // Size of the heap
  int                 _initialized;   // True if heap has been initialized
  int                 _verbose;       // Verbose mode
  int                 _mallocCalls;   // # malloc calls
  int                 _freeCalls;     // # free calls
  int                 _reallocCalls;  // # realloc calls
  int                 _callocCalls;   // # realloc calls

  // Insert to [pos] of the free-list
  bool insertFreeBlock(DualLnkNode* toinsert, int pos);
  DualLnkNode* rmFromFreeLs(int pos, size_t totsize);

  // Non-copyable, non-assignable
  Allocator(const Allocator&);
  Allocator& operator=(const Allocator&);
};

}  // namespace myalloc

#endif  // HEAP_ALLOC_HEADER_
