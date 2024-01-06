#pragma once

#include <tuple>
#include <memory>
#include <functional>

#include "TupleSplice.hpp"

#ifdef _WIN32
#define wintypename typename
#else
#define wintypename
#endif

namespace WITE::Util {

  template<class RET, class... RArgs> class Callback_t {
  public:
    virtual RET call(RArgs... rargs) const = 0;
    virtual ~Callback_t() = default;
    RET operator()(RArgs... rargs) const {
      return call(rargs...);
    };
  };

  template<class RET, class... RArgs> class CallbackConstexprWrapper : public Callback_t<RET, RArgs...> {
    typedef Callback_t<RET, RArgs...> cbt;
  private:
    const cbt* cb;
  public:
    constexpr CallbackConstexprWrapper() : cb(NULL) {};
    constexpr CallbackConstexprWrapper(const CallbackConstexprWrapper& src) : cb(src.cb) {};
    constexpr CallbackConstexprWrapper(const cbt* src) : cb(src) {};
    RET call(RArgs... rargs) const override { return cb->call(std::forward<RArgs>(rargs)...); };
    constexpr operator bool() const { return cb; };
  };

  template<class RET, class... RArgs> class CallbackPtr {
  private:
    mutable std::shared_ptr<Callback_t<RET, RArgs...>> cb;
  public:
    // constexpr CallbackPtr(RET(*x)(RArgs...));
    CallbackPtr() : cb() {};
    CallbackPtr(Callback_t<RET, RArgs...>* cb) : cb(cb) {};
    CallbackPtr(const CallbackPtr& other) : cb(other.cb) {};
    inline RET operator()(RArgs... rargs) const { return cb->call(std::forward<RArgs>(rargs)...); };
    inline const Callback_t<RET, RArgs...>* operator->() const { return cb.get(); };
    inline operator bool() const { return (bool)cb; };
  };

  template<class RET, class... RArgs> class CallbackFactory {
  private:
    template<class L> static auto lambdaWrapper(L l, RArgs... rargs) { return l(std::forward<RArgs>(rargs)...); };
  public:
    template<class T, class... CArgs> class Callback : public Callback_t<RET, RArgs...> {
    private:
      RET(T::*const x)(CArgs..., RArgs...);
      T *const owner;
      const std::tuple<CArgs...> cargs;
    public:
      constexpr Callback(T* t, RET(T::*const x)(CArgs..., RArgs...), CArgs... pda) :
	x(x), owner(t), cargs(std::forward<CArgs>(pda)...) {};
      constexpr ~Callback() {};
      constexpr Callback(const Callback<T, CArgs...>& other) : x(other.x), owner(other.owner), cargs(other.cargs) {};
      RET call(RArgs... rargs) const override {
	return std::apply(x, std::tuple_cat(std::tie(owner), cargs, std::tie(rargs...)));
      };
    };
    template<class... CArgs> class StaticCallback : public Callback_t<RET, RArgs...> {
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
    typedef CallbackPtr<RET, RArgs...> callback_t;
    template<class U, class... CArgs> constexpr static callback_t
    make(U* owner, CArgs... cargs, RET(U::*const func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static callback_t
    make(CArgs... cargs, RET(*const func)(CArgs..., RArgs...));
    template<class L> requires requires(L l, RArgs... ra) { {l(std::forward<RArgs>(ra)...)} -> std::convertible_to<RET>; }
    constexpr static callback_t make(L);
  };

  template<class RET, class... RArgs> template<class U, class... CArgs> constexpr CallbackPtr<RET, RArgs...>
  CallbackFactory<RET, RArgs...>::make(U*const owner, CArgs... cargs, RET(U::*const func)(CArgs..., RArgs...)) {
    return CallbackPtr(new CallbackFactory<RET, RArgs...>::Callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...));
  }

  template<class RET, class... RArgs> template<class... CArgs> constexpr CallbackPtr<RET, RArgs...>
  CallbackFactory<RET, RArgs...>::make(CArgs... cargs, RET(*const func)(CArgs..., RArgs...)) {
    return CallbackPtr(new CallbackFactory<RET, RArgs...>::StaticCallback<CArgs...>(func, std::forward<CArgs>(cargs)...));
  }

  //wrapper for lambdas
  template<class RET, class... RArgs>
  template<class L> requires requires(L l, RArgs... ra) { {l(std::forward<RArgs>(ra)...)} -> std::convertible_to<RET>; }
  constexpr CallbackPtr<RET, RArgs...> CallbackFactory<RET, RArgs...>::make(L l) {
    return make<L>(l, &CallbackFactory<RET, RArgs...>::lambdaWrapper);
  };

#define typedefCB(name, ...) typedef WITE::Util::CallbackFactory<__VA_ARGS__> name## _F; typedef typename name## _F::callback_t name ; typedef WITE::Util::CallbackConstexprWrapper<__VA_ARGS__> name## _ce ;

  typedefCB(rawDataSource, int, void*, size_t)

#define COMMA ,
#define MAKE_FUNC_TEST(type, name, sig, sig2)				\
  template<class T> struct get_member_ ##name## _or_NULL {		\
    static constexpr type## _ce value;					\
  };									\
  template<class T>							\
  requires requires( sig ) { T::name ( sig2 ); }			\
  struct get_member_ ##name## _or_NULL<T> {				\
    static constexpr type## _F::StaticCallback<> cbt = &T::name;	\
    static constexpr type## _ce value = &cbt;				\
  }


};
