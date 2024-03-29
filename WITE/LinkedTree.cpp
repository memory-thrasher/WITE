#include "LinkedTree.hpp"

namespace WITE::Collections {

  LinkedTreeBase::LinkedTreeBase() : root(NULL) {};

  LinkedTreeBase::~LinkedTreeBase() = default;

  void LinkedTreeBase::insert(invertedPtr i) {
    if(!root) {
      [[unlikely]]
      //edge case: list is empty
      root = allocate(i, NULL);
      return;
    }
    node* n = root;
    while(n->data != i) {
      node** next = i > n->data ? &n->high : &n->low;
      if(*next)
	n = *next;
      else
	*next = allocate(i, n);;
    }
    //already present, do nothing
  };

  LinkedTreeBase::node* LinkedTreeBase::allocate(invertedPtr i, node* n) {
    node* gnu;
    if(available.empty()) {
      gnu = &store.emplace_back();
    } else {
      gnu = available.back();
      available.pop_back();
    }
    gnu->low = gnu->high = NULL;
    gnu->parent = n;
    gnu->data = i;
    return gnu;
  };

  bool LinkedTreeBase::isEmpty() {
    return !root;
  };

#ifdef DEBUG
  size_t LinkedTreeBase::count() {
    if(!root) return 0;
    IteratorBase i(root, this);
    size_t ret = 1;
    i.getNext();
    while(i.getNext() != root) ret++;
    return ret;
  };
#endif

  bool LinkedTreeBase::contains(invertedPtr i) {
    if(!root) {
      [[unlikely]]
      //edge case: list is empty
      return false;
    }
    node* n = root;
    while(n->data != i) {
      node** next = i > n->data ? &n->high : &n->low;
      if(*next)
	n = *next;
      else
	return false;//not found
    }
    return true;
  };

  void LinkedTreeBase::remove(invertedPtr i) {
    if(!root) {
      [[unlikely]]
      //edge case: list is empty
      return;
    }
    node* n = root;
    while(n->data != i) {
      node** next = i > n->data ? &n->high : &n->low;
      if(*next)
	n = *next;
      else
	return;//not found
    }
    size_t depthOfLow = 0;
    node* l = n->low;
    if(l) {
      depthOfLow++;
      while(l->high) {
	l = l->high;
	depthOfLow++;
      }
    }
    size_t depthOfHigh = 0;
    node* h = n->high;
    if(h) {
      depthOfHigh++;
      while(h->low) {
	h = h->low;
	depthOfHigh++;
      }
    }
    node* newBranch;
    if(h && l) {
      if(depthOfLow < depthOfHigh) {
	newBranch = n->high;
	h->low = n->low;
	h->low->parent = h;
      } else {
	newBranch = n->low;
	l->high = n->high;
	l->high->parent = l;
      }
    } else if(h) {
      newBranch = n->high;
    } else if(l) {
      newBranch = n->low;
    } else {
      newBranch = NULL;
    }
    *(n->parent ? (n->parent->data < i ? &n->parent->high : &n->parent->low) : &root) = newBranch;
    if(newBranch)
      newBranch->parent = n->parent;
    n->data = 0;
  };

  LinkedTreeBase::IteratorBase::IteratorBase(LinkedTreeBase::node* t, LinkedTreeBase* ltb) : target(t), base(ltb) {};

  LinkedTreeBase::IteratorBase::~IteratorBase() = default;

  LinkedTreeBase::node* LinkedTreeBase::IteratorBase::getNext() {
    node* ret = target;
    while(!ret || !ret->data) {
      if(!base->root || !base->root->data)
	return NULL;
      advance();
      ret = target;
    }
    advance();
    return ret;
  };

  void LinkedTreeBase::IteratorBase::advance() {
    if(!target) {
      target = base->root;
      return;
    }
    if(target->low) {
      target = target->low;
      return;
    }
    if(target->high) {
      target = target->high;
      return;
    }
    while(target->parent && (target == target->parent->high || !target->parent->high))
      target = target->parent;
    if(target->parent) {
      target = target->parent->high;
      return;
    }
    target = base->root;
    return;
  };

};

