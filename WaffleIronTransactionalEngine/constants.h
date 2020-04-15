#pragma once

#if defined(_DEBUG)
#define LAYER_COUNT 1
static const char* LAYERS[] = { "VK_LAYER_LUNARG_standard_validation" };
#else
#define LAYER_COUNT 0
const char* LAYERS[] = {};
#endif

#define NS_PER_MS 1000000
#define BASIC_LOCK_TIMEOUT 100000
#define STATELOCK_TIMEOUT 10000
#define FILEIO_TIMEOUT 10000000 //10 seconds
#define FOREVER_TIMEOUT UINT32_MAX
#define NS_PER_SECOND (NS_PER_MS * 1000)
#define RB_SUCCESS 0
#define RB_BUFFER_UNDERFLOW 1
#define RB_BUFFER_OVERFLOW 2
#define RB_OUT_OF_BOUNDS 3
#define RB_NOMEM 4
#define RB_READ_INCOMPLETE 5
#define RB_LOCK_FAILED 6
#define RB_NOMAP 7
#define WARM_THREADS 8
#define MAX_THREADS 256
#define FLOAT_BYTES 4//gpu
#define SIZEOF_VERTEX (6 * FLOAT_BYTES)//6 floats, 3d vert + either 2d uv or 3d alg coords
#define MAX_RENDER_LAYERS 8
#define MAX_SUBSHADERS 4
//these must match the corrosponding vk_descriptor_type_*
#define SHADER_RESOURCE_UNIFORM 6 //uniform_buffer
#define SHADER_RESOURCE_SAMPLED_IMAGE 1 //combined_image_sampler, **each instance gets its own copy/allocation.
//and these corrospond to the vk_shader_stage_*_bit
#define SHADER_STAGE_VERT 1
#define SHADER_STAGE_FRAG 16
//VkFormat

namespace WITE {
  template<class T> struct remove_array { typedef T type; };
  template<class T> struct remove_array<T[]> { typedef T type; };
  template<class T, size_t N> struct remove_array<T[N]> { typedef T type; };
}

#define __offsetof_array(_type, _array, _idx) (offsetof(_type, _array) + sizeof(WITE::remove_array<decltype(_type::_array)>::type) * _idx)
#define ERRLOGFILE WITE::getERRLOGFILE()
#define LOG(message, ...) { ::fprintf(ERRLOGFILE, "%s:%d: ", __FILE__, __LINE__); ::fprintf(ERRLOGFILE, message, ##__VA_ARGS__); }
// #ifdef _DEBUG
#define CRASHRET(...) { LOG("**CRASH**"); WITE::flush(ERRLOGFILE); exit(1); return __VA_ARGS__; }
// #else
// #define CRASHRET(...) { LOG("**CRASH IMMINENT**"); WITE::flush(ERRLOGFILE); return __VA_ARGS__; }
// #endif
#define CRASH(message) { LOG(message); CRASHRET(); }
#define CRASHIFFAIL(_cmd_, ...) {int64_t _res = (int64_t)_cmd_; if(_res) { LOG("Got result: %I64d\n", _res); CRASHRET(__VA_ARGS__) } }//for VkResult. VK_SUCCESS = 0

#define newOrHere(_out) new(ensurePointer(_out))

#ifdef _WIN32
#define export_dec
#define export_def __declspec(dllexport)
#define wintypename typename
#else
//TODO
#define wintypename
#endif

template<class _T> inline _T* ensurePointer(_T* out) {
	if (!out) out = static_cast<_T*>(malloc(sizeof(_T)));
	return out;
}
