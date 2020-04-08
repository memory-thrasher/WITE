#pragma once

#include "stdafx.h"

template<class T> class AtomicLinkedList {
public:
  AtomicLinkedList(std::weak_ptr<T> data);
  AtomicLinkedList() : AtomicLinkedList(std::weak_ptr<T>()) {};
  ~AtomicLinkedList();
  inline AtomicLinkedList<T>* nextLink();//for iterator
  inline void append(AtomicLinkedList<T>*);//add given node after this one, given must not belong to any list
  inline std::shared_ptr<T> getRef();
  inline T* operator->();//discouraged, use getRef instead
  inline void drop();//remove this thing from the list, making the next and previous elements refer to each other
  inline bool linked();
private:
  AtomicLinkedList(const AtomicLinkedList&) = delete;
  std::atomic<AtomicLinkedList<T>*> next;
  AtomicLinkedList<T>* prev;
  std::weak_ptr<T> data;//null if this is root node
};

template<class T> AtomicLinkedList<T>::AtomicLinkedList(std::weak_ptr<T> data) : next(this), prev(this), data(data) {}

template<class T> AtomicLinkedList<T>::~AtomicLinkedList() {
  drop();
}

template<class T> bool AtomicLinkedList<T>::linked() {
  return next.load(std::memory_order_consume) != this;
}

template<class T> AtomicLinkedList<T>* AtomicLinkedList<T>::nextLink() {
  return next;
}

template<class T> void AtomicLinkedList<T>::append(AtomicLinkedList<T>* n) {//n must not belong to any exiting list
  //"this" is probably the root node
  AtomicLinkedList<T>* oldNext = next.load(std::memory_order_acquire);
  n->next.store(oldNext, std::memory_order_relaxed);
  n->prev = this;
  oldNext->prev = n;
  next.store(n, std::memory_order_release);
}

template<class T> void AtomicLinkedList<T>::drop() {
  //TODO case: next.data gets deleted during this delete; next.locked holds shared pointer to linked node, but not owning T
  AtomicLinkedList<T>* oldNext, *tempThis, *tempPrev;
  oldNext = next.load(std::memory_order_acquire);
  do {
    tempPrev = prev;
    oldNext->prev = tempPrev;
    tempThis = this;
  } while(!tempPrev->next.compare_exchange_strong(tempThis, oldNext, std::memory_order_acq_rel, std::memory_order_relaxed));
  prev = this;
  next.store(this, std::memory_order_release);
}

template<class T> std::shared_ptr<T> AtomicLinkedList<T>::getRef() {
  return data.lock();
}

template<class T> T* AtomicLinkedList<T>::operator->() {
  return data.lock().get();
}

