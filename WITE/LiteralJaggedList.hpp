#pragma once

#include <array>
#include <stddef.h>
#include <type_traits>

#include "StructuralPair.hpp"
#include "LiteralList.hpp"
#include "StdExtensions.hpp"

namespace WITE::Collections {

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
      dataCount = sum(volume),
      DIM = dimensions;
    static constexpr std::array<size_t, dimensions+1> layerBoundaries = cumulativeSum(volume);
    static constexpr auto VOLUME = volume;
    T data[dataCount];//all the data in the jagged array flattened
    size_t indexes[indexCount];//some of these point to data some back into indexes

    //U is an iterable collection, eg initializer_list or vector, of something convertible to T
    template<class U> requires is_collection_of<U, T>
    constexpr void populate(T* data, size_t* indexes, size_t* writeHeads, const U in) {
      for(auto t : in)
	data[writeHeads[0]++] = t;
    };

    //U is a collection of collections
    template<multidimensionalCollection U> constexpr void populate(T* data, size_t* indexes, size_t* writeHeads, const U in) {
      for(auto il : in) {
	data[writeHeads[0]] = {};
	indexes[writeHeads[0]++] = writeHeads[1];
	populate(data, indexes, writeHeads+1, il);
      }
    };

    //U is a collection of StructuralPair<collection for recursion, convertible to T>
    template<class U> requires requires(U u) {
      {u.begin()->v} -> std::convertible_to<T>;
      {*u.begin()->k.begin()};
    }
    constexpr void populate(T* data, size_t* indexes, size_t* writeHeads, const U in) {
      for(auto pair : in) {
	data[writeHeads[0]] = pair.v;
	indexes[writeHeads[0]++] = writeHeads[1];
	populate(data, indexes, writeHeads+1, pair.k);
      }
    };

    template<class U>
    constexpr LiteralJaggedList(const std::initializer_list<U> in) {
      size_t writeHeads[dimensions];
      std::copy(layerBoundaries.begin(), layerBoundaries.begin()+dimensions, writeHeads);
      size_t indexesTemp[indexCount];
      T dataTemp[dataCount];
      populate(dataTemp, indexesTemp, writeHeads, in);
      std::copy(dataTemp, dataTemp+dataCount, data);
      std::copy(indexesTemp, indexesTemp+indexCount, indexes);
    };

    constexpr LiteralJaggedList() requires(sum(volume) == 0) {//empty
    }

    constexpr auto get(size_t path) const {
      return get(tie(path));
    };

    template<class... args> constexpr auto get(size_t f, args... path) const {
      return get(tie<size_t, args...>(f, std::forward<args>(path)...));
    };

    template<size_t N> requires (N <= dimensions && N > 0) constexpr auto get(const std::array<size_t, N>& path) const {
      size_t r = path[0];
      for(size_t i = 1;i < N;i++)
	r = indexes[r] + path[i];
      if constexpr(sizeof(T) > sizeof(void*)) {
	return (const T&)data[r];
      } else {
	return data[r];//primitive
      }
    };

    template<size_t layer> requires (layer < dimensions) struct iterator {
      const SPECULUM* owner;
      size_t target;//either an index into data or indexes, depending on layer == dimensions-1
      constexpr iterator(const SPECULUM* owner, size_t i) : owner(owner), target(i) {};
      constexpr const iterator<layer>& operator*() const
      {//note: nonstandard iterator
	return *this;
      };
      constexpr auto& get() const {
	return owner->data[target];
      };
      constexpr auto operator++() {
	target++;
	return *this;
      };
      constexpr auto operator++(int) {
	auto old = *this;
	operator++();
	return old;
      };
      constexpr inline auto begin() const requires (layer < dimensions-1) {//next layer of jagged array
	return iterator<layer+1>(owner, owner->indexes[target]);
      };
      constexpr inline auto end() const requires (layer < dimensions-1) {//past-the-end iterator of next layer
	return iterator<layer+1>(owner,
				 target == layerBoundaries[layer+1]-1 ? layerBoundaries[layer+2] : owner->indexes[target+1]);
      };
      // template<size_t BN> constexpr inline std::strong_ordering operator<=>(const iterator<BN>& o) const {
      constexpr inline std::strong_ordering operator<=>(const iterator<layer>& o) const {
	return target<=>o.target;
      };
      constexpr inline bool operator!=(const iterator<layer>& o) const { return (*this <=> o) != 0; };
    };

    //only use iterators if you care how the inner arrays are grouped. If you care euqally of all descendents, iterate on data
    constexpr iterator<0> begin() const {
      return { this, 0 };
    };

    constexpr iterator<0> end() const {
      return { this, layerBoundaries[1] };
    };

    template<size_t N> requires (N < dimensions)
      constexpr inline iterator<N-1> getIterator(const std::array<size_t, N>& path) const {
      size_t target = path[0];
      for(size_t i = 1;i < N;i++)
	target = indexes[target] + path[i];
      return { this, target };
    };

    template<class... args> constexpr inline iterator<sizeof...(args)> getIterator(size_t f, args... path) const {
      return getIterator(tie_cast<size_t, args...>(f, std::forward<args>(path)...));
    };

    template<size_t layer = dimensions-1, class Compare = std::less<T>>
    constexpr bool contains(const T& t, Compare c = Compare()) const {
      for(size_t i = layerBoundaries[layer];i < layerBoundaries[layer+1];i++) {
	const T& v = data[i];
	if(!c(v, t) && !c(t, v))
	  return true;
      }
      return false;
    };

  };

  //use in global or name space
#define defineLiteralJaggedList(T, DIM, NOM, ...) constexpr auto NOM = ::WITE::Collections::LiteralJaggedList<T, DIM, ::WITE::Collections::countIlRecursive<DIM>({ __VA_ARGS__ })>({ __VA_ARGS__ });

  //use in a class template declaration
#define acceptLiteralJaggedList(T, DIM, NOM) std::array<size_t, DIM> witeJaggedListVolume_ ##NOM, ::WITE::Collections::LiteralJaggedList<T, DIM, witeJaggedListVolume_ ##NOM> NOM

  //use to pass a JL to a template
#define passLiteralJaggedList(NOM) NOM.VOLUME, NOM

#define passEmptyJaggedList(T, DIM) std::array<size_t, DIM>(), ::WITE::Collections::LiteralJaggedList<T, DIM, std::array<size_t, DIM>()>();

#define defineLiteralJaggedListAliasType(T, DIM, NOM_T) namespace LJLA { constexpr size_t DIM_ ##NOM_T = DIM; template<std::array<size_t, DIM> VOL> using NOM_T = ::WITE::Collections::LiteralJaggedList<T, DIM, VOL>; }

#define acceptLiteralJaggedListAlias(NOM_T, NOM) std::array<size_t, LJLA::DIM_ ##NOM_T> witeJaggedListVolume_ ##NOM, LJLA::NOM_T <witeJaggedListVolume_ ##NOM> NOM

#define passEmptyJaggedListAlias(NOM_T) std::array<size_t, LJLA::DIM_ ##NOM_T>(), LJLA::NOM_T<std::array<size_t, LJLA::DIM_ ##NOM_T >{}>()

  //cross-namespace versions
#define acceptLiteralJaggedListAliasXNS(NS, NOM_T, NOM) std::array<size_t, NS::LJLA::DIM_ ##NOM_T> witeJaggedListVolume_ ##NOM, NS::LJLA::NOM_T <witeJaggedListVolume_ ##NOM> NOM

#define passEmptyJaggedListAliasXNS(NS, NOM_T) std::array<size_t, NS::LJLA::DIM_ ##NOM_T>{}, NS::LJLA::NOM_T<std::array<size_t, NS::LJLA::DIM_ ##NOM_T >{}>{}

  //alias types should make their own definitions

};
