#pragma once

#include <array>
#include <stddef.h>

#include "LiteralList.hpp"

namespace WITE::Collections {

  template<size_t layers, class T> constexpr std::array<size_t, 1> countIlRecursive(const std::initializer_list<T> il) {
    return { countIL(il) };
  };

  template<size_t layers, class T> constexpr std::array<size_t, layers> countIlRecursive(const std::initializer_list<std::initializer_list<T>> il) {
    std::array<size_t, layers> ret = {};
    for(const auto il2 : il) {
      std::array<size_t, layers-1> temp = countIlRecursive<layers-1>(il2);
      for(size_t i = 1;i < layers;i++)
	ret[i] += temp[i-1];
      ret[0]++;
    }
    return ret;
  };

  template<size_t start, size_t len, class T, size_t srcLen> requires (start + len <= srcLen)
    constexpr std::array<T, len> subArray(const std::array<T, srcLen> src) {
    std::array<T, len> ret = {};
    for(size_t i = 0;i < len;i++)
      ret[i] = src[start+i];
    return ret;
  };

  template<size_t N> constexpr size_t sum(std::array<size_t, N> src) {
    size_t ret = 0;
    for(size_t v : src)
      ret += v;
    return ret;
  };

  //cannot call a macro with first argument begin with { so in practice the first list element must be typed by `layers` copies of `std::initializer_list<` followed by the inner type. So helper macros:

#define STDIL(T) std::initializer_list< T >
#define STDIL1(T) STDIL(T)
#define STDIL2(T) STDIL1(STDIL(T))
#define STDIL3(T) STDIL2(STDIL(T))
#define STDIL4(T) STDIL3(STDIL(T))
#define STDIL5(T) STDIL4(STDIL(T))
#define STDIL6(T) STDIL5(STDIL(T))
#define STDIL7(T) STDIL6(STDIL(T))
#define STDIL8(T) STDIL7(STDIL(T))
#define STDIL9(T) STDIL8(STDIL(T))
  //beyond 9 dimensions of jagged array, rethink everything

  template<class T, size_t dimensions, std::array<size_t, dimensions> volume> class LiteralJaggedList {
  public:
    using SPECULUM = LiteralJaggedList<T, dimensions, volume>;
    static constexpr size_t indexCount = sum(subArray<0, dimensions-1>(volume)),
      indexDimensions = dimensions-1,
      dataCount = volume[dimensions-1];
    T data[dataCount];//all the data in the jagged array flattened
    size_t indexes[indexCount];//some of these point to data some back into indexes

    constexpr void populate(T* data, size_t* indexes, size_t* writeHeads,
			    const std::initializer_list<T> in) {
      for(T t : in)
	data[writeHeads[0]++] = t;
    };

    template<class U>
    constexpr void populate(T* data, size_t* indexes, size_t* writeHeads,
			    const std::initializer_list<std::initializer_list<U>> in) {
      for(auto il : in) {
	indexes[writeHeads[0]++] = writeHeads[1];
	populate(data, indexes, writeHeads+1, il);
      }
    };

    template<class U>
    constexpr LiteralJaggedList(const std::initializer_list<U> in) {
      size_t writeHeads[dimensions];
      size_t runningOffset = 0;
      for(size_t i = 0;i < indexDimensions;i++) {
	writeHeads[i] = runningOffset;
	runningOffset += volume[i];//skip last element cuz it points into data starting at 0
      }
      writeHeads[0] = writeHeads[dimensions-1] = 0;
      size_t indexesTemp[indexCount];
      T dataTemp[dataCount];
      populate(dataTemp, indexesTemp, writeHeads, in);
      std::copy(dataTemp, dataTemp+dataCount, data);
      std::copy(indexesTemp, indexesTemp+indexCount, indexes);
    };

    constexpr auto get(size_t... path) {
      constexpr size_t len = sizeof...(path);
      std::array<size_t, len> pa(std::forward<size_t>(path)...);
      return get<len>(pa);
    };

    template<size_t N> require N <= dimensions && N > 0 constexpr auto get(const std::array<size_t, N>& path) {
      size_t r = path[0];
      for(size_t i = 1;i < N;i++)
	r = indexes[r] + path[i];
      if constexpr(N == dimensions) {
	return (T&)data[r];
      } else {
	return indexes[r];
      }
    };

    template<size_t layer> struct iterator {
      static constexpr length = layer+1;
      SPECULUM* owner;
      std::array<size_t, length> path;
      template<> require layer == 0 constexpr iterator(SPECULUM* owner, size_t i) : owner(owner), path({i}) {};
      template<> require layer > 0 constexpr iterator(SPECULUM* owner, const iterator<layer-1>& previous, size_t next) :
				 owner(owner) {
	std::copy(previous.path.begin(), previous.path.end(), path.begin());
	path[length-1] = i;
      };
      constexpr iterator(SPECULUM* owner, std::array<size_t, length> data) : owner(owner) {
	std::copy(data.begin(), data.end(), path.begin());
      };
      template<> require layer < dimensions constexpr iterator<layer+1> operator*() const {
	return iterator<layer+1>(*this, 0);
      };
      template<> require layer == dimensions constexpr T& operator*() const {
	return owner->get(path);
      };
      constexpr auto operator++() {
	path[length-1]++;
	return *this;
      };
      constexpr auto operator++(int) {
	auto old = *this;
	operator++();
	return old;
      };
      template<> require layer < dimensions constexpr inline auto begin() { return this->operator*(); };
      template<> require layer < dimensions constexpr inline auto end() {
	//
      };
    };

    constexpr bool contains(const T& t) {
      //TODO iterate directlyon data
    };

  };

#define defineLiteralJaggedList(T, DIM, NOM, ...) constexpr auto NOM = ::WITE::Collections::LiteralJaggedList<T, DIM, ::WITE::Collections::countIlRecursive<DIM>({ __VA_ARGS__ })>({ __VA_ARGS__ });

  //TODO more defines for template declaration and definition

};
