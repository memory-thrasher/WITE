#pragma once

#include <vector>
#include <algorithm>
#include <cstring> //memcpy
#include <string>
#include <map>
#include <atomic>

namespace WITE {

  template<typename T> inline void remove(std::vector<T>& t, T p) {
    t.erase(std::remove(t.begin(), t.end(), p), t.end());
  }

  template<typename T, class UP> inline void remove_if(std::vector<T>& t, UP p) {
    t.erase(std::remove_if(t.begin(), t.end(), p), t.end());
  }

  template<typename T, class C> inline constexpr bool contains(C& t, T e) {
    for(auto v : t)
      if(v == e)
	return true;
    return false;
  }

  template<class C> constexpr inline void concat(C& dst, const C& src) {
    dst.reserve(dst.size() + src.size());
    for(auto i = src.begin();i != src.end();i++)
      dst.push_back(*i);
  }

  template<class C> inline void concat_move(C& dst, C& src) {
    dst.reserve(dst.size() + src.size());
    for(auto i = src.begin();i != src.end();i++)
      dst.push_back(std::move(*i));
    src.clear();
  }

  template<size_t CNT> std::string&& concat(std::string& strings[CNT]) {
    std::string ret;
    size_t totalLen = 0;
    for(std::string& s : strings)
      totalLen += s.size();
    ret.reserve(totalLen);
    for(std::string& s : strings)
      ret.append(s);
    return st::move(ret);
  }

  template<class T, class U, class V> void uniq(T src, U p, V& out) {
    for(auto it = src.begin();it != src.end();it++) {
      auto u = p(*it);
      if(!contains(out, u))
	out.push_back(u);
    }
  }

  template<class T, class K, class V> bool collectionIntersectsMap(const T& col, const std::map<K, V>& map) {
    auto iter = col.begin(), end = col.end();
    while(iter++ != end)
      if(map.contains(*iter))
	return true;
    return false;
  };

  inline void memcpy(uint8_t* dst, const uint8_t* src, size_t len) {
    std::memcpy(reinterpret_cast<void*>(dst), reinterpret_cast<const void*>(src), len);
  }

  template<typename T> inline void cpy(T* dst, const T* src, size_t len)
    requires std::negation_v<std::is_volatile<T>> {
    std::memcpy(reinterpret_cast<void*>(dst), reinterpret_cast<const void*>(src), len * sizeof(T));
  }

  template<typename D, typename S> inline void memcpy(D& dst, const S& src)
    requires std::negation_v<std::is_volatile<D>> {
    static_assert(sizeof(D) >= sizeof(S));
    std::memcpy(reinterpret_cast<void*>(&dst), reinterpret_cast<const void*>(&src), sizeof(S));
  }

  template<typename S> inline void memcpy(void* dst, const S& src)
    requires std::negation_v<std::is_volatile<S>> {
    std::memcpy(dst, reinterpret_cast<const void*>(&src), sizeof(S));
  }

  template<typename D> inline void memcpy(D& dst, const void* src)
    requires std::negation_v<std::is_volatile<D>> {
    std::memcpy(reinterpret_cast<void*>(&dst), src, sizeof(D));
  }

  // template<typename D, typename S> inline void memcpy(D* dst, const S* src, size_t len)
  //   requires std::negation_v<std::is_volatile<D>> {
  //   std::memcpy(reinterpret_cast<void*>(dst), reinterpret_cast<const void*>(src), len);
  // }

  // template<typename D, typename S> inline void memcpy(D* dst, const S* src, size_t len)
  //   requires std::is_volatile_v<D> {
  //   for(size_t i = len;i--;) {
  //     dst[i] = src[i];
  //   }
  // }

  template<typename D, typename S> inline auto memcmp(const D* dst, const S* src, size_t len) {
    return std::memcmp(reinterpret_cast<const void*>(dst), reinterpret_cast<const void*>(src), len);
  }

  template<typename T> inline void memset(T* dst, const char value, const size_t len) {
    std::memset(reinterpret_cast<void*>(dst), value, len);
  }

  template<size_t cnt, typename T> inline void memset(T* dst, const T value) {
    if constexpr(cnt == 1)
      *dst = value;
    else if constexpr(cnt > 1) {
      constexpr size_t m = (cnt+1)>>1;
      memset<m, T>(dst, value);
      cpy(dst+m, dst, (cnt-m));
    }
  }

  template<typename T, size_t L> inline void memcpy(T* out, T(&in)[L]) {
    memcpy(out, &in, L * sizeof(T));
  }

  template<typename T, size_t L> inline void memcpy(T(&out)[L], T* in) {
    memcpy(&out, in, L * sizeof(T));
  }

  template<typename T, size_t L> inline void memcpy(T* out, const T(&in)[L]) {
    memcpy(out, &in, L * sizeof(T));
  }

  template<typename T, size_t L> inline void memcpy(T(&out)[L], const T* in) {
    memcpy(&out, in, L * sizeof(T));
  }

  // template<typename CVT, typename T = std::remove_cv_t<CVT>> inline T cv_cast(CVT i) {
  //   return const_cast<T>(i);
  // }

  template<typename CVT, typename T = std::remove_cv_t<CVT>> inline T* cv_remove(CVT* i) {
    return const_cast<T*>(i);
  }

  template<typename CVT, typename T = std::remove_cv_t<CVT>>
  inline T&& cv_remove(CVT i) requires std::is_trivially_copyable_v<T> {
    T ret = i;
    return std::move(ret);
  }

  template<typename T> [[nodiscard]] inline constexpr T* calloc(size_t cnt) {
    std::allocator<T> a;
    return std::allocator_traits<std::allocator<T>>::allocate(a, cnt);
    //return reinterpret_cast<T*>(std::malloc(sizeof(T) * cnt));
  }

  //I and O are either iterators or pointers
  template<class I, class O, class Alloc = std::allocator<std::remove_reference_t<decltype(*O())>>>
  constexpr O const_copy(I start, I end, O out, Alloc alloc = Alloc()) {
    auto o = out;
    while(start != end)
      std::allocator_traits<Alloc>::construct(alloc, &*o++, *start++);//constructor or assignment call
    return out;
  }

  template<class U, class T> concept is_collection_of = requires(U u) { {*u.begin()} -> std::convertible_to<T>; };

  template<class T> concept collection = requires(T t) { *t.begin(); };

  template<class U> concept multidimensionalCollection = requires(U u) { {*u.begin()->begin()}; };

  // template<class T> struct beginAlias {
  //   auto operator()(T t) { return t.begin(); };
  // };

  // template<class T> struct remove_collection {
  //   typedef T type;
  // };

  // template<collection T> struct remove_collection<T> {
  //   typedef remove_collection<typename std::remove_ref<typename std::invoke_result<beginAlias<T>, T>::type>::type>::type type;
  // };

  // template<class U, class T> concept is_collection_of_recursive = std::convertible_to<typename remove_collection<U>::type, T>;

  template<class T> requires std::is_pointer<T>::value inline T auto_cast(T* i) { return i; };
  template<class T> requires std::is_pointer<T>::value inline T auto_cast(T& i) { return &i; };
  template<class T, class U> requires std::is_pointer<T>::value inline T auto_cast(U* i) { return reinterpret_cast<T>(i); };
  template<class T, class U> requires std::is_pointer<T>::value inline T auto_cast(U& i) { return reinterpret_cast<T>(&i); };
  // template<std::copy_constructible T, std::copy_constructible U> requires requires {
  //   !std::is_pointer<U>::value;
  //   !std::is_reference<U>::value;
  //   !std::is_pointer<T>::value;
  //   !std::is_reference<T>::value;
  // }
  // inline T auto_cast(U i) { return reinterpret_cast<T>(i); };
  //TODO more as needed (non-ptr with caution)

  template<ssize_t start, ssize_t stride, size_t... S> inline constexpr auto mkSequence(std::index_sequence<S...>) {
    return std::index_sequence<(S * stride + start)...>();
  }

  template<ssize_t start, size_t count, ssize_t stride = 1> inline constexpr auto mkSequence() {
    return mkSequence<start, stride>(std::make_index_sequence<count>());
  }

  template<class... T, size_t... S> constexpr auto subtuple(std::index_sequence<S...>, std::tuple<T...> tuple) {
    return std::make_tuple(std::get<S>(tuple)...);
  };

  template<class T, class... U> constexpr auto popTupleFront(std::tuple<T, U...> tuple) {
    return subtuple(mkSequence<1, sizeof...(U)>(), tuple);
  };

  template<class T> consteval inline size_t sizeofCollection(T t) {
    //std::vector et al can be used constexpr but when used as a temporary, the compiler complains of not deallocating the temporary. Redirecting it by value into another CE function makes it ok.
    return t.size();
  };

  template<class T> inline void atomicMin(std::atomic<T>& a, T v) {
    T old = a.load(std::memory_order_relaxed);
    while(v < old && !atomic_compare_exchange_weak_explicit(&a, &old, v, std::memory_order_relaxed, std::memory_order_relaxed));
  };

  template<class T> inline void atomicMax(std::atomic<T>& a, T v) {
    T old = a.load(std::memory_order_relaxed);
    while(v > old && !atomic_compare_exchange_weak_explicit(&a, &old, v, std::memory_order_relaxed, std::memory_order_relaxed));
  };

}
