#ifndef MCP_BASE_FILE_CACHE_HEADER
#define MCP_BASE_FILE_CACHE_HEADER

#include <string>
#include <tr1/unordered_map>

#include "buffer.hpp"
#include "lock.hpp"

namespace base {

using std::string;
using std::tr1::hash;
using std::tr1::unordered_map;

// The FileCache maintains a map from file names to their contents,
// stored as 'Buffer's. The sum of all Buffer's stored in the map
// never exceeds the size determined for the cache (at construction
// time).
//
// A request to pin a file that is already in the cache is supposed to
// be very fast. We do so by leveraging reader-writer locks. A cache
// hit needs only to grab a read lock on the map -- it doesn't change
// it -- and to increment the pin count for that file -- which can be
// done with an atomic fetch-and-add.
//
// A cache miss is slower, but we expect them to be less frequent. The
// buffers are connected together in a single-linked FIFO list. That's
// right, for the lab 2 there is no special eviction policy. When
// space is needed, we traverse the list and evict the first
// non-pinned buffer we find. And repeat until enough space was
// cleared.
//
// If not enough unpinned space is found to fit a new request, then
// the pin request may fail.
//
// Thread safety:
//   + pin() and unpin() can be done from different threads
//   + ~FileCache is NOT thread-safe. The caller has to be sure there
//     are no ongoing cache readers before disposing of the cache
//
// Usage:
//   FileCache my_cache(50<<20 /* 50MB */);
//
//   Buffer* buf;
//   int error;
//   CacheHandle h = my_cache.pin("a_file.html", &buf, &error);
//   if (h != 0) {
//     can read contents of *buf
//   } else if (error == 0) {
//     no space on cache. will need to read the file on your own
//   } else /* error < 0 */ {
//     other error reading the file; error contains errno
//   }
//
class FileCache {
public:
  typedef const string* CacheHandle;

  explicit FileCache(int max_size);
  ~FileCache();

  CacheHandle pin(const string& file_name, Buffer** buf, int* error);
  void unpin(CacheHandle h);

  // accessors

  int maxSize() const   { return max_size_; }
  int bytesUsed() const { return bytes_used_; }
  int pins() const      { return pin_total_; }
  int hits() const      { return hit_total_; }
  int failed() const    { return failed_total_; }

private:
  struct Node {
    const string file_name;
    Buffer*      buf;             // owned here
    int          pin_count;
    int          size;
    Node*        next;

    Node(const string& f, Buffer* b)
      : file_name(f), buf(b), pin_count(0), size(0), next(NULL) { }

    ~Node() { delete buf; }
  };

  // A map from a file_name to its node. Because we want to save
  // space, we use the file_name inside the Node as the key (as a
  // string*) rather than to duplicate that (as a string).
  struct HashStrPtr {
    size_t operator()(const string* a) const {
      hash<string> hasher;
      return hasher(*a);
    }
  };
  struct EqStrPtr {
    bool operator()(const string* a, const string* b) const {
      return *a == *b;
    }
  };
  typedef unordered_map<const string*, Node*, HashStrPtr, EqStrPtr> CacheMap;

  const int     max_size_;
  Node*         head_;
  Node          tail_;           // use a sentinel

  RWMutex       rw_m_;           // protects cache_map_
  CacheMap      cache_map_;      // look-up for file names

  // atomic counters
  int           bytes_used_;     // regardless if pinned or not
  int           pin_total_;      // # of requests to pin
  int           hit_total_;      // # of requests to pin that were hits
  int           failed_total_;   // # of requests to pint that failed

  // Helper for the actual file load
  CacheHandle load(const string& file_name, Buffer** buf, int* error);
  bool evict(size_t bytes_to_evict);

  // list manipulation
  void nodeInsert(Node* node);
  void nodeExtract(Node* node, Node* prev);

  // Non-copyable, non-assignable
  FileCache(FileCache&);
  FileCache& operator=(FileCache&);

};

} // namespace base

#endif // DISTRIB_BASE_FILE_CACHE_HEADER

