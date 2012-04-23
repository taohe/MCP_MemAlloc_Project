#include "thread_registry.hpp"

namespace base {

using std::pair;

pthread_once_t ThreadRegistry::init_control_ = PTHREAD_ONCE_INIT;
ThreadRegistry* ThreadRegistry::instance_ = NULL;

ThreadRegistry::ThreadRegistry() {
}

ThreadRegistry::~ThreadRegistry() {
}

void ThreadRegistry::init() {
  instance_ = new ThreadRegistry;
}

ThreadRegistry* ThreadRegistry::instance() {
  pthread_once(&init_control_, &ThreadRegistry::init);
  return instance_;
}

bool ThreadRegistry::registerThread(pthread_t tid) {
  ScopedLock l(&m_);
  pair<ThreadIDs::iterator, bool> res;
  res = thread_ids_.insert(tid);
  return res.second;
}

bool ThreadRegistry::unregisterThread(pthread_t tid) {
  ScopedLock l(&m_);
  return thread_ids_.erase(tid) > 0;
}

void ThreadRegistry::getAllThreadIDs(unordered_set<pthread_t>* tids) const {
  ScopedLock l(&m_);
  tids->clear();
  tids->insert(thread_ids_.begin(), thread_ids_.end());
}

} // namespace base
