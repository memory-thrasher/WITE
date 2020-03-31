#include "stdafx.h"
#include "LoadBalancer.h"


std::unique_ptr<LoadBalancer::VertexBufferRange> LoadBalancer::allocateVB() {
	ScopeLock lock(&lockSubbuffers);
	size_t i, iStart = i = allocationCursor, size = subbuffers.size();
	SubVertexBuffer* raw = subbuffers.data();//no
	while (i < size) if (!raw[i].allocated) goto found; else i++;
	i = 0;
	while (i < iStart) if (!raw[i].allocated) goto found; else i++;
	subbuffers.resize(subbuffers.capacity() + 10000);
	i = size;
found:
	LoadBalancer::VertexBufferRange* ret = new(&subbuffers[i])LoadBalancer::VertexBufferRange(i);
	return std::unique_ptr<LoadBalancer::VertexBufferRange>(ret);
}
