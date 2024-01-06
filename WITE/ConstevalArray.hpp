#pragma once

#include "Utils_Memory.hpp"

namespace WITE::Collections {

  template<class T> struct ConstevalArrayIterator;

  template<class T> struct ConstevalArrayBase { //so it can be iterated
    constexpr ConstevalArrayBase() = default;
    constexpr virtual const ConstevalArrayBase<T>* next() const = 0;
    constexpr virtual size_t length() const = 0;
    constexpr virtual const T& getData() const = 0;
    consteval virtual T& operator[](size_t i) = 0;
    consteval virtual const T& operator[](size_t i) const = 0;
    constexpr inline ConstevalArrayIterator<T> begin() const { return { this }; };
    constexpr inline ConstevalArrayIterator<T> end() const { return {}; };
  };

  template<class T> struct ConstevalArrayIterator {
    typedef std::input_iterator_tag iterator_category;
    typedef std::input_iterator_tag iterator_concept;
    typedef size_t difference_type;
    typedef T value_type;
    typedef T* pointer;
    typedef T& reference;
    using U = ConstevalArrayBase<T>;
    using SPECULUM = ConstevalArrayIterator<T>;
    const U* data;//NULL when past-the-end

    constexpr ConstevalArrayIterator() = default;
    constexpr ConstevalArrayIterator(const SPECULUM& o) = default;
    constexpr ConstevalArrayIterator(const U* d) : data(d) {};

    constexpr friend bool operator==(const SPECULUM& a, const SPECULUM& b) { return a.data == b.data; };
    constexpr friend bool operator!=(const SPECULUM& a, const SPECULUM& b) { return a.data != b.data; };
    constexpr const T& operator*() { return data->getData(); };
    constexpr const T& operator->() { return data->getData(); };

    constexpr SPECULUM& operator++() {
      if(data)
	data = data->next();
      return *this;
    };

    constexpr SPECULUM& operator++(int) {
      SPECULUM old = *this;
      operator++();
      return old;
    };

  };

  template<class T, size_t L> struct alignas(T[L]) ConstevalArray : public ConstevalArrayBase<T> {
  private:
    template<class Lambda> consteval ConstevalArray(Lambda l, size_t i) : data(l(i)), rest(l, i+1) {};
  public:
    typedef ConstevalArray<T, L-1> rest_t;
    T data;
    rest_t rest;//so we don't have to initialize data all at once

    constexpr ConstevalArray() {};
    constexpr ConstevalArray(const ConstevalArray<T, L>& o) : data(o.data), rest(o.rest) {};
    constexpr ~ConstevalArray() = default;
    template<class iter> constexpr ConstevalArray(iter start) : data(*start), rest(++start) {};
    consteval ConstevalArray(std::initializer_list<T> il) : ConstevalArray(il.begin()) {};

    template<class Lambda> requires requires(Lambda l, size_t i) { { l(i) } -> std::convertible_to<T>; }
    consteval ConstevalArray(Lambda l) : ConstevalArray<T, L>(l, 0) {};

    constexpr inline operator T*() { return &data; };
    constexpr inline operator const T*() const { return &data; };

    consteval virtual inline T& operator[](size_t i) override { return i == 0 ? data : rest[i-1]; };
    consteval virtual inline const T& operator[](size_t i) const override { return i == 0 ? data : rest[i-1]; };
    constexpr virtual const T& getData() const override { return data; };
    constexpr virtual const rest_t* next() const override { return &rest; };
    constexpr virtual size_t length() const override { return L; };

    template<size_t i> requires (i < L) constexpr inline T& get() {
      if constexpr(i == 0)
	return data;
      else
	return rest.template get<i-1>();
    };

    inline T& get(size_t i) {//separate from consteval operator[] so it can't accidentally slow down a runtime call
      if(i == 0)
	return data;
      else
	return rest.get(i-1);
    };

    template<size_t i> consteval inline auto& sub() {
      if constexpr(i == 0)
	return *this;
      else
	return rest.template sub<i-1>();
    };

    template<size_t i> consteval inline const auto& sub() const {
      if constexpr(i == 0)
	return *this;
      else
	return rest.template sub<i-1>();
    };

    // consteval std::array<T, L> toStdArray() {
    //   return std::to_array<T, L>(toTuple());
    // };

    // consteval auto toTuple() {
    //   //TODO recursively build tuple
    // }; //backup incase the below breaks when used

    constexpr std::array<T, L> toStdArray() {
      return toStdArrayImpl(std::make_index_sequence<L>{});
    };

    template<size_t... I> constexpr std::array<T, L> toStdArrayImpl() {
      return {{ this[I]... }};
    };

  };

  template<class T> struct alignas(T) ConstevalArray<T, 1> : public ConstevalArrayBase<T> {
  private:
    static constexpr size_t L = 1;
    template<class Lambda> consteval ConstevalArray(Lambda l, size_t i) : data(l(i)) {};
  public:
    T data;

    constexpr ConstevalArray() {};
    constexpr ConstevalArray(const ConstevalArray<T, L>&) = default;
    constexpr ConstevalArray(T& t) : data(t) {};
    constexpr ~ConstevalArray() = default;
    template<class iter> constexpr ConstevalArray(iter start) : data(*start) {};
    consteval ConstevalArray(std::initializer_list<T> il) : ConstevalArray(il.begin()) {};
    constexpr ConstevalArray(T (&t)[L]) : ConstevalArray<T, L>(t[0]) {};

    template<class Lambda> requires requires(Lambda l, size_t i) { { l(i) } -> std::convertible_to<T>; }
    consteval ConstevalArray(Lambda l) : ConstevalArray<T, L>(l, 0) {};

    constexpr inline operator T*() { return &data; };
    constexpr inline operator const T*() const { return &data; };

    consteval virtual inline T& operator[](size_t i) override { ASSERT_CONSTEXPR(i == 0); return data; };
    consteval virtual inline const T& operator[](size_t i) const override { ASSERT_CONSTEXPR(i == 0); return data; };
    constexpr virtual const T& getData() const override { return data; };
    constexpr virtual const ConstevalArray<T, 1>* next() const override { return NULL; };
    constexpr virtual size_t length() const override { return L; };
    template<size_t i> requires (i < L) constexpr inline T& get() { return data; };
    inline T& get(size_t i) { ASSERT_TRAP(i == 0, "array underflow"); return data; };

    template<size_t i> consteval inline auto& sub() { static_assert(i == 0); return *this; };
    template<size_t i> consteval inline const auto& sub() const { static_assert(i == 0); return *this; };
  };

  template<class T> struct ConstevalArray<T, 0> {//emptiness (not stored)
    static constexpr size_t L = 0;
    constexpr ConstevalArray() {};
    constexpr ConstevalArray(const ConstevalArray<T, L>&) = default;
    constexpr inline ConstevalArrayIterator<T> begin() { return {}; };
    constexpr inline ConstevalArrayIterator<T> end() { return {}; };
  };

};
