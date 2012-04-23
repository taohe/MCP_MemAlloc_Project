#include <stdio.h>  // perror
#include <stdlib.h> // exit

#include "logging.hpp"
#include "thread.hpp"
#include "thread_registry.hpp"

namespace base {

static void* threadFunction(void* arg) {
  ThreadRegistry* registry = ThreadRegistry::instance();
  pthread_t tid = pthread_self();
  registry->registerThread(tid);
  (* reinterpret_cast<Callback<void>*>(arg))();
  registry->unregisterThread(tid);
  return 0;
}

pthread_t makeThread(Callback<void>* body) {
  void* arg = reinterpret_cast<void*>(body);
  pthread_t tid;
  if (pthread_create(&tid, NULL, threadFunction, arg) != 0) {
    LOG(LogMessage::FATAL) << "Can't create thread";
    return -1;
  }
  return tid;
}

}  // namespace base
