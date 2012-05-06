#ifndef MYMALLOC_HEADER_
#define MYMALLOC_HEADER_

#include "lock.hpp"

#define NUMOFSIZECLASSES 65

using base::Mutex;

enum { ObjFree = 0, ObjAllocated = 1 };

// Header of an object. Used both when the object is allocated and freed
struct ObjHeader {     // Footer is the same structure
  int _flags;          // flags == ObjFree or flags = ObjAllocated
  size_t _objectSize;  // Size of the object. Used when allocated/freed
};

class Allocator {
public:
  // This is the only instance of the allocator.
  static Allocator TheAllocator;
  Allocator() : _heapSize(0), _initialized(0), _verbose(0),
                _mallocCalls(0), _freeCalls(0), _reallocCalls(0),
                _callocCalls(0) { }
  ~Allocator() { }

  //Initializes the heap
  void initialize();

  // Allocates an object 
  void * allocateObject( size_t size );

  // Frees an object
  void freeObject( void * ptr );

  // Returns the size of an object
  size_t objectSize( void * ptr );

  // At exit handler
  void atExitHandler();

  // Gets memory from the OS
  void * getMemoryFromOS( size_t size );
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
  DualLnkNode* freels_[NUMOFSIZECLASSES];
  Mutex        _m;
  size_t       _heapSize;      // Size of the heap
  int          _initialized;   // True if heap has been initialized
  int          _verbose;       // Verbose mode
  int          _mallocCalls;   // # malloc calls
  int          _freeCalls;     // # free calls
  int          _reallocCalls;  // # realloc calls
  int          _callocCalls;   // # realloc calls

  // Insert to [pos] of the free-list
  bool insertFreeBlock(DualLnkNode* toinsert, int pos);
  DualLnkNode* rmFromFreeLs(int pos, size_t totsize);

  // Non-copyable, non-assignable
  //Allocator(Allocator&);
  Allocator& operator=(Allocator&);
};

#endif
