#ifndef MCP_BASE_BARRIER_SEMAPHORE_HEADER
#define MCP_BASE_BARRIER_SEMAPHORE_HEADER

#include <cstdio> // snprintf, perror
#include <cstdlib> // exit
#include <fcntl.h>  // O_* constants
#include <sys/stat.h> // mode constants
#include <unistd.h>
#include <semaphore.h>

namespace base {

// "Barriers" here refer to the language-level synchronization
// primitive sense (not the memory fence one).
//
// (Not) usage note:
//
//   We do not want to use barriers in multi-threaded coded
//   willy-nilly. One seldom can predict wheather all threads involved
//   in a barrier will progress in a common pace. A slow occasional
//   operation is all it takes to slow down the entire group.
//
//   Testing code might be fine. The above holds there too.
//
class Barrier {
public:

  explicit Barrier(int num_participants)
    : arrival_(SEM_FAILED),
      departure_(SEM_FAILED),
      num_participants_(num_participants),
      counter_(0) {

    // Create a unique name (among threads of this process) for the
    // semaphore. We're not being strict cleaning up in case of
    // aborted executions, so we do so here.
    snprintf(arrival_name_, NAME_SIZE, "A%p", (void *)this);
    snprintf(departure_name_, NAME_SIZE, "B%p", (void *)this);
    unlink(arrival_name_);
    unlink(departure_name_);

    mode_t mode = S_IRUSR | S_IWUSR;
    int flag = O_CREAT | O_EXCL;
    arrival_ = sem_open(arrival_name_, flag, mode, 1);
    departure_ = sem_open(departure_name_, flag, mode, 0);
    if ((arrival_ == SEM_FAILED) || (departure_ == SEM_FAILED)) {
      perror("Couldn't open semaphore");
      exit(1);
      return;
    }
  }

  ~Barrier() {
    sem_close(arrival_);
    sem_close(departure_);
    sem_unlink(arrival_name_);
    sem_unlink(departure_name_);
  }

  void wait() {
    sem_wait(arrival_);
    counter_ += 1;
    if (counter_ < num_participants_) {
      sem_post(arrival_);
    } else {
      sem_post(departure_);
    }

    sem_wait(departure_);
    counter_ -= 1;
    if (counter_ > 0) {
      sem_post(departure_);
    } else {
      sem_post(arrival_);
    }
  }

private:
  // We use the address of this instance to build the semaphore's
  // name, "?OxFFFFFFFFFFFFFFFF". The arrival semaphore adds an 'A'
  // suffix. The departure, a 'D'.
  static const int NAME_SIZE = sizeof(Barrier*)*2 + 4;
  char arrival_name_[NAME_SIZE];
  char departure_name_[NAME_SIZE];

  sem_t* arrival_;              // protects counter_ in the arrival phase
  sem_t* departure_;            // protects counter_ in the departure phase
  const int num_participants_;  // wait() must be called that many times
  int counter_;                 // num participants in the barrier

  // Non-copyable, non-assignable
  Barrier(const Barrier&);
  Barrier& operator=(const Barrier&);
};

} // namespace base

#endif // MCP_BASE_BARRIER_SEMAPHORE_HEADER
