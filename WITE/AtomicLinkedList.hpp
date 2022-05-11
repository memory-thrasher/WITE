namespace WITE::Collections {

  template<class Node> using AtomicLinkedListNodeMember = Node*;//should this be atomic?

  template<class Node, AtomicLinnkedListNodeMember Node::*NodePtr> class AtomicLinkedList {
  private:
    std::atomic<Node*> first, last;
    AtomicLinkedList(AtomicLinkedList&) = delete;
  public:
    void push(Node* gnu) {
      gnu->*NodePtr = NULL;
      Node* previousLast = last.exchange(gnu, std::memory_order_acq_rel);
      previousLast->*NodePtr = gnu;
    };
    Node* pop() {
      Node* ret = first.Load(std::memory_order_acquire);
      do {
	if(!ret) return NULL;
	Node* next = ret->*NodePtr;
      } while(!first.compare_exchange_strong(&ret, next, std::memory_order_release, std::memory_order_acq_rel));
      return ret;
    };
    Node* peek() {
      return first.Load(std::memory_order_consume);
    };
    class Iterator {
    private:
      Node* current;
      Iterator(Node* c) : current(c) {}
    public:
      Node* operator->() { return current; };
      Iterator& operator++() {
	if(current) current = current->*NodePtr;
	return *this;
      };
      Iterator operator++(int) {
	Iterator old = *this;
	operator++();
	return old;
      };
    };
    Iterator begin() { return Iterator(first.load(std::memory_order_consume)); };
  }

}
