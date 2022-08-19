namespace WITE::Collections {

  template<class T> using LinkedListAnchor = T*;

  template<class T, LinkedListAnchor<T> T::* A> class LinkedList {//root element, stored in something other than T
  public:
    // template<Anchor T::A>
    struct RootNode {
      T* first;
      T* last;
    };
    using LinkedType = T;
  private:
    RootNode root;
  public:
    void append(T* n) {
      n->*A = root.last;
      root.last = n;
    };

    class iterator {
    private:
      T* t;
    public:
      iterator() : t(NULL) {};
      iterator(T* t) : t(t) {};
      LinkedType& operator*() const { return *t; };
      LinkedType* operator->() const { return t; };
      iterator& operator++() {
	if(t) t = t->*A;
	return *this;
      };
      iterator operator++(int) {
	iterator old = *this;
	operator++();
	return old;
      };
      inline bool operator==(const iterator& r) const { return t == r.t; };
      inline bool operator!=(const iterator& r) const { return !(*this == r); };
      inline iterator& operator=(const iterator& other) {
	if(this == &other) return *this;
	t = other.t;
	return *this;
      };
    };
    iterator begin() const {
      return root.first;
    };
    iterator end() const {
      return iterator();
    };

    template<class UP> void remove_if(UP p) {
      if(!root.first) return;
      T* current = root.first;
      while(current && p(current)) {
	T* temp = current;
	current = current->*A;
	temp->*A = NULL;
      }
      if(!current) {
	root.first = root.last = NULL;
	return;
      } else root.first = current;
      T* last = current;
      while(current) {
	while(current && p(current)) {
	  T* temp = current;
	  current = current->*A;
	  temp->*A = NULL;
	  last->*A = current;
	}
	if(current) {
	  last = current;
	  current = current->*A;
	} else {
	  last->*A = NULL;
	  root.last = last;
	}
      }
    };
  };

}
