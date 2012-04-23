#ifndef MCP_BASE_THREAD_HEAD
#define MCP_BASE_THREAD_HEAD

#include <pthread.h>
#include "callback.hpp"

namespace base {

using base::Callback;

// makeThread creates a new thread and returns its thread ID. The
// thread will run 'body', which can be any function or object method
// with no arguments and no return.
//
// Usage:
//   #include "callback.hpp"
//
//   class Server {
//   public:
//     Server(...);
//     void doTask();
//   ...
//   };
//
//   Server my_server(...);
//
//   // Let's run my_server.doTask() on a separate thread, go do something else,
//   // and then wait for that thread to finish.
//   Callback<void>* threadBody = makeCallableOnce(&Server::doTask, &my_server);
//   pthread_t tid = makeThread(threadBody);
//   ... go do something else ...
//   pthread_join(tid, NULL);
//

// Returns the thread id of the thread create to run 'body'. The
// latter onwership is determined by whether the callback is call-once
// or not. Internally, 'body' is only invoked once.
//
// If a thread cannot be created, this call will exit()
pthread_t makeThread(Callback<void>* body);

}  // namespace base

#endif // MCP_BASE_THREAD_HEAD
