#include "SyncLock.hpp"

#include <atomic>

namespace WITE::Collections {

  template<class Node> using AtomicLinkedListNodeMember = Node*;

  template<class Node, AtomicLinkedListNodeMember<Node> Node::*NodePtr> class AtomicLinkedList {
  private:
    Node* first, *last;
    Util::SyncLock mutex;
    AtomicLinkedList(AtomicLinkedList&) = delete;
  public:
    AtomicLinkedList() {};
    void push(Node* gnu) {
      gnu->*NodePtr = NULL;
      Util::ScopeLock lock(&mutex);
      Node* previousLast;
      previousLast = last;
      last = gnu;
      if(!previousLast)
	first = gnu;
      else
	previousLast->*NodePtr = gnu;
      ASSERT_TRAP(previousLast != gnu, "Attempted to self-reference linked list!");
    };
    Node* pop() {
      Util::ScopeLock lock(&mutex);
      Node* ret = first;
      if(!ret) return NULL;
      Node* next = ret->*NodePtr;
      first = next;
      if(!next)
	last = NULL;
      return ret;
    };
    Node* peek() const {
      return first;
    };
    const Node* const peekLast() const {
      Util::ScopeLock lock(&mutex);
      return last;
    };
    const Node* const peekLastDirty() const {
      return peekLast();
    };
    class iterator {
    private:
      Node* current;
      iterator(Node* c) : current(c) {}
      // iterator(Node* c, AtomicLinkedList<Node>* t) : current(c), lock(m) {}
      iterator(iterator& o) : current(o.current) {}
      friend class AtomicLinkedList;
    public:
      operator Node*() { return current; };
      Node* operator->() { return current; };
      iterator& operator++() {
	if(current) current = current->*NodePtr;
	return *this;
      };
      iterator operator++(int) {
	iterator old = *this;
	operator++();
	return old;
      };
      operator bool() { return current; };
    };
    iterator begin() {
      return iterator(root.load(std::memory_order_consume).first);
    };
  };

}
