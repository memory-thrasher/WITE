#include "stdafx.h"
#include "Thread.h"
#include "Export.h"

uint32_t Thread::seed = 0;
ThreadResource<Thread*> Thread::threads;
SyncLock Thread::lock;

#ifdef _WIN32

DWORD dwTlsIndex;

void Thread::init() {//static
	if (seed.load() == 0) {
		dwTlsIndex = TlsAlloc();
	}
	initThisThread();
}

Thread* Thread::current() {
	LPVOID ret = TlsGetValue(dwTlsIndex);
	if (!ret) CRASHRET(NULL);
	return static_cast<Thread*>(ret);
}

void putThreadRef(Thread* ref) {
	if (!TlsSetValue(dwTlsIndex, ref)) CRASH;
}

DWORD WINAPI winThreadCallback(_In_ LPVOID lpParam) {
	Thread::initThisThread(static_cast<Thread::threadEntry_t>(lpParam));
	return 0;
}

void Thread::spawnThread(threadEntry_t entry) {//static
	CreateThread(NULL, 0, winThreadCallback, entry, 0, NULL);
}

#endif

void Thread::initThisThread(threadEntry_t entry) {//static  entry defaults to null
	uint32_t tid = seed.fetch_add(1, std::memory_order_relaxed);
	Thread* ret = new Thread(entry, tid), **storage;
	storage = threads.get(tid);
	if (*storage) delete *storage;
	*storage = ret;
	putThreadRef(ret);
	if (ret->entry) ret->entry->call();
}

uint32_t Thread::getTid() {
	return tid;
}

uint32_t Thread::getCurrentTid() {
	return current()->getTid();
}

Thread::Thread(threadEntry_t entry, uint32_t id) : entry(entry), tid(id) {}

Thread::~Thread() {}

