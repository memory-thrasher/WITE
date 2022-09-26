#include <atomic>

namespace WITE::Collections {

  template<class Node> using AtomicLinkedListNodeMember = Node*volatile;

  template<class Node, AtomicLinkedListNodeMember<Node> Node::*NodePtr> class AtomicLinkedList {
  private:
    typedef struct { Node* first, *last; } root_t;
    std::atomic<root_t> root;
    AtomicLinkedList(AtomicLinkedList&) = delete;
  public:
    AtomicLinkedList() {};
    void push(Node* gnu) {
      gnu->*NodePtr = NULL;
      Node* previousLast;
      root_t newRoot, temp = root.load(std::memory_order_acquire);
      do {
	newRoot = temp;
	previousLast = newRoot.last;
	newRoot.last = gnu;
	if(!previousLast)
	  newRoot.first = gnu;
      } while(!root.compare_exchange_weak(temp, newRoot, std::memory_order_release, std::memory_order_relaxed));
      ASSERT_TRAP(previousLast != gnu, "Attempted to self-reference linked list!");
      if(previousLast)
	previousLast->*NodePtr = gnu;
    };
    Node* pop() {
      Node* ret;
      root_t newRoot, temp = root.load(std::memory_order_acquire);
      do {
	newRoot = temp;
	ret = newRoot.first;
	if(!ret) return NULL;
	Node* next = ret->*NodePtr;
	newRoot.first = next;
	if(!next)
	  newRoot.last = NULL;
      } while(!root.compare_exchange_weak(temp, newRoot, std::memory_order_release, std::memory_order_relaxed));
      return ret;
    };
    Node* peek() const {
      return root.load(std::memory_order_consume).first;
    };
    const Node* const peekLast() const {
      return root.load(std::memory_order_consume).last;
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
