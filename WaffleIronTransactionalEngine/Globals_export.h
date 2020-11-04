#pragma once

namespace WITE {

  class Window;
  class Camera;
  class Object;
  class Database;
  class GPU;
  class MeshSource;
  class Mesh;
  class Renderer;
  class ShaderResource;
  class Shader;
  class StaticMesh;
  class SyncLock;
  class ScopeLock;
  class Thread;
  class Time;
  class Transform;
  class BBox3D;
  typedef uint8_t renderLayerIdx;
  typedef uint64_t renderLayerMask;

#define __offsetof_array(_type, _array, _idx) (offsetof(_type, _array) + sizeof(WITE::remove_array<decltype(_type::_array)>::type) * _idx)
#define ERRLOGFILE WITE::getERRLOGFILE()
#define LOG(message, ...) { ::fprintf(ERRLOGFILE, "%s:%d: ", __FILE__, __LINE__); ::fprintf(ERRLOGFILE, message, ##__VA_ARGS__); WITE::flush(ERRLOGFILE); }
#ifdef _DEBUG
#define CRASHRET(...) { LOG("**CRASH**\n"); auto db = database; if(db) db->gracefulStop(); abort(); exit(1); return __VA_ARGS__; }
#else
#define CRASHRET(...) { LOG("**CRASH**\n"); auto db = database; if(db) db->gracefulStop(); exit(1); return __VA_ARGS__; }
#endif
#define CRASHRETLOG(ret, ...) { LOG(__VA_ARGS__); CRASHRET(ret); }
#define CRASH(message, ...) { LOG(message, ##__VA_ARGS__); CRASHRET(); } //all crashes should explain themselves
#define CRASHIFFAIL(_cmd_, ...) {int64_t _res = (int64_t)_cmd_; if(_res) { LOG("Got result: %I64d\n", _res); CRASHRET(__VA_ARGS__) } }//for VkResult. VK_SUCCESS = 0
#define CRASHIFWITHERRNO(_cmd_, ...) { if(_cmd_) { auto _en = errno; LOG("Got errno: %d\n", _en); CRASHRET(__VA_ARGS__) } }
#define LOGMAT(mat, name) LOG("%s:\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n", name,\
                  mat [0][0], mat [1][0], mat [2][0], mat [3][0],\
                  mat [0][1], mat [1][1], mat [2][1], mat [3][1],\
                  mat [0][2], mat [1][2], mat [2][2], mat [3][2],\
                  mat [0][3], mat [1][3], mat [2][3], mat [3][3])

#define TIME(cmd, level, ...) if(DO_TIMING_ANALYSIS >= level) {uint64_t _time = WITE::Time::nowNs(); cmd; _time = WITE::Time::nowNs() - _time; LOG(__VA_ARGS__, _time);} else {cmd;}
#define debugMode (WITE::getDebugMode())

  template<class T> struct remove_array { typedef T type; };
  template<class T> struct remove_array<T[]> { typedef T type; };
  template<class T, size_t N> struct remove_array<T[N]> { typedef T type; };
  template<class T> struct remove_array_deep { typedef T type; };
  template<class T> struct remove_array_deep<T[]> { typedef wintypename remove_array_deep<T>::type type; };
  template<class T, size_t N> struct remove_array_deep<T[N]> { typedef wintypename remove_array_deep<T>::type type; };

  export_dec void WITE_INIT(const char* gameName, uint64_t debugFlags);
  export_dec FILE* file_open(const char* path, const char* mode);
  export_dec uint64_t timeNs();
  export_dec void flush(FILE* file);
  export_dec FILE* getERRLOGFILE();
  export_dec void sleep(uint64_t us);
  export_dec bool loadFile(const char* filename, size_t &len, const unsigned char** dataout);
  export_dec bool endsWith(const char* str, const char* suffix);
  export_dec bool startsWith(const char* str, const char* prefix);
  export_dec uint64_t getFormat(size_t components, size_t componentSize, size_t componentType);
  export_dec size_t getFileSize(const char* path);//0 for does not exist
  export_dec bool strcmp_s(const std::string& a, const std::string& b);
  export_dec void enterMainLoop();//this call does not return. Set up type registration (with callbacks) first.
#define database (*WITE::get_database())
  export_dec WITE::Database** get_database();
  export_dec uint64_t getDebugMode();
  export_dec void gracefulExit();//returns quickly, stops app gracefully after frame completion and db sync.

  template <typename _T, typename _Alloc = std::allocator<_T>>
  void vectorPurge(std::vector<_T, _Alloc>* v, _T o) {//Not thread safe. sync externally
    auto it = v->begin();
    while(it != v->end())
      if(o == *it)
        it = v->erase(it);
      else
        it++;
  }

  template <typename _T, typename _Alloc = std::allocator<_T>>
  bool vectorContains(std::vector<_T, _Alloc>* v, _T o) {//Not thread safe. sync externally
    auto it = v->begin();
    while(it != v->end())
      if(o == *it)
        return true;
    return false;
  }

}
