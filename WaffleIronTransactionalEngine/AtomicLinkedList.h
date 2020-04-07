#pragma once

#include "stdafx.h"

template<class T> class AtomicLinkedList {
public:
  AtomicLinkedList(std::weak_ptr<T> data);
  AtomicLinkedList() : this(std::weak_ptr<T>()) {};
  ~AtomicLinkedList();
  inline std::shared_ptr<AtomicLinkedList<T>> nextLink();//for iterator
  inline void append(AtomicLinkedList<T>*);
  inline void append(std::shared_ptr<AtomicLinkedList<T>>);
  inline std::shared_ptr<T> getRef();
  inline T* operator->();//discouraged, use getRef instead
private:
  inline void drop();//remove this thing from the list, making the next and previous elements refer to each other
  AtomicLinkedList(const AtomicLinkedList&) = delete;
  std::atomic<std::weak_ptr<AtomicLinkedList<T>>> prev;
  std::weak_ptr<AtomicLinkedList<T>> next, thisWeak;
  std::weak_ptr<T> data;//null if this is root node
};

template<class T> AtomicLinkedList<T>::AtomicLinkedList(std::weak_ptr<T> data) :
  thisWeak(std::make_shared<AtomicLinkedList<T>>(this)), next(thisWeak), prev(thisWeak), data(data) {}

template<class T> AtomicLinkedList<T>::~AtomicLinkedList() {
  drop();
}

template<class T> std::shared_ptr<AtomicLinkedList<T>> AtomicLinkedList<T>::nextLink() {
  return next.lock();
}

template<class T> void AtomicLinkedList<T>::append(AtomicLinkedList<T>* n) {
  append(new std::shared_ptr(n));
}

template<class T> void AtomicLinkedList<T>::append(std::shared_ptr<AtomicLinkedList<T>> n) {
  //"this" is probably the root node
  std::shared_ptr<AtomicLinkedList<T>> tail;
  n->next = thisWeak;
  tail = this.prev.exchange(std::weak_ptr<AtomicLinkedList<T>>(n), std::memory_order_acq_rel).lock();//this is the real fence
  n->prev.store(std::weak_ptr<AtomicLinkedList<T>>(tail), std::memory_order_release);//maybe could be relaxed
  tail.next = n;
}

template<class T> void AtomicLinkedList<T>::drop() {
  //TODO case: next.data gets deleted during this delete; next.locked holds shared pointer to linked node, but not owning T
  std::weak_ptr<AtomicLinkedList<T>> p, temp;
  p = this.prev.exchange(this);
  temp = thisWeak;
  while (!next || !next.lock().prev.compare_exchange_weak(temp, p)) temp = thisWeak;
  p.lock().next = next;
}

template<class T> std::shared_ptr<T> AtomicLinkedList<T>::getRef() {
  return data.lock();
}

template<class T> T* AtomicLinkedList<T>::operator->() {
  return data.lock().get();
}

