#pragma once

#include <tuple>
#include <memory>
#include <functional>

#include "tupleSplice.hpp"

namespace WITE {

  template<class RET, class... RArgs> class callback_t {
  public:
    virtual RET call(RArgs... rargs) const = 0;
    virtual ~callback_t() = default;
    RET operator()(RArgs... rargs) const {
      return call(rargs...);
    };
  };

  template<class RET, class... RArgs> class callbackConstexprWrapper : public callback_t<RET, RArgs...> {
    typedef callback_t<RET, RArgs...> cbt;
  private:
    const cbt* cb;
  public:
    constexpr callbackConstexprWrapper() : cb(NULL) {};
    constexpr callbackConstexprWrapper(const callbackConstexprWrapper& src) : cb(src.cb) {};
    constexpr callbackConstexprWrapper(const cbt* src) : cb(src) {};
    RET call(RArgs... rargs) const override { return cb->call(std::forward<RArgs>(rargs)...); };
    constexpr operator bool() const { return cb; };
  };

  template<class RET, class... RArgs> class callbackPtr {
  private:
    mutable std::shared_ptr<callback_t<RET, RArgs...>> cb;
  public:
    // constexpr callbackPtr(RET(*x)(RArgs...));
    callbackPtr() : cb() {};
    callbackPtr(callback_t<RET, RArgs...>* cb) : cb(cb) {};
    callbackPtr(const callbackPtr& other) : cb(other.cb) {};
    inline RET operator()(RArgs... rargs) const { return cb->call(std::forward<RArgs>(rargs)...); };
    inline const callback_t<RET, RArgs...>* operator->() const { return cb.get(); };
    inline operator bool() const { return (bool)cb; };
  };

  template<class RET, class... RArgs> class callbackFactory {
  private:
    template<class L> static auto lambdaWrapper(L l, RArgs... rargs) { return l(std::forward<RArgs>(rargs)...); };
  public:
    template<class T, class... CArgs> class callback : public callback_t<RET, RArgs...> {
    private:
      RET(T::*const x)(CArgs..., RArgs...);
      T *const owner;
      const std::tuple<CArgs...> cargs;
    public:
      constexpr callback(T* t, RET(T::*const x)(CArgs..., RArgs...), CArgs... pda) :
	x(x), owner(t), cargs(std::forward<CArgs>(pda)...) {};
      constexpr ~callback() {};
      constexpr callback(const callback<T, CArgs...>& other) : x(other.x), owner(other.owner), cargs(other.cargs) {};
      RET call(RArgs... rargs) const override {
	return std::apply(x, std::tuple_cat(std::tie(owner), cargs, std::tie(rargs...)));
      };
    };
    template<class... CArgs> class StaticCallback : public callback_t<RET, RArgs...> {
    private:
      RET(*const x)(CArgs..., RArgs...);
      std::tuple<CArgs...> cargs;
    public:
      constexpr StaticCallback(RET(*const x)(CArgs..., RArgs...), CArgs... pda) :
	x(x), cargs(std::forward<CArgs>(pda)...) {};
      constexpr ~StaticCallback() {};
      constexpr StaticCallback(const StaticCallback<CArgs...>& other): x(other.x), cargs(other.cargs) {};
      RET call(RArgs... rargs) const override {
	return std::apply(x, std::tuple_cat(cargs, std::tie(rargs...)));
      };
    };
    typedef callbackPtr<RET, RArgs...> callback_t;
    template<class U, class... CArgs> constexpr static callback_t
    make(U* owner, CArgs... cargs, RET(U::*const func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static callback_t
    make(CArgs... cargs, RET(*const func)(CArgs..., RArgs...));
    template<class L> requires requires(L l, RArgs... ra) { {l(std::forward<RArgs>(ra)...)} -> std::convertible_to<RET>; }
    constexpr static callback_t make(L);
  };

  template<class RET, class... RArgs> template<class U, class... CArgs> constexpr callbackPtr<RET, RArgs...>
  callbackFactory<RET, RArgs...>::make(U*const owner, CArgs... cargs, RET(U::*const func)(CArgs..., RArgs...)) {
    return callbackPtr(new callbackFactory<RET, RArgs...>::callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...));
  }

  template<class RET, class... RArgs> template<class... CArgs> constexpr callbackPtr<RET, RArgs...>
  callbackFactory<RET, RArgs...>::make(CArgs... cargs, RET(*const func)(CArgs..., RArgs...)) {
    return callbackPtr(new callbackFactory<RET, RArgs...>::StaticCallback<CArgs...>(func, std::forward<CArgs>(cargs)...));
  }

  //wrapper for lambdas
  template<class RET, class... RArgs>
  template<class L> requires requires(L l, RArgs... ra) { {l(std::forward<RArgs>(ra)...)} -> std::convertible_to<RET>; }
  constexpr callbackPtr<RET, RArgs...> callbackFactory<RET, RArgs...>::make(L l) {
    return make<L>(l, &callbackFactory<RET, RArgs...>::lambdaWrapper);
  };

#define typedefCB(name, ...) typedef WITE::callbackFactory<__VA_ARGS__> name## _F; typedef typename name## _F::callback_t name ; typedef WITE::callbackConstexprWrapper<__VA_ARGS__> name## _ce ;

  typedefCB(rawDataSource, int, void*, size_t)

#define COMMA ,
#define MAKE_FUNC_TEST(type, name, sig, sig2)				\
  template<class T> struct get_member_ ##name## _or_NULL {		\
    static constexpr type## _ce value;					\
    static constexpr bool exists = false;				\
  };									\
  template<class T>							\
  requires requires( sig ) { T::name ( sig2 ); }			\
  struct get_member_ ##name## _or_NULL<T> {				\
    static constexpr type## _F::StaticCallback<> cbt = &T::name;	\
    static constexpr type## _ce value = &cbt;				\
    static constexpr bool exists = true;				\
  }


};
