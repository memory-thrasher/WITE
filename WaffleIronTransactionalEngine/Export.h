#pragma once

#include <atomic>
#include <stdint.h>
#include <limits>
#include <vector>
#include <string>
#include <map>
#include <type_traits>
#include <iterator>
#include <memory>

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

#include "constants.h"

#if !(defined(_RELEASE) || defined(_DEBUG))
#define _DEBUG
#endif
#ifndef DO_TIMING_ANALYSIS
#define DO_TIMING_ANALYSIS 0
#endif

#ifdef _WIN32
#define export_dec __declspec(dllexport)
#define export_def __declspec(dllexport)
#define wintypename typename
#else
//TODO
#define wintypename
#endif

#define export_var export_def

#include "Globals_export.h"
#include "CallbackFactory_export.h"
#include "Thread_export.h"
#include "RollingBuffer_export.h"
#include "SyncLock_export.h"
#include "WMath_export.h"
#include "Transform_export.h"
#include "Database_export.h"
#include "Object_export.h"
#include "ShaderResource_export.h"
#include "Shader_export.h"
#include "MeshSource_export.h"
#include "StaticMesh_export.h"
#include "Mesh_export.h"
#include "GPU_export.h"
#include "Renderer_export.h"
#include "Camera_export.h"
#include "Window_export.h"
#include "Time_export.h"
