namespace WITE::Collections {

  template<class Node> using SynchronizedLinkedListNodeMember = Node*;

  template<class Node, SynchronizedLinkedListNodeMember<Node> Node::*NodePtr> class SynchronizedLinkedList {
  private:
    Node *first, *last;
    mutable Util::SyncLock mutex;
    SynchronizedLinkedList(SynchronizedLinkedList&) = delete;
  public:
    SynchronizedLinkedList() : first(NULL), last(NULL) {};//not being created by default?
    void push(Node* gnu) {
      gnu->*NodePtr = NULL;
      Util::ScopeLock lock(&mutex);
      Node* previousLast = last;
      last = gnu;
      if(previousLast)
	previousLast->*NodePtr = gnu;
      else
	first = gnu;
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
      Util::ScopeLock lock(&mutex);
      return first;
    };
    const Node* const peekLast() const {
      Util::ScopeLock lock(&mutex);
      return last;
    };
    const Node* const peekLastDirty() const {
      return last;
    };
    class iterator {
    private:
      Node* current;
      Util::ScopeLock lock;
      iterator(Node* c, Util::SyncLock* m) : current(c), lock(m) {}
      // iterator(Node* c, SynchronizedLinkedList<Node>* t) : current(c), lock(m) {}
      iterator(iterator& o) : current(o.current), lock(o.lock) {}
      friend class SynchronizedLinkedList;
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
      return iterator(first, &mutex);
    };
  };

}
