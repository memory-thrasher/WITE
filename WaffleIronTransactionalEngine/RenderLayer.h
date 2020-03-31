#pragma once

#include "Renderer.h"

typedef uint8_t renderLayerIdx;
typedef uint64_t renderLayerMask;

class RenderLayer {
public:
	RenderLayer();
	~RenderLayer();
	renderLayerIdx idx;
private:
	SyncLock memberLock;
	std::vector<class Renderer*> members;
	friend class Renderer;
};

