#ifdef this_is_obsolete_but_backup

#include "stdafx.h"
#include "RollingBuffer.h"
#include "Export.h"

//no thread lock bc private, locked by owner
template<class T> T RollingBuffer::sub(size_t idx) {
	T ret;
	getByDepth((uint8_t*)&ret, sizeof(T), idx * (sizeof(T) + sizeof(header)) + sizeof(header));
	return ret;
}

//no thread lock bc private, locked by owner
template<class T> void RollingBuffer::put(T data, size_t idx) {
	putByDepth((uint8_t*)&data, sizeof(T), idx * (sizeof(T) + sizeof(header)) + sizeof(header));
}

uint16_t RollingBuffer::swallow(uint8_t* newData, size_t dataSize, uint64_t* handleOut, size_t pad) {
	if (TL && !lock->obtainWriteLock()) return RB_LOCK_FAILED;
	uint16_t ret = RB_SUCCESS;
	if (used() + dataSize > size) ret = resize(used() + dataSize + pad);
	if (!ret) ret = push(newData, dataSize, handleOut);
	if (TL) lock->releaseWriteLock();
	return ret;
}

RollingBuffer::RollingBuffer(bool TL, bool MAP, bool fixedSize, size_t size, uint8_t* data, size_t mapsize,
	uint8_t* mapdata) : TL(TL), MAP(MAP), FIXEDSIZE(fixedSize), size(size), head(0), tail(0), handleSeed(0),
		outer(false), PREALLOC(data), buf(PREALLOC ? data : static_cast<uint8_t*>(malloc(size))) {
	if (TL) lock = std::make_unique<DataLock>();
	if (MAP)
		map = std::make_unique<RollingBuffer>(false, false, fixedSize, mapsize * sizeof(mapEntry), mapdata);
}

RollingBuffer::~RollingBuffer() {
	if(!PREALLOC) free(buf);
}

inline bool RollingBuffer::wrapped() {
	return tail < head;
}

//no thread lock bc private, locked by owner
void RollingBuffer::getByDepth(uint8_t* data, size_t len, size_t depth) {
	size_t start = (head + depth) % size, end = (start + len) % size;
	if (end < start) {//wrap
		memcpy(data, buf + start, size - start);
		memcpy(data + size - start, buf, end);
	} else {
		memcpy(data, buf + start, len);
	}
}

//no thread lock bc private, locked by owner
void RollingBuffer::putByDepth(uint8_t* data, size_t len, size_t depth) {
	size_t start = (head + depth) % size, end = (start + len) % size;
	if (end < start) {//wrap
		memcpy(buf + start, data, size - start);
		memcpy(buf, data + size - start, end);
	} else {
		memcpy(buf + start, data, len);
	}
}

uint16_t RollingBuffer::get(uint8_t* data, size_t maxread, size_t handle, size_t* actualRead) {
	size_t read;
	struct mapEntry m;
	struct header h;
	if (!MAP) return RB_NOMAP;
	if (TL) if (!lock->obtainReadLock()) return RB_LOCK_FAILED;
	m = map->sub<struct mapEntry>(0);
	if (handle > m.handle)
		m = map->sub<struct mapEntry>(handle - m.handle);
	getByDepth((uint8_t*)(&h), sizeof(struct header), m.offset);
	read = glm::min<size_t>(maxread, h.size);
	getByDepth(data, read, m.offset + sizeof(header));
	if (TL) lock->releaseReadLock();
	if (actualRead) *actualRead = read;
	return RB_SUCCESS;
}

size_t RollingBuffer::used() {
	size_t ret;
	if (TL) if (!lock->obtainReadLock()) return RB_LOCK_FAILED;
	ret = head == tail ? (outer ? size : 0) : head < tail ? tail - head : tail + size - head;
	if (TL) lock->releaseReadLock();
	return ret;
}

size_t RollingBuffer::count() {
	size_t i, ret = 0;
	if (MAP) {
		struct header h;
		for (i = 0;i < used();i += h.size + sizeof(struct header)) {
			getByDepth((uint8_t*)&h, sizeof(header), i);
			ret++;
		}
	} else {
		ret = map->used() / (sizeof(struct header) + sizeof(struct mapEntry));
	}
	return ret;
}

uint16_t RollingBuffer::push(
	uint8_t* newData, size_t dataSize, uint64_t* handleOut) {
	if (TL) if (!lock->obtainWriteLock()) return RB_LOCK_FAILED;
	size_t newTail = tail + dataSize + sizeof(struct header);
	uint16_t ret = RB_SUCCESS;
	struct header h;
	struct mapEntry m;
	bool rolled = newTail >= size;
	if (rolled) newTail -= size;
	if (used() + dataSize + sizeof(struct header) <= size) {
		if (MAP) {
			m.handle = handleSeed++;
			if (handleOut) *handleOut = m.handle;
			m.offset = tail;
			map->swallow((uint8_t*)&m, sizeof(m), NULL,
				(sizeof(struct header) + sizeof(struct mapEntry)) * 4);
		}
		h.size = dataSize;
		if (rolled) {//TODO if this doesn't work, just use putByDepth
			if (size - tail < sizeof(struct header)) {
				memcpy(buf + tail, &h, size - tail);
				memcpy(buf, (uint8_t*)&h + sizeof(struct header), sizeof(struct header) - size + tail);
				memcpy(buf + sizeof(struct header) - size + tail, newData, newTail);
			} else {
				memcpy(buf + tail, &h, sizeof(struct header));
				memcpy(buf + tail + sizeof(struct header), newData, (size - tail - sizeof(struct header)));
				memcpy(buf, newData + size - tail - sizeof(struct header), newTail);
			}
		} else {
			memcpy(buf + tail, &h, sizeof(struct header));
			memcpy(buf + tail + sizeof(struct header), newData, dataSize);
		}
		tail = newTail;
		outer = tail == head;
	} else
		ret = RB_BUFFER_OVERFLOW;
	if (TL) lock->releaseWriteLock();
	return ret;
}

uint16_t RollingBuffer::resize(size_t newSize) {
	if (FIXEDSIZE) return RB_NOMEM;
	if (TL) if (!lock->obtainWriteLock()) return RB_LOCK_FAILED;
	uint16_t ret = RB_SUCCESS;
	size_t newTail;
	if (newSize > size) {
		uint8_t* tempBuf = (uint8_t*)realloc(buf, newSize);
		if (!tempBuf) {
			ret = RB_NOMEM;
			goto resizecleanup;
		}
		buf = tempBuf;
		if (tail < head || (tail == head && outer)) {
			size_t delta = newSize - size;
			if (delta > tail) {//buffer did wrap, but now does not
				newTail = tail + size;
				memcpy(buf + size, buf, tail);
			} else {
				newTail = tail - delta;
				memcpy(buf + size, buf, tail - newTail);
				memmove(buf, buf + tail - newTail, newTail);
			}
			if (MAP) {
				struct mapEntry m;
				for (size_t i = sizeof(struct header);i < map->used();
					i += sizeof(struct mapEntry) + sizeof(struct header)) {
					map->getByDepth((uint8_t*)&m, sizeof(struct mapEntry), i);
					if (m.offset < tail) {
						m.offset = (m.offset + newSize - delta) % newSize;
						map->putByDepth((uint8_t*)&m, sizeof(struct mapEntry), i);
					}
				}
			}
			tail = newTail;
			outer = false;
		}
	}
	else if (newSize != size) {//shrink; infrequent if ever
		size_t c = used(), newHead, from, to, len;
		if (newSize < c) {
			ret = RB_BUFFER_OVERFLOW;
			goto resizecleanup;
		}
		outer = newSize == c;
		newTail = tail;
		newHead = head;
		if (tail >= newSize && head >= newSize) {
			//entire active region is in the discard region (or else the size test would've failed)
			newHead = 0;
			newTail = c % newSize;
			memcpy(buf + newHead, buf + head, c);
			len = c;
			to = newHead;
			from = head;
		} else if (tail >= newSize) {
			//region did not wrap, but now will, move the tail end to the buffer beginning
			newTail = tail - newSize;
			len = tail - newSize + 1;
			to = 0;
			from = newSize;
			memcpy(buf, buf + newSize, len);
		} else if (head > tail || (head == tail && outer)) {
			//region did wrap, either (beginning in) or (using the entirety of) the discard area
			newHead = head - (size - newSize);
			len = size - head;
			from = head;
			to = newHead;
			memmove(buf + newHead, buf + head, len);
		} //else entire active region is in the keep area, no moves needed
		buf = static_cast<uint8_t*>(realloc(buf, newSize));
		if (MAP) {
			struct mapEntry m;
			for (size_t i = sizeof(struct header);i < map->used();
				i += sizeof(struct mapEntry) + sizeof(struct header)) {
				map->getByDepth((uint8_t*)&m, sizeof(struct mapEntry), i);
				if (m.offset >= from && m.offset < from + len) {
					m.offset = m.offset + to - from;
					map->putByDepth((uint8_t*)&m, sizeof(struct mapEntry), i);
				}
			}
		}
		tail = newTail;
		head = newHead;
#ifdef _DEBUG
		if (outer && head != tail) LOG("WARN: RollingBuffer shrink size fail");
#endif
	}
	size = newSize;
resizecleanup:
	if (TL) lock->releaseWriteLock();
	return ret;
}

uint16_t RollingBuffer::pop(
	uint8_t* data, size_t* starts, size_t maxsize, size_t max) {
	if (TL) if (!lock->obtainWriteLock()) return RB_LOCK_FAILED;
	uint16_t ret = RB_SUCCESS;
	if (used()) {
		struct header h;
		getByDepth((uint8_t*)&h, sizeof(struct header), 0);
		while (h.size <= maxsize && max) {
			if (data) {
				getByDepth(data, h.size, sizeof(struct header));
				data += h.size;
			}
			maxsize -= h.size;
			max--;
			head = (head + sizeof(struct header) + h.size) % size;
			outer = false;
			if (starts) {
				*starts = head;
				starts++;
			}
			if (MAP) map->pop(NULL, NULL, sizeof(struct mapEntry), 1);
		}
	} else
		ret = RB_BUFFER_UNDERFLOW;
	if (TL) lock->obtainWriteLock();
	if (starts) *starts = 0;//null term
	return ret;
}

std::unique_ptr<URollingBuffer> URollingBuffer::make(bool TL, bool MAP, size_t size, size_t mapsize) {
	return std::make_unique<RollingBuffer>(TL, MAP, size, mapsize);
}
#endif