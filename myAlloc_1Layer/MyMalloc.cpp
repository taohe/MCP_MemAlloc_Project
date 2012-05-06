// The current implementation gets memory from the OS
// every time memory is requested and never frees memory.
// 05/05/2012 12:00 PM -- calloc has been removed to avoid pthread_create
// calling, but without freeing
//
#include <cassert>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "MyMalloc.hpp"

Allocator Allocator::TheAllocator;

extern "C" void atExitHandlerInC() {
  Allocator::TheAllocator.atExitHandler();
}
// void atExitHandlerInC() {
//   Allocator::TheAllocator.atExitHandler();
// }

void Allocator::initialize() {
  // Environment var VERBOSE prints stats at end and turns on debugging
  // Default is on
  _verbose = 1;
  const char * envverbose = getenv("MALLOCVERBOSE");
  if (envverbose && !strcmp( envverbose, "NO")) {
    _verbose = 0;
  }

  // In verbose mode register also printing statistics at exit
  atexit(atExitHandlerInC);
  for (int i = 0; i < NUMOFSIZECLASSES; ++i)
    freels_[i] = NULL;

  // _initialized = 1;  // Already set by CAS instruction
}

void* Allocator::allocateObject(size_t size) {
  //Make sure that allocator is initialized
  if (!_initialized) {
    if (__sync_bool_compare_and_swap(&_initialized, 0, 1))
      initialize();
  }

  // Add the ObjHeader and Footer to the size and round the total size
  // up to a multiple of 8 bytes for alignment.
  size_t totalSize = (size + (sizeof(ObjHeader) << 1) + 7) & ~7;
  // Min = 48, has to have space to fill in "DualLnkNode"
  totalSize = (totalSize < 48)? 48 : totalSize;

  // You should get memory from the OS only if the memory in the free
  // list could not satisfy the request.
  void* mem;
  size_t index = (totalSize / 8 > NUMOFSIZECLASSES -1)?
    (NUMOFSIZECLASSES - 1) : (totalSize / 8);

  // Lock the shared doubly-linked-list-of-lists heap:
  _m.lock();

  if (freels_[index++] != NULL) {
    // if 0 <= index <= 63, "mem" won't be NULL, since
    // freels_[index] != NULL
    mem = static_cast<void*>(rmFromFreeLs(totalSize / 8, totalSize));
    if (mem != NULL) {  // suitable size free node found
      mem = (void*)((unsigned char*)mem - sizeof(ObjHeader));
      // If the actual size for 'mem' is larger than totalSize, reassign
      if (((ObjHeader*)mem)->_objectSize > totalSize) {
        totalSize = ((ObjHeader*)mem)->_objectSize;
      }
    } else {  // requesting > 512 bytes, not suitable free node exist
      mem = getMemoryFromOS(totalSize);
    }
  } else {  // Search for larger free-lists in "freels_[]"
    while (index < NUMOFSIZECLASSES) {  // search for larger slots
      if (freels_[index] != NULL)
        break;
      ++index;
    }
    if (index == NUMOFSIZECLASSES)
      mem = getMemoryFromOS(totalSize);
    else {  // Split larger free slot
      DualLnkNode* toSplit = rmFromFreeLs(index, index * 8);
      size_t realSize = ((ObjHeader*)((unsigned char*)toSplit -
            sizeof(ObjHeader)))->_objectSize;
      if (realSize >= (totalSize + sizeof(DualLnkNode) +
            2 * sizeof(ObjHeader))) {
        size_t newclass = (realSize - totalSize) / 8;
        // Set header and footer for new splitted object
        ObjHeader* splitobj = (ObjHeader*)((unsigned char*)toSplit +
            totalSize - sizeof(ObjHeader));
        splitobj->_objectSize = realSize - totalSize;  // may > sizeclass
        splitobj->_flags = ObjFree;
        splitobj = (ObjHeader*)((unsigned char*)toSplit + realSize
          - 2 * sizeof(ObjHeader));  // Now, pointing to footer
        splitobj->_objectSize = realSize - totalSize;  // may > sizeclass
        splitobj->_flags = ObjFree;
        assert(insertFreeBlock((DualLnkNode*)((unsigned char*)toSplit +
          totalSize), newclass));
        mem = (void*)((unsigned char*)toSplit - sizeof(ObjHeader));
      } else {  // Cannot split
        totalSize = realSize;  // Gave a larger free-node back
        mem = (void*)((unsigned char*)toSplit - sizeof(ObjHeader));
      }
    }
  }
  _m.unlock();

  // Get a pointer to the object header ????? didn't change footer
  ObjHeader* obj = static_cast<ObjHeader*>(mem);
  // Store the totalSize. We will need it in realloc() and in free()
  obj->_objectSize = totalSize;
  // Set object as allocated
  obj->_flags = ObjAllocated;
  // "obj" now points to the Footer, set footer values
  obj = (ObjHeader*)((unsigned char*)obj + totalSize - sizeof(ObjHeader));
  obj->_objectSize = totalSize;
  obj->_flags = ObjAllocated;
  // "obj" repoints to the header
  obj = (ObjHeader*)((unsigned char*)obj - totalSize + sizeof(ObjHeader));

  // Return the pointer after the object header.
  return static_cast<void*>(obj + 1);
}

void Allocator::freeObject(void* ptr) {
  // Here you will return the object to the free list sorted by address
  // and you will coalesce it if possible.
  ObjHeader* obj = reinterpret_cast<ObjHeader*>((unsigned char*)ptr -
      sizeof(ObjHeader));
  size_t totalSize = obj->_objectSize;

  _m.lock();
  // No space to put it into free-list (min: 48 bytes)
  if (totalSize < (sizeof(DualLnkNode) + 2 * sizeof(ObjHeader))) { 
    _m.unlock();
    puts("Free without gettting back-------------");
    return;
  } else {
    obj->_flags = ObjFree;
    // "obj" now points to the Footer, set footer values
    obj = (ObjHeader*)((unsigned char*)obj+totalSize - sizeof(ObjHeader));
    obj->_flags = ObjFree;  // Set footer flag to freed
    // "obj" still points to the footer now
    assert(insertFreeBlock((DualLnkNode*)ptr, (totalSize / 8)));
    _m.unlock();
  }
}

size_t Allocator::objectSize(void* ptr) {
  // Return the size of the object pointed by ptr. We assume that ptr
  // is a valid obejct.
  ObjHeader* obj =
    reinterpret_cast<ObjHeader*>((char*)ptr - sizeof(ObjHeader));

  // Substract the size of the header and footer
  return obj->_objectSize - (sizeof(ObjHeader) << 1);
}

void Allocator::print() {
  printf("-------------------\n");
  printf("# mallocs:\t%d\n", _mallocCalls );
  printf("# reallocs:\t%d\n", _reallocCalls );
  printf("# callocs:\t%d\n", _callocCalls );
  printf("# frees:\t%d\n", _freeCalls );
  size_t sumfreelssize = sumFreeListSize();
  printf("HeapSize: %10lu  sumFreeLsSize: %10lu   (Equal? %c)\n",
      _heapSize, sumfreelssize, ((_heapSize == sumfreelssize)? 'Y':'N'));

  printf("-------------------\n");
}

void* Allocator::getMemoryFromOS(size_t size) {
  // Use sbrk() to get memory from OS
  _heapSize += size;
  
  return sbrk(size);
}

void Allocator::atExitHandler() {
  // Print statistics when exit
  if (_verbose) {
    print();
    checkALL();
  }
}

// Free-list manipulation methods
bool Allocator::insertFreeBlock(DualLnkNode* toinsert, int pos) {
  // Check boundary
  if (pos < 0)
    return false;
  if (pos > NUMOFSIZECLASSES - 1)
    pos = NUMOFSIZECLASSES - 1;

  if ((pos == NUMOFSIZECLASSES - 1) && (freels_[pos] != NULL)) {
    // Special case, >= 512 bytes, put them in non-decreasing order
    size_t toinsertSize = ((ObjHeader*)((unsigned char*)toinsert -
        sizeof(ObjHeader)))->_objectSize;
    DualLnkNode* iter = freels_[pos];
    DualLnkNode* preiter = NULL;
    while (iter != NULL) {
      if (((ObjHeader*)((unsigned char*)iter -
          sizeof(ObjHeader)))->_objectSize >= toinsertSize)
        break;
      preiter = iter;
      iter = iter->next_;
    }
    // Insert to the left of iter
    if (iter == NULL) {  // Insert to the end of the this list
      preiter->next_ = toinsert;
      toinsert->prev_ = preiter;
      toinsert->next_ = NULL;
    } else if (preiter == NULL) {  // Insert into the front of the list
      freels_[pos] = toinsert;
      toinsert->next_ = iter;
      toinsert->prev_ = NULL;
      iter->prev_ = toinsert;
    } else {  // Insert between preiter and iter, both not-null
      preiter->next_ = toinsert;
      toinsert->next_ = iter;
      iter->prev_ = toinsert;
      toinsert->prev_ = preiter;
    }
    return true;
  } else {  // Insert at the front of the double-linked list
    DualLnkNode* tmpnext = freels_[pos];
    toinsert->next_ = tmpnext;
    if (tmpnext)
      tmpnext->prev_ = toinsert;
    toinsert->prev_ = NULL;
    freels_[pos] = toinsert;

    return true;
  }
}

Allocator::DualLnkNode* Allocator::rmFromFreeLs(int pos, size_t totsize) {
  // Check boundary
  if (pos < 0)
    return NULL;
  if (pos > NUMOFSIZECLASSES - 1)
    pos = NUMOFSIZECLASSES - 1;
  if (freels_[pos] == NULL)
    return NULL;

  if (pos == NUMOFSIZECLASSES - 1) {
    DualLnkNode* iter = freels_[pos];  // Must exist
    // Special case, >= 512 bytes, list is in non-decreasing order
    while (iter != NULL) {
      if (((ObjHeader*)((unsigned char*)iter -
          sizeof(ObjHeader)))->_objectSize >= totsize)
        break;
      iter = iter->next_;
    }
    if (iter == NULL)
      return NULL;  // Didn't find suitable size
    else {  // Remove iter from this doulbe linked list
      if (iter == freels_[pos]) {  // Free first node
        freels_[pos] = iter->next_;
        if (iter->next_)
          iter->next_->prev_ = NULL;
        return iter;  // Didn't do splitting, ---- internal fragmentation
      } else {
        iter->prev_->next_ = iter->next_;
        if (iter->next_)
          iter->next_->prev_ = iter->prev_;
        return iter;
      }
    }
  } else {
    DualLnkNode* firstNode = freels_[pos];  // Must exist
    freels_[pos] = firstNode->next_;
    if (firstNode->next_)
      firstNode->next_->prev_ = NULL;

    return firstNode;
  }
}

// print header / footer info for a given free-list node pointer
void Allocator::getHeadFootInfo(const DualLnkNode* node) const {
  ObjHeader* obj = (ObjHeader*)((unsigned char*)node - sizeof(ObjHeader));
  size_t objsize = obj->_objectSize;
  printf("Header: h_size = %lu, h_flag = %d\n", objsize, obj->_flags);
  // Now "obj" points to the footer of this node
  obj = (ObjHeader*)((unsigned char*)node + objsize-2*sizeof(ObjHeader));
  printf("Footer: f_size = %lu, f_flag = %d\n", obj->_objectSize,
      obj->_flags);
}

void Allocator::checkFreeLsConsist(int index) const {
  assert(index < NUMOFSIZECLASSES);

  size_t classsize = index * 8, totsize = 0;
  DualLnkNode* iter = freels_[index];
  ObjHeader* head, *foot;

  while (iter != NULL) {
    head = (ObjHeader*)((unsigned char*)iter - sizeof(ObjHeader));
    assert(head->_flags == ObjFree);
    totsize = head->_objectSize;
    if (index <= NUMOFSIZECLASSES - 2) {
      assert(totsize == classsize);
    } else {
      assert(totsize >= classsize);
    }
    foot = (ObjHeader*)((unsigned char*)iter + totsize
        - 2 * sizeof(ObjHeader));
    assert(foot->_flags == ObjFree);
    assert(foot->_objectSize == totsize);

    iter = iter->next_;
  }
}

void Allocator::checkDualLnkList(int index) const {
  assert(index < NUMOFSIZECLASSES);
  DualLnkNode* iter = freels_[index], *preiter = NULL;

  while (iter != NULL) {
    assert(iter->prev_ == preiter);
    preiter = iter;
    iter = iter->next_;
  }
}

void Allocator::checkALL() const {
  for (int i = 0; i < NUMOFSIZECLASSES; ++i) {
    checkFreeLsConsist(i);
    checkDualLnkList(i);
  }
}

size_t Allocator::sumFreeListSize() const {
  size_t sumsize = 0;
  DualLnkNode* iter;
  for (int i = 0; i < NUMOFSIZECLASSES; ++i) {
    iter = freels_[i];
    while (iter != NULL) {
      sumsize += getFreeNodeSize(iter);
      iter = iter->next_;
    }
  }
  return sumsize;
}

size_t Allocator::getFreeNodeSize(const DualLnkNode* node) const {
  return ((ObjHeader*)((unsigned char*)node - sizeof(ObjHeader)))->
    _objectSize;
}

// --------------
// C interface
//

extern "C" void* malloc(size_t size) {
  assert(size > 0);
  void* ptr = Allocator::TheAllocator.allocateObject(size);
  Allocator::TheAllocator.increaseMallocCalls();
  return ptr;
}

extern "C" void free(void* ptr) {
  if (ptr == NULL) {
    return;  // No object to free
  }

  Allocator::TheAllocator.increaseFreeCalls();
  Allocator::TheAllocator.freeObject(ptr);
}

extern "C" void* realloc(void *ptr, size_t size) {
  Allocator::TheAllocator.increaseReallocCalls();

  // Allocate new object
  void * newptr = Allocator::TheAllocator.allocateObject(size);

  // Copy old object only if ptr != 0
  if (ptr != 0) {
    // copy only the minimum number of bytes
    size_t sizeToCopy =  Allocator::TheAllocator.objectSize(ptr);
    if (sizeToCopy > size) {
      sizeToCopy = size;
    }
    
    memcpy( newptr, ptr, sizeToCopy );

    //Free old object
    Allocator::TheAllocator.freeObject( ptr );
  }

  return newptr;
}

// extern "C" void* calloc(size_t nelem, size_t elsize) {
//   Allocator::TheAllocator.increaseCallocCalls();
//   // calloc allocates and initializes
//   size_t size = nelem * elsize;
// 
//   void* ptr = Allocator::TheAllocator.allocateObject(size);
// 
//   if (ptr) {
//     // No error, Initialize chunk with 0s
//     memset(ptr, 0, size);
//   }
// 
//   return ptr;
// }

extern "C" void checkHeap() {
  // Verifies the heap consistency by iterating over all objects
  // in the free lists and checking that the next, previous pointers
  // size, and boundary tags make sense.
  // The checks are done by calling assert( expr ), where "expr"
  // is a condition that should be always true for an object.
  //
  // assert will print the file and line number and abort
  // if the expression "expr" is false.
}
