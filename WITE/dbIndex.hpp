/*
Copyright 2020-2024 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include "dbFile.hpp"
#include "callback.hpp"
#include "concurrentReadSyncLock.hpp"

namespace WITE {

  //not to be embedded into a datatype or database, but should target data that is
  //ideally the per-record value of the field being indexed should not change. If it does, it is the caller's responsibility to call update()
  //rebalance should be called externally after significant cumulative changes
  template<class F, class Compare = std::less<F>> struct dbIndex {
    struct node {
      F targetValue;
      /*T*/ uint64_t target = NONE;
      /*node*/ uint64_t high = NONE, low = NONE, behind = NONE;

      std::strong_ordering operator<=>(const F& r) const {
	if(Compare()(targetValue, r)) return std::strong_ordering::less;
	else if(Compare()(r, targetValue)) return std::strong_ordering::greater;
	else return std::strong_ordering::equal;
      };

      std::strong_ordering operator<=>(const node& r) const {
	return *this <=> r.targetValue;
      };

      uint64_t findAny(const F& v, dbIndex* owner) {
	auto comp = n <=> v;
	if(comp == 0) return target;
	uint64_t nid = comp < 0 ? n.high : n.low;
	return nid == NONE ? NONE : owner->file.deref(nid).findAny(v, owner);
      };

      //can't store owner as a field bc node is on disk and outlives its owner
      void insert(uint64_t entity, const F& v, dbIndex* owner, uint64_t thisId) {
	uint64_t& next = *this < v ? low : high;//is the smell worth avoiding a few more ternaries?
	if(next != NONE) [[likely]] {
	  owner->file.deref(next).insert(etity, v, owner);
	} else {
	  next = owner->file.allocate();
	  node& n = owner->file.deref(next);
	  n.targetValue = v;
	  n.target = entity;
	  n.behind = thisId;
	}
      };

      void update(dbIndex* owner) {
	owner->read(target, targetValue);
      };

      void updateAll(dbIndex* owner) {
	update(owner);
	if(high != NONE) [[likely]] owner->file.deref(high).updateAll(owner);
	if(low != NONE) [[likely]] owner->file.deref(low).updateAll(owner);
    };

    dbFile<node, 65536/sizeof(node)+1> file;//shoot for 64kb page
    typedefCB(read_cb, void, uint64_t, F&);//might contain a read op from a database or another dbFile
    read_cb read;
    concurrentReadSyncLock mutex;

    dbIndex(const std::filesystem::path& fn, bool clobber, read_cb read) : file(fn, clobber), read(read) {};

    void update() {//updates targetValue from target on every node
      uint64_t nid = file.first();
      if(nid == NONE) [[unlikely]] return;
      file.deref(nid).updateAll(this);
    };

    void rebalance() {//probably very slow
      uint64_t nid = file.first();
      if(nid == NONE) [[unlikely]] return;
      // file.deref(nid).rebalance(this);
      #error TODO
    };

    uint64_t findAny(const F& v) {
      uint64_t nid = file.first();
      if(nid == NONE) [[unlikely]]
	return NONE;
      return file.deref(nid).findAny(v, this);
    };

    void insert(uint64_t entity, const F& v) {
      uint64_t nid = file.first();
      if(nid == NONE) [[unlikely]] { //first insert
	nid = file.allocate();
	node& n = file.deref(nid);
	n.target = entity;
	n.targetValue = v;
	n.high = n.low = n.behind = NONE;
      } else {
	node& n = file.deref(nid);
	n.insert(entity, v, this, nid);
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
