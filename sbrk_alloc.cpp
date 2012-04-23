#include <unistd.h>  // for sbrk, getpagesize, off_t

// static allocators
class SbrkSysAllocator {
public:
  SbrkSysAllocator() { }
  void* Alloc(size_t size, size_t *actual_size, size_t alignment);
};
static char sbrk_space[sizeof(SbrkSysAllocator)];

SbrkSysAllocator* sys_alloc = NULL;

void* SbrkSysAllocator::Alloc(size_t size, size_t *actual_size,
                              size_t alignment) {
  // sbrk will release memory if passed a negative number, so we do
  // a strict check here
  if (static_cast<ptrdiff_t>(size + alignment) < 0) return NULL;

  // This doesn't overflow because TCMalloc_SystemAlloc has already
  // tested for overflow at the alignment boundary.
  size = ((size + alignment - 1) / alignment) * alignment;

  // "actual_size" indicates that the bytes from the returned pointer
  // p up to and including (p + actual_size - 1) have been allocated.
  if (actual_size) {
    *actual_size = size;
  }

  // Check that we we're not asking for so much more memory that we'd
  // wrap around the end of the virtual address space.  (This seems
  // like something sbrk() should check for us, and indeed opensolaris
  // does, but glibc does not:
  //    http://src.opensolaris.org/source/xref/onnv/onnv-gate/usr/src/lib/libc/port/sys/sbrk.c?a=true
  //    http://sourceware.org/cgi-bin/cvsweb.cgi/~checkout~/libc/misc/sbrk.c?rev=1.1.2.1&content-type=text/plain&cvsroot=glibc
  // Without this check, sbrk may succeed when it ought to fail.)
  if (reinterpret_cast<intptr_t>(sbrk(0)) + size < size) {
    return NULL;
  }

  void* result = sbrk(size);
  if (result == reinterpret_cast<void*>(-1)) {
    return NULL;
  }

  // Is it aligned?
  uintptr_t ptr = reinterpret_cast<uintptr_t>(result);
  if ((ptr & (alignment-1)) == 0)  return result;

  // Try to get more memory for alignment
  size_t extra = alignment - (ptr & (alignment-1));
  void* r2 = sbrk(extra);
  if (reinterpret_cast<uintptr_t>(r2) == (ptr + size)) {
    // Contiguous with previous result
    return reinterpret_cast<void*>(ptr + extra);
  }

  // Give up and ask for "size + alignment - 1" bytes so
  // that we can find an aligned region within it.
  result = sbrk(size + alignment - 1);
  if (result == reinterpret_cast<void*>(-1)) {
    return NULL;
  }
  ptr = reinterpret_cast<uintptr_t>(result);
  if ((ptr & (alignment-1)) != 0) {
    ptr += alignment - (ptr & (alignment-1));
  }
  return reinterpret_cast<void*>(ptr);
#endif  // HAVE_SBRK
}


static bool system_alloc_inited = false;
void InitSystemAllocators(void) {
  SbrkSysAllocator *sbrk = new (sbrk_space) SbrkSysAllocator();
  sys_alloc = sbrk;
}

void* TCMalloc_SystemAlloc(size_t size, size_t *actual_size,
                           size_t alignment) {
  // Discard requests that overflow
  if (size + alignment < size) return NULL;

  SpinLockHolder lock_holder(&spinlock);

  if (!system_alloc_inited) {
    InitSystemAllocators();
    system_alloc_inited = true;
  }

  // Enforce minimum alignment
  if (alignment < sizeof(MemoryAligner)) alignment = sizeof(MemoryAligner);

  void* result = sys_alloc->Alloc(size, actual_size, alignment);
  if (result != NULL) {
    if (actual_size) {
      CheckAddressBits<kAddressBits>(
          reinterpret_cast<uintptr_t>(result) + *actual_size - 1);
    } else {
      CheckAddressBits<kAddressBits>(
          reinterpret_cast<uintptr_t>(result) + size - 1);
    }
  }
  return result;
}

void TCMalloc_SystemRelease(void* start, size_t length) {
#ifdef MADV_FREE
  if (FLAGS_malloc_devmem_start) {
    // It's not safe to use MADV_FREE/MADV_DONTNEED if we've been
    // mapping /dev/mem for heap memory.
    return;
  }
  if (pagesize == 0) pagesize = getpagesize();
  const size_t pagemask = pagesize - 1;

  size_t new_start = reinterpret_cast<size_t>(start);
  size_t end = new_start + length;
  size_t new_end = end;

  // Round up the starting address and round down the ending address
  // to be page aligned:
  new_start = (new_start + pagesize - 1) & ~pagemask;
  new_end = new_end & ~pagemask;

  ASSERT((new_start & pagemask) == 0);
  ASSERT((new_end & pagemask) == 0);
  ASSERT(new_start >= reinterpret_cast<size_t>(start));
  ASSERT(new_end <= end);

  if (new_end > new_start) {
    // Note -- ignoring most return codes, because if this fails it
    // doesn't matter...
    while (madvise(reinterpret_cast<char*>(new_start), new_end - new_start,
                   MADV_FREE) == -1 &&
           errno == EAGAIN) {
      // NOP
    }
  }
#endif
}
