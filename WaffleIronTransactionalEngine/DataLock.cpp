#include "stdafx.h"
#include "DataLock.h"
#include "Export.h"

DataLock::DataLock() : readlocks(0) {}
DataLock::~DataLock() {}

void DataLock::obtainReadLock() {
	(*readLocksPerThread.get())++;
	ScopeLock sl(&stateLock);
	readlocks++;
}

void DataLock::releaseReadLock() {
	//if (!readlocks) CRASH;
	(*readLocksPerThread.get())--;
	ScopeLock sl(&stateLock);
	readlocks--;
}

void DataLock::obtainWriteLock() {
	stateLock.WaitForLock();
	while (readlocks > *readLocksPerThread.get()) stateLock.yield();
}

void DataLock::releaseWriteLock() {
	stateLock.ReleaseLock();
}

ScopeReadLock::ScopeReadLock(UDataLock * sl) {
	sl->obtainReadLock();
	lock = sl;
}

ScopeReadLock::~ScopeReadLock() {
	if (lock) lock->releaseReadLock();
	lock = NULL;
}

ScopeWriteLock::ScopeWriteLock(UDataLock * sl) {
	sl->obtainWriteLock();
	lock = sl;
}

ScopeWriteLock::~ScopeWriteLock() {
	if (lock) lock->releaseWriteLock();
	lock = NULL;
}

std::unique_ptr<UDataLock> UDataLock::make() {
	return std::make_unique<DataLock>();
}

std::unique_ptr<UScopeReadLock> UScopeReadLock::make(UDataLock * lock, uint64_t timeoutNs) {
	return std::make_unique<ScopeReadLock>(lock, timeoutNs);
}

std::unique_ptr<UScopeWriteLock> UScopeWriteLock::make(UDataLock * lock, uint64_t timeoutNs) {
	return std::make_unique<ScopeWriteLock>(lock, timeoutNs);
}

