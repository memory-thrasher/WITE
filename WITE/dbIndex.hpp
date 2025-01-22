/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <concepts>

#include "dbFile.hpp"
#include "callback.hpp"
#include "concurrentReadSyncLock.hpp"

namespace WITE {

  //not to be embedded into a datatype or database, but should target data that is
  //ideally the per-record value of the field being indexed should not change. If it does, it is the caller's responsibility to call update()
  //rebalance should be called externally after significant cumulative changes
  template<class F, class Compare = std::less<F>> requires requires(F& a, F& b) {
    { Compare()(a, b) } -> std::convertible_to<bool>;
    { F{} };
    { a = b };
  }
  struct dbIndex {
    struct node {
      F targetValue;
      /*T*/ uint64_t target = NONE;
      /*node*/ uint64_t high = NONE, low = NONE;

      std::strong_ordering operator<=>(const F& r) const {
	if(Compare()(targetValue, r)) return std::strong_ordering::less;
	else if(Compare()(r, targetValue)) return std::strong_ordering::greater;
	else return std::strong_ordering::equal;
      };

      std::strong_ordering operator<=>(const node& r) const {
	return *this <=> r.targetValue;
      };

      uint64_t findAny(const F& v, dbIndex* owner) {
	auto comp = *this <=> v;
	if(comp == 0) return target;
	uint64_t nid = comp < 0 ? high : low;
	return nid == NONE ? NONE : owner->file.deref(nid).findAny(v, owner);
      };

      //can't store owner as a field bc node is on disk and outlives its owner
      void insert(uint64_t entity, const F& v, dbIndex* owner) {
	uint64_t& next = *this < v ? low : high;//is the smell worth avoiding a few more ternaries?
	if(next != NONE) [[likely]] {
	  owner->file.deref(next).insert(entity, v, owner);
	} else {
	  next = owner->file.allocate();
	  node& n = owner->file.deref(next);
	  n.targetValue = v;
	  n.target = entity;
	  n.high = n.low = NONE;
	}
      };

      // void update(dbIndex* owner) {
      // 	owner->read(target, targetValue);
      // };

      // void updateAll(dbIndex* owner) {
      // 	update(owner);
      // 	if(high != NONE) [[likely]] owner->file.deref(high).updateAll(owner);
      // 	if(low != NONE) [[likely]] owner->file.deref(low).updateAll(owner);
      // };

      void remove(const F& v, dbIndex* owner, uint64_t& thisId) {
	auto comp = *this <=> v;
	if(comp == 0) {
	  //either low or high will replace this branch, and the other will be linked by the end of the replacement's chain, following the extreme edge. Just avoid removing things, or rebalance after.
	  uint64_t temp = thisId;
	  if(low == NONE) {
	    thisId = high;
	  } else if(high == NONE) {
	    thisId = low;
	  } else {
	    uint64_t lowCnt = 0, lowId = low;
	    while(true) {
	      uint64_t next = owner->file.deref(lowId).high;
	      if(next == NONE) break;
	      lowId = next;
	    }
	    uint64_t highCnt = 0, highId = high;
	    while(true) {
	      uint64_t next = owner->file.deref(highId).low;
	      if(next == NONE) break;
	      highId = next;
	    }
	    if(highCnt < lowCnt) {//high replaces this, low appended to low side of high
	      thisId = high;
	      owner->file.deref(highId).low = low;
	    } else {//low replaces this, high appended to high side of low
	      thisId = low;
	      owner->file.deref(lowId).high = high;
	    }
	  }
	  owner->file.free(temp);
	  return;
	} else if(comp < 0) {
	  if(high != NONE) [[likely]] owner->file.deref(high).remove(v, owner, high);
	} else {
	  if(low != NONE) [[likely]] owner->file.deref(low).remove(v, owner, low);
	}
      };

      uint64_t count(dbIndex* owner) {
	uint64_t lowCnt = low == NONE ? 0 : owner->file.deref(low).count(owner, low);
	uint64_t highCnt = high == NONE ? 0 : owner->file.deref(high).count(owner, high);
	return lowCnt + highCnt + 1;
      };

      //@param thisId is updated if this node is moved
      uint64_t rebalance(dbIndex* owner, uint64_t& thisId) {
	uint64_t lowCnt = low == NONE ? 0 : owner->file.deref(low).rebalance(owner, low);
	uint64_t highCnt = high == NONE ? 0 : owner->file.deref(high).rebalance(owner, high);
	if(lowCnt + lowCnt/4 + 1 < highCnt) {
	  node& other = owner->file.deref(high);
	  uint64_t temp = thisId;
	  thisId = high;
	  high = other.low;
	  other.low = temp;
	  rebalance(owner, other.low);
	} else if(highCnt + highCnt/4 + 1 < lowCnt) {
	  node& other = owner->file.deref(low);
	  uint64_t temp = thisId;
	  thisId = low;
	  low = other.high;
	  other.high = temp;
	  rebalance(owner, other.high);
	}
	return lowCnt + highCnt + 1;
      };

    };

    dbFile<node, 65536/sizeof(node)+1> file;//shoot for 64kb page
    typedefCB(read_cb, void, uint64_t, F&);//might contain a read op from a database or another dbFile
    read_cb read;
    concurrentReadSyncLock mutex;

    dbIndex(const std::filesystem::path& fn, bool clobber, read_cb read) : file(fn, clobber), read(read) {
      //file.first() is used to track the pseudo node that holds a reference to the root node (high)
      if(file.first() == NONE)
	file.allocate();
    };

    //if this is needed, we will need to rearrange nodes, probably start over with a new list
    // void update() {//updates targetValue from target on every node
    //   concurrentReadLock_write lock(&mutex);
    //   uint64_t nid = file.deref(file.first()).high;
    //   if(nid == NONE) [[unlikely]] return;
    //   file.deref(nid).updateAll(this);
    // };

    //returns number of records in index
    uint64_t rebalance() {//probably very slow
      concurrentReadLock_write lock(&mutex);
      uint64_t nid = file.deref(file.first()).high;
      if(nid == NONE) [[unlikely]] return 0;
      return file.deref(nid).rebalance(this, nid);
    };

    uint64_t count() {
      concurrentReadLock_write lock(&mutex);
      uint64_t nid = file.deref(file.first()).high;
      if(nid == NONE) [[unlikely]] return 0;
      return file.deref(nid).count(this);
    };

    uint64_t findAny(const F& v) {
      concurrentReadLock_read lock(&mutex);
      uint64_t nid = file.deref(file.first()).high;
      if(nid == NONE) [[unlikely]]
	return NONE;
      return file.deref(nid).findAny(v, this);
    };

    void insert(uint64_t entity, const F& v) {
      concurrentReadLock_write lock(&mutex);
      uint64_t nid = file.deref(file.first()).high;
      if(nid == NONE) [[unlikely]] { //first insert
	nid = file.allocate();
	node& n = file.deref(nid);
	n.target = entity;
	n.targetValue = v;
	n.high = n.low = NONE;
      } else {
	node& n = file.deref(nid);
	n.insert(entity, v, this);
      }
    };

    void insert(uint64_t entity) {
      F v;
      read(entity, v);
      insert(entity, v);
    };

    //implement if needed
    // uint64_t findNth(const F& v, uint64_t n);//if multiple values exist that match F, find the nth one in an order that is consistent between calls to this (unmodified) index but otherwise undefined
    // uint64_t count(const F& v);

  };

}
