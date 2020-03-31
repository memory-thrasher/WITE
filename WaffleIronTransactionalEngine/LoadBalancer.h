#pragma once

#include "RenderLayer.h"
#include "Database.h"

//TODO define pad size for subbuffers?

static class LoadBalancer {
public:
	//static std::unique_ptr<VertexBuffer> allocateVB();
	static void mainLoop();
private:
	LoadBalancer() {};
	~LoadBalancer() = delete;
	static Database db;//this is backed by a file, all else is volatile
};

