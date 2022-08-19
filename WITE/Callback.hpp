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
    virtual Callback_t* copy() const = 0;
  };

  template<class RET, class... RArgs> class CallbackPtr {
  private:
    const Callback_t<RET, RArgs...>* cb;
  public:
    // constexpr CallbackPtr(RET(*x)(RArgs...));
    constexpr CallbackPtr() : cb(NULL) {};
    constexpr CallbackPtr(Callback_t<RET, RArgs...>* cb) : cb(cb) {};
    constexpr explicit CallbackPtr(void* cb) : CallbackPtr<RET, RArgs...>(reinterpret_cast<Callback_t<RET, RArgs...>*>(cb)) {};
    //CallbackPtr(CallbackPtr&&) = delete;
    constexpr CallbackPtr(const CallbackPtr& other) : cb(other ? other.cb->copy() : NULL) {};
    ~CallbackPtr() {
      if(cb) delete cb;
    };
    inline RET operator()(RArgs... rargs) const { return cb->call(std::forward<RArgs>(rargs)...); };
    inline const Callback_t<RET, RArgs...>& operator*() const { return *cb; };
    inline const Callback_t<RET, RArgs...>* operator->() const { return cb; };
    inline operator bool() const { return cb; };
    inline explicit operator void*() const { return reinterpret_cast<void*>(cb ? cb->copy() : NULL); };
  };

  template<class RET, class... RArgs> class CallbackFactory {
  public:
    template<class T, class... CArgs> class Callback : public Callback_t<RET, RArgs...> {
    private:
      RET(T::*x)(CArgs..., RArgs...);
      T * owner;
      const std::tuple<CArgs...> cargs;
    public:
      constexpr Callback(T* t, RET(T::*x)(CArgs..., RArgs...), CArgs... pda) :
	x(x), owner(t), cargs(std::forward<CArgs>(pda)...) {};
      constexpr ~Callback() {};
      constexpr Callback(const Callback<T, CArgs...>& other) : x(other.x), owner(other.owner), cargs(other.cargs) {};
      RET call(RArgs... rargs) const override {
	return std::apply(x, std::tuple_cat(std::tie(owner), cargs, std::tie(rargs...)));
      };
      Callback_t<RET, RArgs...>* copy() const override {
	return new Callback<T, CArgs...>(*this);
      };
    };
    template<class... CArgs> class StaticCallback : public Callback_t<RET, RArgs...> {
    private:
      RET(*x)(CArgs..., RArgs...);
      std::tuple<CArgs...> cargs;
    public:
      constexpr StaticCallback(RET(*x)(CArgs..., RArgs...), CArgs... pda) :
	x(x), cargs(std::forward<CArgs>(pda)...) {};
      constexpr ~StaticCallback() {};
      constexpr StaticCallback(const StaticCallback<CArgs...>& other): x(other.x), cargs(other.cargs) {};
      RET call(RArgs... rargs) const override {
	return std::apply(x, std::tuple_cat(cargs, std::tie(rargs...)));
      };
      Callback_t<RET, RArgs...>* copy() const override {
	return new StaticCallback<CArgs...>(*this);
      };
    };
    typedef CallbackPtr<RET, RArgs...> callback_t;
    template<class U, class... CArgs> constexpr static callback_t
    make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static callback_t
    make(CArgs... cargs, RET(*func)(CArgs..., RArgs...));
  };

  template<class RET, class... RArgs> template<class U, class... CArgs> constexpr CallbackPtr<RET, RArgs...>
  CallbackFactory<RET, RArgs...>::make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return CallbackPtr(new CallbackFactory<RET, RArgs...>::Callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...));
  }

  template<class RET, class... RArgs> template<class... CArgs> constexpr CallbackPtr<RET, RArgs...>
  CallbackFactory<RET, RArgs...>::make(CArgs... cargs, RET(*func)(CArgs..., RArgs...)) {
    return CallbackPtr(new CallbackFactory<RET, RArgs...>::StaticCallback<CArgs...>(func, std::forward<CArgs>(cargs)...));
  }

#define typedefCB(name, ...) typedef WITE::Util::CallbackFactory<__VA_ARGS__> name## _F; typedef typename name## _F::callback_t name ;

  typedefCB(rawDataSource, int, void*, size_t)

};
