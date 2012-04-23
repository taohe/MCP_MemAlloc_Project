#ifndef MCP_BASE_DESCRIPTOR_POLLER_HEADER
#define MCP_BASE_DESCRIPTOR_POLLER_HEADER

namespace base {

class Descriptor;

// The class is a wrapper around the different poll() variations.
// Currently, it works with epoll (in Linux) and kqueue (in MAC OS).
class DescriptorPoller {
public:
  // Events the implementation should monitor for.
  enum PollEvents {
    DP_ERROR       = 0x0000001,
    DP_READ_READY  = 0x0000002,
    DP_WRITE_READY = 0x0000004
  };

  DescriptorPoller();
  ~DescriptorPoller();

  // Initializes internal event processing machinery. Must be called
  // before any other call in the class is issued.
  void create();

  // Adds the descriptor 'fd' to the poller, associating 'descr' with
  // that descriptor's additional data. 'fd' will going to be polled
  // for all the events in PollEvents by all the subsequent calls to
  // Poll().
  void setEvent(int fd, Descriptor* descr);

  // Returns the number of ready descriptors and prepare to issue
  // 'getEvents()' for each of them.
  int poll();

  // Returns in 'event's the anding of all PollEvents set for the i-th
  // descriptor, along with that descriptors additional data,
  // 'descr'.  'i' must be within the bounds returned by the previous
  // Poll() call.
  void getEvents(int i, int* events, Descriptor** descr);

private:
  // The implementation is architecture dependant.
  struct InternalPoller;
  InternalPoller* poller_;

  // Non-copyable, non-assignable.
  DescriptorPoller(const DescriptorPoller&);
  DescriptorPoller& operator=(const DescriptorPoller&);
};


}  // namespace base

#endif   // MCP_BASE_DESCRIPTOR_POLLER_HEADER
