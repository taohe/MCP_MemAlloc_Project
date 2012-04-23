#ifndef MCP_LIST_SET_HEADER
#define MCP_LIST_SET_HEADER

#include "lock.hpp"

namespace base {

using base::Mutex;

// This is a list-based set.
//
// The class is thread-safe for all item manipulation but not for bulk
// clearing or destruction. We expect no concurrent access when the
// latter are performed.
//
class ListBasedSet {
public:

  // Creates a list.
  ListBasedSet();

  // The destructor is not thread-safe.
  ~ListBasedSet();

  // Returns true if 'value' does not yet exist on the set and inserts
  // it. Otherwise returns false.
  bool insert(int value);

  // Returns true if 'value' exists on the set and remove it. Otherwise
  // returns false.
  bool remove(int value);

  // Returns true if 'value' exists on the set.
  bool lookup(int value) const;

  // Removes all the elements from the list. This is not a thread-safe
  // operation.
  void clear();

  // Returns true after traversing the list, making sure the ordering
  // of elements is preserved. Otherwise returns false.
  bool checkIntegrity() const;

private:
  //
  // ADD PRIVATE STATE HERE
  //

  // Non-copyable, non-assignable.
  ListBasedSet(ListBasedSet&);
  ListBasedSet& operator=(const ListBasedSet&);
};

} // namespace base

#endif  // MCP_LIST_SET_HEADER
