#include "LinkedTree.hpp"

namespace WITE::Collections {

  LinkedTreeBase::LinkedTreeBase() = default;

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
    *(n->parent ? (n->parent->data < i ? &n->parent->low : &n->parent->high) : &root) = newBranch;
    newBranch->parent = n->parent ? n->parent : NULL;
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
    while(target->parent && target == target->parent->high)
      target = target->parent;
    if(target->parent) {
      target = target->parent->high;
      return;
    }
    target = base->root;
    return;
  };

};

