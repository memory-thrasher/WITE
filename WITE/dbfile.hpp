#pragma once

#include <string>
#include <cstring> //memcpy
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <filesystem>
//TODO see if these are all needed

#inlucde "syncLock.hpp"
#include "DEBUG.hpp"

namespace WITE {

  template<class T, size_t AU>//allocation units, number of T per file growth. sizeof(T)*AU is minimum file size and the increment
  class dbfile {

  private:
    struct header_t {
      uint64_t freeSpceLen;//lifo queue position
    };
    struct au_t {
      uint64_t freeSpace[AU];//lifo queue space
      T data[AU];
    };
    static constexpr size_t au_size = sizeof(au_t);
    static constexpr uint8_t plug[au_size] = { 0 };

    syncLock fileMutex, blocksMutex, freeSpaceMutex;
    header_t* header;
    std::vector<au_t*> blocks;//these are mmaped regions (or subdivisions thereof)
    int fd;
    const std::string filename;
#if DEBUG
    std::set<uint64_t> freeSpaceBitmap;//sanity check for debugging only, duplicates the on-disk allocation queue
#endif

    void initialize(uint64_t auId) {
      ASSERT_TRAP(header->freeSpaceLen <= auId * AU, "queue position invalid before initializing new allocation unit");
      uint64_t base = auId * au_size;
      for(uint32_t i = 0;i < au_size;i++) {
	freeSpaceLEA(header->freeSpaceLen++) = base + i;
#if DEBUG
	ASSERT_TRAP(freeSpaceBitmap.emplace(i).second, "duplicate entity found in free space queue");
#endif
      }
    };

    inline uint64_t& freeSpaceLEA(uint64_t idx) {
      return blocks[idx / AU]->freeSpace[idx % AU];
    };

  public:
    dbfile() = delete;
    dbfile(dbfile&&) = delete;

    dbfile(const std::string fn, bool clobber) : filename(fn) {
      scopeLock fl(&fileMutx), bm(&blocksMutex), fsm(&freeSpaceMutex);
      fd = open(filename.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW | O_LARGEFILE | (clobber ? O_TRUNC : 0), 0660);
      ASSERT_TRAP(fd, "failed to open file errno: ", errno);
      ASSERT_TRAP(flock(fd, LOCK_EX | LOCK_NB) == 0, "failed to lock file ", filename, " with errno: ", errno); //lock will be closed when fd is closed
      stat fst;
      size_t size = 0;
      ASSERT_TRAP(fstat(fd, &fst) == 0, "file creation failed");
      ASSERT_TRAP((fst.st_mode & S_IFMT) == S_IFREG, "attempted to open a file that exists but is not a regular file");
      size = static_cast<size_t>(fst.off_t);
      bool load = size;
      size_t existingAUs = size ? (size - sizeof(header_t)) / au_size : 0;
      if(existingAUs) [[likely]] {//load
	ASSERT_TRAP((size - sizeof(header_t)) % au_size == 0, "attempted to load file with invalid size");
	lseek(fd, 0, SEEK_END);
      } else {//init
	header_t temph;
	write(fd, temph, sizeof(header_t));
	write(fd, plug, au_size);
	size = au_size;
      }
      void* mm = mmap(NULL, size, PROTO_READ | PROTO_WRITE, MAP_SHARED, fd, 0);
      ASSERT_TRAP(mm, "mmap fail", errno);
      header = reinterpret_cast<header_t*>(mm);
      blocks.emplace_back(reinterpret_cast<au_t*>(mm + sizeof(header_t)));
      if(existingAUs) [[likely]] {
#if DEBUG
	ASSERT_TRAP(header->freeSpaceLen <= initialTCount);
	for(uint64_t i = 0;i < header->freeSpaceLen;i++) {
	  uint64_t j = freeSpaceLEA(i);
	  ASSERT_TRAP(freeSpaceBitmap.emplace(j).second, "duplicate entity found in free space queue");
	}
#endif
	//if the file contains multiple allocation units, then those all share one large mmap, but still populate `blocks` with portions of that mmap rather than complicate the logic of deciding which map to use
	for(uint64_t i = 1;i < existingAUs;i++)
	  blocks.emplace_back(&blocks[0] + i);
      } else {//initialize file contents
	initialize(0);
      }
    };

    uint64_t allocate() {
      scopeLock fsm(&freeSpaceMutex);
      uint64_t ret;
      if(!freeSpaceLen) [[unlikely]] {
	scopeLock fl(&fileMutx), bm(&blocksMutex);
	uint64_t auId = blocks.size();
	write(fd, plug, au_size);
	void* mm = mmap(NULL, au_size, PROTO_READ | PROTO_WRITE, MAP_SHARED, fd, auId * au_size);
	ASSERT_TRAP(mm, "mmap fail", errno);
	blocks.emplace_back(reinterpret_cast<au_t*>(mm));
	initialize(auId);
      }
      ASSERT_TRAP(header->freeSpaceLen, "disk allocation failed?");
      ret = freeSpaceLEA(header->freeSpaceLen--);
#if DEBUG
      auto iter = freeSpaceBitmap.find(ret);
      ASSERT_TRAP(iter != freeSpaceBitmap.end(), "duplicate entity found in free space queue");
      freeSpaceBitmap.erase(iter);
#endif
      return ret;
    };

    void free(uint64_t idx) {
      scopeLock fsm(&freeSpaceMutex);
      ASSERT_TRAP(idx < blocks.size() * AU, "idx too big");
      freeSpaceLEA(freeSpaceLen++) = idx;
      ASSERT_TRAP(freeSpaceBitmap.emplace(idx).second, "duplicate entity found in free space queue");
    };

    T* deref(uint64_t idx) {
      scopeLock bm(&blocksMutex);
      return &blocks[idx / AU]->data[idx % AU];
    };

    void copy(std::string dstFilename) {
      scopeLock fsm(&freeSpaceMutex);
      std::filesystem::copy_file(filename, dstFilename, std::filesystem::copy_options::overwrite_existing);
    };

  };

}


