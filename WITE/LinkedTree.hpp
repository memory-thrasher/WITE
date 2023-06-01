#pragma once

#include <deque>

#include "Utils_Memory.hpp"

namespace WITE::Collections {

  class LinkedTreeBase {
  protected:
    typedef uint64_t invertedPtr;
    static_assert(sizeof(uint64_t) == sizeof(void*));
    struct node {
      invertedPtr data;
      node* low, *high, *parent;
    };
  private:
    node* allocate(invertedPtr i, node* n);
  public:
    class IteratorBase {
    private:
      node* target;
      LinkedTreeBase* base;
      void advance();
    public:
      IteratorBase(node*, LinkedTreeBase*);
      virtual ~IteratorBase();
      node* getNext();
    };
  protected:
    node* root;
    std::deque<node> store;
    std::deque<node*> available;
    ~LinkedTreeBase();
    LinkedTreeBase();
    void insert(invertedPtr);
    void remove(invertedPtr);
  };

  //Not inherently thread safe. Protect the collection itself with a mutex. Read only iterators are thread safe but may fail sporadically.
  template<class T> class LinkedTree : public LinkedTreeBase {
  private:
    static_assert(sizeof(invertedPtr) == sizeof(T*));
    static inline invertedPtr reverse(T* i) { return Util::reverse(reinterpret_cast<uint64_t>(i)); };
    static inline T* reverse(invertedPtr i) { return reinterpret_cast<T*>(Util::reverse(i)); };
  public:
    class Iterator : public IteratorBase {
    public:
      Iterator(node* t, LinkedTreeBase* ltb) : IteratorBase(t, ltb) {};
      inline T* operator()() { auto n = getNext(); return n ? reverse(n->data) : NULL; };
    };
    inline void insert(T* t) { LinkedTreeBase::insert(reverse(t)); };
    inline void remove(T* t) { LinkedTreeBase::remove(reverse(t)); };
    inline Iterator iterate() { return { root, this }; };//nonstandard ENDLESS! iterator.
  };

}
