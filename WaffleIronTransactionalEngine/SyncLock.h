#pragma once

#include "stdafx.h"
#include "Export.h"

class SyncLock : public USyncLock {
public:
	SyncLock();
	~SyncLock();
	void WaitForLock();
	void ReleaseLock();
	uint64_t getId();
	bool isLocked();
	inline void yield();
private:
	typedef std::atomic_uint64_t mutex_t;
	static mutex_t seed;
	uint64_t id, holds;
	mutex_t holder, queueSeed, queueCurrent;
};

class ScopeLock : public UScopeLock {
public:
	inline void yield();
	ScopeLock(const USyncLock const * lock);
	~ScopeLock();
private:
	const USyncLock *lock;
};
