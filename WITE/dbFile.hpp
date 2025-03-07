/*
Copyright 2020-2025 Wafflecat Games, LLC

This file is part of WITE.

WITE is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

WITE is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with WITE. If not, see <https://www.gnu.org/licenses/>.

Stable and intermediate releases may be made continually. For this reason, a year range is used in the above copyrihgt declaration. I intend to keep the "working copy" publicly visible, even if it is not functional. I consider every push to this publicly visible repository as a release. Releases intended to be stable will be marked as such via git tag or similar feature.
*/

#pragma once

#include <cstring> //memcpy

#ifdef DEBUG
#include <set>
#endif

#include "syncLock.hpp"
#include "concurrentReadSyncLock.hpp"
#include "DEBUG.hpp"
#include "shared.hpp"
#include "mmap.hpp"

namespace WITE {

  template<class T, size_t AU>//allocation units, number of T per file growth. sizeof(T)*AU is minimum file size and the increment
  class dbFile {

  private:
    struct link_t {
      uint64_t previous = NONE, next = NONE;
    };
    struct header_t {
      uint64_t freeSpaceLen = 0;//lifo queue position
      uint64_t allocatedFirst = NONE, allocatedLast = NONE;//LL root node
    };
    struct au_t {
      uint64_t freeSpace[AU];//lifo queue space
      link_t allocatedLL[AU];//parallels data, shows which are allocated
      T data[AU];
    };
    static constexpr size_t au_size = sizeof(au_t);
    static constexpr uint8_t plug[au_size] = { 0 };

    syncLock fileMutex;
    concurrentReadSyncLock blocksMutex, allocationMutex;
    header_t* header;
    std::vector<au_t*> blocks;//these are pointers into mmaped regions
    struct mmap_t { void*region; size_t len; };
    std::vector<mmap_t> mmapedRegions;//for later gc, because elements of `blocks` might be trimmed due to alignment requirements
    fileHandle fd;
    const std::filesystem::path filename;
    size_t fileSize;
#if DEBUG
    std::set<uint64_t> freeSpaceBitmap;//sanity check for debugging only, duplicates the on-disk allocation queue
#endif

    void initialize(uint64_t auId) {
      ASSERT_TRAP(header->freeSpaceLen <= auId * AU, "queue position invalid before initializing new allocation unit");
      uint64_t base = auId * AU;
      WITE_DEBUG_DB_HEADER;
      for(uint32_t i = 0;i < AU;i++) {
	int j = base + i;
	freeSpaceLEA(header->freeSpaceLen) = j;
	WITE_DEBUG_DB_FREESPACE(header->freeSpaceLen);
	header->freeSpaceLen++;
	ASSERT_TRAP(freeSpaceBitmap.insert(j).second, "duplicate entity found in free space queue ", j);
      }
      WITE_DEBUG_DB_HEADER;
    };

    inline uint64_t& freeSpaceLEA(uint64_t idx) {
      ASSERT_TRAP(idx / AU < blocks.size(), "out of bounds: block does not exist");
      return blocks[idx / AU]->freeSpace[idx % AU];
    };

    inline link_t& allocatedLEA(uint64_t idx) {
      ASSERT_TRAP(idx / AU < blocks.size(), "out of bounds: block does not exist");
      return blocks[idx / AU]->allocatedLL[idx % AU];
    };

  public:
    dbFile() = delete;
    dbFile(dbFile&&) = delete;

    dbFile(const std::filesystem::path& fn, bool clobber) : filename(fn) {
      scopeLock fl(&fileMutex);
      concurrentReadLock_write bm(&blocksMutex), am(&allocationMutex);
      const std::filesystem::path dir = filename.parent_path();
      std::error_code ec;
      if(!std::filesystem::exists(dir))
	ASSERT_TRAP_OR_RUN(std::filesystem::create_directories(dir, ec), "create dir failed ", ec);
      ASSERT_TRAP(std::filesystem::is_directory(dir, ec), "not a directory ", ec);
      fd = WITE::openFile(filename, true, clobber);
      ASSERT_TRAP_OR_RUN(WITE::lockFile(fd), "failed to lock file ", filename); //lock will be closed when fd is closed
      fileSize = static_cast<size_t>(std::filesystem::file_size(filename));
      size_t existingAUs = fileSize ? (fileSize - sizeof(header_t)) / au_size : 0;
      if(existingAUs) [[likely]] {//load
	ASSERT_TRAP((fileSize - sizeof(header_t)) % au_size == 0, "attempted to load file with invalid size");
	WITE::seekFileEnd(fd);
      } else {//init
	header_t temph;
	writeFile(fd, &temph);
	writeArrayFile(fd, plug, au_size);
	fileSize = au_size + sizeof(header_t);
      }
      //TODO maximum size?
      void* mm = WITE::mmapFile(fd, 0, fileSize);
      mmapedRegions.emplace_back(mm, fileSize);
      header = reinterpret_cast<header_t*>(mm);
      blocks.emplace_back(reinterpret_cast<au_t*>(reinterpret_cast<uint8_t*>(mm) + sizeof(header_t)));
      if(existingAUs) [[likely]] {
#if DEBUG
	ASSERT_TRAP(header->freeSpaceLen <= existingAUs * AU, "invalid free space length (recovery nyi)");
	for(uint64_t i = 0;i < header->freeSpaceLen;i++) {
	  uint64_t j = freeSpaceLEA(i);
	  ASSERT_TRAP(freeSpaceBitmap.emplace(j).second, "duplicate entity found in free space queue ", j);
	}
#endif
	//if the file contains multiple allocation units, then those all share one large mmap, but still populate `blocks` with portions of that mmap rather than complicate the logic of deciding which map to use
	for(uint64_t i = 1;i < existingAUs;i++)
	  blocks.emplace_back(blocks[0] + i);
      } else {//initialize file contents
	initialize(0);
      }
    };

    ~dbFile() {
      close();
    };

    void close() {
      scopeLock fl(&fileMutex);
      concurrentReadLock_write bm(&blocksMutex), am(&allocationMutex);
      if(mmapedRegions.size()) {
	WITE::unlockFile(fd);
	for(mmap_t& m : mmapedRegions)
	  WITE::closeMmapFile(m.region, m.len);
	WITE::closeFile(fd);
	mmapedRegions.clear();
      }
    };

    uint64_t allocate() {
      concurrentReadLock_write am(&allocationMutex);
      uint64_t ret;
      WITE_DEBUG_DB_HEADER;
      if(!header->freeSpaceLen) [[unlikely]] {
	scopeLock fl(&fileMutex);
	concurrentReadLock_write bm(&blocksMutex);
	uint64_t auId = blocks.size();
	writeArrayFile(fd, plug, au_size);
	size_t pageSize = fileSizeMultiple(),
	  start = auId * au_size + sizeof(header_t),
	  realStart = start / pageSize * pageSize,
	  entryPad = start - realStart,
	  realLength = au_size + entryPad;
	void* mm = WITE::mmapFile(fd, realStart, realLength);
	mmapedRegions.emplace_back(mm, realLength);
	blocks.emplace_back(reinterpret_cast<au_t*>(reinterpret_cast<uint8_t*>(mm) + entryPad));
	initialize(auId);
      }
      ASSERT_TRAP(header->freeSpaceLen, "disk allocation failed?");
      ret = freeSpaceLEA(--header->freeSpaceLen);
#if DEBUG
      auto iter = freeSpaceBitmap.find(ret);
      ASSERT_TRAP(iter != freeSpaceBitmap.end(), "allocated entity from free space queue not in bitmap ", ret);
      freeSpaceBitmap.erase(iter);
#endif
      link_t& l = allocatedLEA(ret);
      l.previous = header->allocatedLast;//might be NONE
      l.next = NONE;
      if(header->allocatedFirst == NONE) [[unlikely]] {//list was empty
	header->allocatedFirst = ret;
      } else {
	ASSERT_TRAP(header->allocatedLast != NONE, "root node in invalid state");
	link_t& oldLast = allocatedLEA(header->allocatedLast);
	ASSERT_TRAP(oldLast.next == NONE, "last node didn't know it was last.");
	oldLast.next = ret;
      }
      header->allocatedLast = ret;
      WITE_DEBUG_DB_HEADER;
#ifdef WITE_DEBUG_DB
      WARN("dbFile: ", filename, "Allocated: ", ret);
#endif
      return ret;
    };

    uint64_t allocate_unsafe() {
      uint64_t ret;
      WITE_DEBUG_DB_HEADER;
      if(!header->freeSpaceLen) [[unlikely]] {
	uint64_t auId = blocks.size();
	writeArrayFile(fd, plug, au_size);
	size_t pageSize = fileSizeMultiple(),
	  start = auId * au_size + sizeof(header_t),
	  realStart = start / pageSize * pageSize,
	  entryPad = start - realStart,
	  realLength = au_size + entryPad;
	void* mm = WITE::mmapFile(fd, realStart, realLength);
	mmapedRegions.emplace_back(mm, realLength);
	blocks.emplace_back(reinterpret_cast<au_t*>(reinterpret_cast<uint8_t*>(mm) + entryPad));
	initialize(auId);
      }
      ASSERT_TRAP(header->freeSpaceLen, "disk allocation failed?");
      ret = freeSpaceLEA(--header->freeSpaceLen);
#if DEBUG
      auto iter = freeSpaceBitmap.find(ret);
      ASSERT_TRAP(iter != freeSpaceBitmap.end(), "allocated entity from free space queue not in bitmap ", ret);
      freeSpaceBitmap.erase(iter);
#endif
      link_t& l = allocatedLEA(ret);
      l.previous = header->allocatedLast;//might be NONE
      l.next = NONE;
      if(header->allocatedFirst == NONE) [[unlikely]] {//list was empty
	header->allocatedFirst = ret;
      } else {
	ASSERT_TRAP(header->allocatedLast != NONE, "root node in invalid state");
	link_t& oldLast = allocatedLEA(header->allocatedLast);
	ASSERT_TRAP(oldLast.next == NONE, "last node didn't know it was last.");
	oldLast.next = ret;
      }
      header->allocatedLast = ret;
      WITE_DEBUG_DB_HEADER;
#ifdef WITE_DEBUG_DB
      WARN("dbFile: ", filename, "Allocated: ", ret);
#endif
      return ret;
    };

    //NOTE: free might break an iterator (if the iteratee is freed)
    void free(uint64_t idx) {
      concurrentReadLock_write am(&allocationMutex);
      free_unsafe(idx);
    };

    void free_unsafe(uint64_t idx) {
#ifdef WITE_DEBUG_DB
      WARN("dbFile: ", filename, "Freeing: ", idx);
#endif
      ASSERT_TRAP(idx < blocks.size() * AU, "idx too big");
      WITE_DEBUG_DB_HEADER;
      freeSpaceLEA(header->freeSpaceLen) = idx;
      WITE_DEBUG_DB_FREESPACE(header->freeSpaceLen);
      header->freeSpaceLen++;
      ASSERT_TRAP(freeSpaceBitmap.emplace(idx).second, "duplicate entity found in free space queue ", idx);
      link_t& d = allocatedLEA(idx);
      if(d.next == NONE) [[unlikely]]
	header->allocatedLast = d.previous;
      else {
	allocatedLEA(d.next).previous = d.previous;
	WITE_DEBUG_DB_ALLOCATION(d.next);
      }
      if(d.previous == NONE) [[unlikely]]
	header->allocatedFirst = d.next;
      else {
	allocatedLEA(d.previous).next = d.next;
	WITE_DEBUG_DB_ALLOCATION(d.previous);
      }
      d.next = d.previous = NONE;
      WITE_DEBUG_DB_HEADER;
    };

    T& deref(uint64_t idx) {
      concurrentReadLock_read bm(&blocksMutex);
      return deref_unsafe(idx);
    };

    T& deref_unsafe(uint64_t idx) {
      ASSERT_TRAP(idx / AU < blocks.size(), "out of bounds: block does not exist");
      return blocks[idx / AU]->data[idx % AU];
    };

    inline T* get(uint64_t idx) {
      if(idx == NONE) return NULL;
      return &deref(idx);
    };

    void copy(const std::filesystem::path& dstFilename) {
      concurrentReadLock_read am(&allocationMutex);
      std::filesystem::copy_file(filename, dstFilename, std::filesystem::copy_options::overwrite_existing);
    };

    inline uint64_t first() {
      concurrentReadLock_read am(&allocationMutex);
      return first_unsafe();
    };

    inline uint64_t first_unsafe() {
      WITE_DEBUG_DB_HEADER;
      ASSERT_TRAP(header->freeSpaceLen <= blocks.size() * AU, "invalid header, pointer trouble?");
      return header->allocatedFirst;
    };

    inline uint64_t after(uint64_t id) {
      concurrentReadLock_read am(&allocationMutex);
      return after_unsafe(id);
    };

    inline uint64_t after_unsafe(uint64_t id) {
      WITE_DEBUG_DB_ALLOCATION(id);
      return allocatedLEA(id).next;
    };

    //NOTE: iterator_t i is invalidated if free(*i) is called
    class iterator_t {
    private:
      dbFile<T, AU>* dbf;
      uint64_t target;
    public:
      typedef int64_t difference_type;
      typedef std::forward_iterator_tag iterator_concept;

      iterator_t() : target(NONE) {};
      iterator_t(const iterator_t& o) = default;
      iterator_t(dbFile<T, AU>* dbf) : dbf(dbf), target(dbf->first()) {};
      iterator_t(dbFile<T, AU>* dbf, uint64_t t) : dbf(dbf), target(t) {};

      uint64_t operator*() const {
	return target;
      };

      iterator_t& operator++() {//prefix
	target = dbf->after(target);
	return *this;
      };

      iterator_t operator++(int) {//postfix
	iterator_t ret = *this;
	operator++();
	return ret;
      };

      inline bool operator==(const iterator_t& r) const {
	const iterator_t& l = *this;
	return (l.target == NONE && r.target == NONE) ||//because default-initialized is required
	  (l.dbf == r.dbf && l.target == r.target);
      };

      inline bool operator!=(const iterator_t& r) const {
	return !(*this == r);
      };

    };

    inline iterator_t begin() {
      return { this };
    };

    inline iterator_t end() {
      return {};
    };

    inline uint64_t capacity() {
      concurrentReadLock_read bm(&blocksMutex);
      return capacity_unsafe();
    };

    inline uint64_t capacity_unsafe() {
      return blocks.size() * AU;
    };

    inline uint64_t freeSpace() {
      WITE_DEBUG_DB_HEADER;
      return header->freeSpaceLen;
    };

    inline uint64_t size() {
      return capacity() - freeSpace();
    };

    inline uint64_t size_unsafe() {
      return capacity_unsafe() - freeSpace();
    };

  };

}


