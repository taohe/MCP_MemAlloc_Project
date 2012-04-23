#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <list>

#include "file_cache.hpp"
#include "logging.hpp"

namespace base {

using std::make_pair;
using std::list;

FileCache::FileCache(int max_size)
  : max_size_(max_size),
    head_(&tail_),
    tail_("", NULL),
    bytes_used_(0),
    pin_total_(0),
    hit_total_(0),
    failed_total_(0) {
}

// REQUIRES: No ongoing pin. This code assumes no one is using the
// cache anymore
FileCache::~FileCache() {
  rw_m_.wLock();

  Node* curr = tail_.next;
  while (curr != NULL) {
    Node* to_delete = curr;
    curr = curr->next;
    delete to_delete;
  }

  rw_m_.unlock();
}

FileCache::CacheHandle FileCache::pin(const string& file_name,
                                      Buffer** buf,
                                      int* error) {
  CacheHandle h;
  int internal_error;
  if (error == NULL) {
    error = &internal_error;
  }

  h = 0;
  *error = 0;
  *buf = 0;

  // The short path: the file is loaded already.
  rw_m_.rLock();

  CacheMap::iterator it = cache_map_.find(&file_name);
  if (it != cache_map_.end()) {
    h = it->first;
    Node* node = it->second;
    *buf = node->buf;

    // We can write under this lock because it protects the cache_map
    // from changing but not the node itself changing. We also do it
    // here to avoid a race between this and the eviction code. That
    // race would go like this:
    // [reader1] Reader finds file F with pin 0 (here) and is about
    //           to increment it to 1
    // [reader2] Eviction is being done on F's buffer on behalf of
    //           another read (eviction code)
    // [reader1] Increments pin count of a node that doesn't exist
    //           anymore
    //
    // Since eviction needs a write-lock on the map, incrementing the
    // buffers pin count here would prevent it from being evicted.
    //

    __sync_fetch_and_add(&node->pin_count, 1);

    rw_m_.unlock();

    __sync_fetch_and_add(&pin_total_, 1);
    __sync_fetch_and_add(&hit_total_, 1);

    return h;
  }

  rw_m_.unlock();

  return this->load(file_name, buf, error);
}

FileCache::CacheHandle FileCache::load(const string& file_name,
                                       Buffer** buf,
                                       int* error) {
  // More than one thread may be loading a given file because we do
  // that without holding any locks. The alternative would have been
  // to write-lock the cache_map_ while loading. But that would mean to
  // have all readers wait while a single thread is bringing a file to
  // memory. The trade-off is that we may be reading a file that we
  // won't keep because another load brought it up concurrently.

  int fd = ::open(file_name.c_str(), O_RDONLY);
  if (fd < 0) {
    LOG(LogMessage::WARNING) << "could not open " << file_name
                             << ": " << strerror(errno);
    *error = errno;
    return 0;
  }

  // Check if there's space enough for the file and, if not, try to
  // make some.
  struct stat stat_buf;
  fstat(fd, &stat_buf);
  if (max_size_ - bytes_used_ < stat_buf.st_size) {
    if (! this->evict(stat_buf.st_size)) {
      // Not enough available or unpinned space.
      __sync_fetch_and_add(&pin_total_, 1);
      __sync_fetch_and_add(&failed_total_, 1);
      close(fd);

      *error = 0;
      return 0;
    }
  }

  // Read the file into a Buffer
  // TODO: How about creating a helper Buffer::newBuffer(file_name)
  // and moving this code out of here.
  Buffer* new_buf = new Buffer;
  size_t to_read = stat_buf.st_size;
  while (to_read > 0) {
    new_buf->reserve(Buffer::BlockSize);
    int bytes_read = read(fd, new_buf->writePtr(), new_buf->writeSize());
    if (bytes_read < 0 ) {
      std::cout << strerror(errno);
      LOG(LogMessage::ERROR) << "cant read file " << file_name
                             << ": " << strerror(errno);
      close(fd);
      delete new_buf;

      *error = errno;
      return 0;
    }
    to_read -= bytes_read;
  }
  if (to_read != 0) {
    LOG(LogMessage::WARNING) << "file change while reading " << file_name;
  }
  new_buf->advance(stat_buf.st_size - to_read);
  *buf = new_buf;

  Node* new_node = new Node(file_name, new_buf);
  new_node->pin_count = 1;
  new_node->size = stat_buf.st_size;

  rw_m_.wLock();

  // Hang the newly read file/Buffer into the cache map structure.
  CacheHandle h = 0;
  bool duplicate_load = false;
  std::pair<CacheMap::iterator, bool> it;
  it = cache_map_.insert(make_pair(&new_node->file_name, new_node));
  if (it.second) {
    this->nodeInsert(new_node);
    h = reinterpret_cast<CacheHandle>(&new_node->file_name);
  } else {
    // TODO: is it possible to increment the pin counter and behave as
    // this was a hit?
    duplicate_load = true;
  }

  rw_m_.unlock();

  __sync_fetch_and_add(&pin_total_, 1);
  if (duplicate_load) {
    __sync_fetch_and_add(&failed_total_, 1);
    *buf = 0;
    delete new_node;
  } else {
    __sync_fetch_and_add(&bytes_used_, stat_buf.st_size);
  }

  close(fd);
  return h;
}

bool FileCache::evict(size_t bytes_to_evict) {
  list<Node*> to_delete;

  rw_m_.wLock();

  if (head_ == &tail_) {
    rw_m_.unlock();
    return false;
  }

  int bytes_evicted = 0;
  Node* curr = tail_.next;
  Node* prev = &tail_;
  while ((bytes_to_evict > 0) && (curr != NULL)) {
    if (curr->pin_count == 0) {
      bytes_evicted += curr->size;
      bytes_to_evict -= curr->size;
      cache_map_.erase(&curr->file_name);
      to_delete.push_back(curr);
      nodeExtract(curr, prev);
    }

    prev = curr;
    curr = curr->next;
  }

  __sync_fetch_and_sub(&bytes_used_, bytes_evicted);

  rw_m_.unlock();

  while (to_delete.size() > 0) {
    delete to_delete.front();
    to_delete.pop_front();
  }

  return bytes_to_evict <= 0;
}

void FileCache::unpin(CacheHandle h) {
  if (h == 0) {
    return;
  }

  const string* file_name = reinterpret_cast<const string*>(h);

  rw_m_.rLock();

  CacheMap::iterator it = cache_map_.find(file_name);
  if (it == cache_map_.end()) {
    LOG(LogMessage::FATAL) << "Trying to unpin not cached " << *file_name;
    rw_m_.unlock();
    return;
  }

  Node* node = it->second;
  int count = __sync_fetch_and_sub(&node->pin_count, 1);
  if (count == 0) {
    LOG(LogMessage::FATAL) << "Trying to unpin not loaded " << *file_name;
    rw_m_.unlock();
    return;

  }

  rw_m_.unlock();
}

void FileCache::nodeInsert(Node* node) {
  head_->next = node;
  head_ = node;
  node->next = NULL;
}

void FileCache::nodeExtract(Node* node, Node* prev) {
  if ((node == prev) || (prev == NULL)) {
    LOG(LogMessage::FATAL) << "Can't delete without previous node ";
    return;
  }

  if (head_ == node) {
    head_ = prev;
  }
  prev->next = node->next;

  // not deleting the node itself!
}

}  // namespace base
