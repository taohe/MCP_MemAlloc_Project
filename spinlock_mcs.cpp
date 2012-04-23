#include "spinlock_mcs.hpp"

namespace base {

__thread SpinlockMCS::Node* SpinlockMCS::qnode_;

}
