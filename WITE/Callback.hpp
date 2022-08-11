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

  template<class RET, class... RArgs> class CallbackFactory {
  private:
    template<class T, class... CArgs> class Callback : public Callback_t<RET, RArgs...> {
    private:
      RET(T::*x)(CArgs..., RArgs...);
      T * owner;
      const std::tuple<CArgs...> cargs;
    public:
      constexpr Callback(T* t, RET(T::*x)(CArgs..., RArgs...), CArgs... pda) :
	x(x), owner(t), cargs(std::forward<CArgs>(pda)...) {};
      constexpr ~Callback() {};
      RET call(RArgs... rargs) const override {
	return std::apply(x, std::tuple_cat(std::tie(owner), cargs, std::tie(rargs...)));
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
      RET call(RArgs... rargs) const override {
	return std::apply(x, std::tuple_cat(cargs, std::tie(rargs...)));
      };
    };
    template<class... CArgs> class MemberCallback : public Callback_t<RET, RArgs...> {
      using T = typename std::tuple_element<0, RArgs...>::type;
      using Rest = typename tuple_splice<0, 1, RArgs...>::Right;
    private:
      RET(T::*x)(CArgs..., Rest...);
      const std::tuple<CArgs...> cargs;
    public:
      constexpr MemberCallback(RET(T::*x)(CArgs..., Rest...), CArgs... pda) :
	x(x), cargs(std::forward<CArgs>(pda)...) {}
      constexpr MemberCallback();
      RET call(T t, Rest r) const override {
	return std::apply(x, std::tuple_cat(std::tie(t), cargs, r));
      };
    };
  public:
    typedef Callback_t<RET, RArgs...>* callback_t;
    //pointers:
    template<class U, class... CArgs> constexpr static callback_t
    make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static callback_t
    make(CArgs... cargs, RET(*func)(CArgs..., RArgs...));//for non-members or static members
    template<class... CArgs> constexpr static callback_t
    make_membercaller(CArgs... cargs, decltype(MemberCallback<CArgs...>::x) x);
    //unique_ptrs:
    template<class U, class... CArgs> constexpr static std::unique_ptr<Callback_t<RET, RArgs...>>
    make_unique(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static std::unique_ptr<Callback_t<RET, RArgs...>>
    make_unique(CArgs... cargs, RET(*func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static std::unique_ptr<Callback_t<RET, RArgs...>>
    make_membercaller_unique(CArgs... cargs, decltype(MemberCallback<CArgs...>::x) x);
    //references:
    template<class U, class... CArgs> constexpr static Callback_t<RET, RArgs...>&
    make_ref(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static Callback_t<RET, RArgs...>&
    make_ref(CArgs... cargs, RET(*func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static Callback_t<RET, RArgs...>&
    make_membercaller_ref(CArgs... cargs, decltype(MemberCallback<CArgs...>::x) x);
  };

  template<class RET, class... RArgs> template<class U, class... CArgs> constexpr Callback_t<RET, RArgs...>*
  CallbackFactory<RET, RArgs...>::make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return new wintypename CallbackFactory<RET, RArgs...>::Callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class... CArgs> constexpr Callback_t<RET, RArgs...>*
  CallbackFactory<RET, RArgs...>::make(CArgs... cargs, RET(*func)(CArgs..., RArgs...)) {
    return new wintypename CallbackFactory<RET, RArgs...>::StaticCallback<CArgs...>(func, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class... CArgs> constexpr Callback_t<RET, RArgs...>*
  CallbackFactory<RET, RArgs...>::make_membercaller(CArgs... cargs, decltype(MemberCallback<CArgs...>::x) x) {
    return new wintypename CallbackFactory<RET, RArgs...>::MemberCallback<CArgs...>(x, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class U, class... CArgs> constexpr std::unique_ptr<Callback_t<RET, RArgs...>>
  CallbackFactory<RET, RArgs...>::make_unique(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return std::unique_ptr<Callback_t<RET, RArgs...>>(make(owner, std::forward(cargs)..., func));
  }

  template<class RET, class... RArgs> template<class... CArgs> constexpr std::unique_ptr<Callback_t<RET, RArgs...>>
  CallbackFactory<RET, RArgs...>::make_unique(CArgs... cargs, RET(*func)(CArgs..., RArgs...)) {
    return std::unique_ptr<Callback_t<RET, RArgs...>>(make(std::forward(cargs)..., func));
  }

  template<class RET, class... RArgs> template<class... CArgs> constexpr std::unique_ptr<Callback_t<RET, RArgs...>>
  CallbackFactory<RET, RArgs...>::make_membercaller_unique(CArgs... cargs, decltype(MemberCallback<CArgs...>::x) x) {
    return std::unique_ptr<Callback_t<RET, RArgs...>>(make(std::forward(cargs)..., x));
  };

  template<class RET, class... RArgs> template<class U, class... CArgs> constexpr Callback_t<RET, RArgs...>&
  CallbackFactory<RET, RArgs...>::make_ref(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return * new wintypename CallbackFactory<RET, RArgs...>::Callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class... CArgs> constexpr Callback_t<RET, RArgs...>&
  CallbackFactory<RET, RArgs...>::make_ref(CArgs... cargs, RET(*func)(CArgs..., RArgs...)) {
    return * new wintypename CallbackFactory<RET, RArgs...>::StaticCallback<CArgs...>(func, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class... CArgs> constexpr Callback_t<RET, RArgs...>&
  CallbackFactory<RET, RArgs...>::make_membercaller_ref(CArgs... cargs, decltype(MemberCallback<CArgs...>::x) x) {
    return * new wintypename CallbackFactory<RET, RArgs...>::MemberCallback<CArgs...>(x, std::forward<CArgs>(cargs)...);
  };

#define typedefCB(name, ...) typedef WITE::Util::CallbackFactory<__VA_ARGS__> name## _F; typedef typename name## _F::callback_t name ;// typedef typename std::unique_ptr<WITE::Util::Callback_t<__VA_ARGS__>> name## _unique;

  typedefCB(rawDataSource, int, void*, size_t)

};
