#pragma once

#include "stdafx.h"

//class TrackedResource {
//public:
//	TrackedResource(uint8_t* resource, size_t resourceSize, size_t initialLogSize = 1);
//	~TrackedResource();
//	void snapshot(uint8_t* data, size_t idx = -1, size_t offset = 0, size_t size = -1);
//	void beginWriteLock();
//	void push(uint8_t* data, size_t offset = 0, size_t size = -1);
//	void commitWrite();
//	void rollbackWrite();
//	/*attempts to apply the given number of snapshots. Does not block. True if successful and complete.*/
//	static bool apply(size_t snapshots);
//private:
//	//static const SyncLock* creationLock;
//	class DataLock lock;//transient
//	RollingBuffer<false, false> log;//volatile
//	size_t resourceSize;
//	uint8_t* resource;//serialize as relative to master table
//};
