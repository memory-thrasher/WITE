#pragma once

namespace WITE::Collections {

  template<class T> using LinkedListAnchor = T*;

  template<class T, LinkedListAnchor<T> T::* A> class LinkedList {//root element, stored in something other than T
  public:
    // template<Anchor T::A>
    using LinkedType = T;
  private:
    struct RootNode {
      T* first;
      T* last;
    };
    RootNode root;
  public:
    void append(T* n) {
      if(root.last)
	root.last->*A = n;
      root.last = n;
      if(!root.first)
	root.first = n;
    };
    inline void push(T* t) { append(t); };
    T* pop() {
      T* ret = root.first;
      if(!ret) return ret;
      T* next = ret->*A;
      root.first = next;
      if(!next)
	root.last = NULL;
      return ret;
    };
    inline const T* peek() const { return root.first; };
    inline const T* peekLast() const { return root.last; };
    inline auto peekLastDirty() const { return peekLast(); };//for drop-in replacement of atomic

    class iterator {
    private:
      T* t;
    public:
      iterator() : t(NULL) {};
      iterator(T* t) : t(t) {};
      operator T*() { return t; };
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
      operator bool() { return t; };
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
