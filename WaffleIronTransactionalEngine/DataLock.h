#pragma once

#include "stdafx.h"
#include "Thread.h"

class DataLock : public UDataLock {
public:
	DataLock();
	~DataLock();
	void obtainReadLock();
	void releaseReadLock();
	void obtainWriteLock();
	void releaseWriteLock();
	//TODO debug only list of tids of lockers and validate release,escalate,drop
private:
	SyncLock stateLock;
	size_t readlocks;
	ThreadResource<size_t> readLocksPerThread;
};

class ScopeReadLock : public UScopeReadLock {
public:
	ScopeReadLock(UDataLock const * lock);
	~ScopeReadLock();
private:
	UDataLock * lock;
};

class ScopeWriteLock : public UScopeWriteLock {
public:
	ScopeWriteLock(UDataLock const * lock);
	~ScopeWriteLock();
private:
	UDataLock * lock;
};
