#pragma once

#include <tuple>
#include <memory>
#include <functional>

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
  };

  template<class RET, class... RArgs> class CallbackFactory {
  private:
    template<class T, class... CArgs> class Callback : public Callback_t<RET, RArgs...> {
    private:
      RET(T::*x)(CArgs..., RArgs...);
      T * owner;
      const std::tuple<CArgs...> cargs;
    public:
      constexpr Callback(T* t, RET(T::*x)(CArgs..., RArgs...), CArgs... pda);
      constexpr ~Callback() {};
      RET call(RArgs... rargs) const override {
        //return (*owner.*(x))(std::get<CArgs>(cargs)..., rargs...);
	return std::apply(x, std::tuple_cat(std::tie(owner), cargs, std::tie(rargs...)));
      };
    };
    template<class... CArgs> class StaticCallback : public Callback_t<RET, RArgs...> {
    private:
      RET(*x)(CArgs..., RArgs...);
      std::tuple<CArgs...> cargs;
    public:
      constexpr StaticCallback(RET(*x)(CArgs..., RArgs...), CArgs... pda);
      constexpr ~StaticCallback() {};
      RET call(RArgs... rargs) const override {
        // return (*x)(std::get<CArgs>(cargs)..., rargs...);
	return std::apply(x, std::tuple_cat(cargs, std::tie(rargs...)));
      };
    };
  public:
    typedef Callback_t<RET, RArgs...>* callback_t;
    //pointers:
    template<class U, class... CArgs> constexpr static callback_t
    make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static callback_t
    make(CArgs... cargs, RET(*func)(CArgs..., RArgs...));//for non-members or static members
    //unique_ptrs:
    template<class U, class... CArgs> constexpr static std::unique_ptr<Callback_t<RET, RArgs...>>
    make_unique(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static std::unique_ptr<Callback_t<RET, RArgs...>>
    make_unique(CArgs... cargs, RET(*func)(CArgs..., RArgs...));
    //references:
    template<class U, class... CArgs> constexpr static Callback_t<RET, RArgs...>&
    make_ref(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
    template<class... CArgs> constexpr static Callback_t<RET, RArgs...>&
    make_ref(CArgs... cargs, RET(*func)(CArgs..., RArgs...));
  };
  template<class RET2, class... RArgs2> template<class T2, class... CArgs2> constexpr
  CallbackFactory<RET2, RArgs2...>::Callback<T2, CArgs2...>::Callback(T2* t, RET2(T2::*x)(CArgs2..., RArgs2...), CArgs2... pda) :
    x(x), owner(t), cargs(std::forward<CArgs2>(pda)...) {}

  template<class RET2, class... RArgs2> template<class... CArgs2> constexpr
  CallbackFactory<RET2, RArgs2...>::StaticCallback<CArgs2...>::StaticCallback(RET2(*x)(CArgs2..., RArgs2...), CArgs2... pda) :
    x(x), cargs(std::forward<CArgs2>(pda)...) {}

  template<class RET, class... RArgs> template<class U, class... CArgs> constexpr Callback_t<RET, RArgs...>*
  CallbackFactory<RET, RArgs...>::make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return new wintypename CallbackFactory<RET, RArgs...>::Callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class... CArgs> constexpr Callback_t<RET, RArgs...>*
  CallbackFactory<RET, RArgs...>::make(CArgs... cargs, RET(*func)(CArgs..., RArgs...)) {
    return new wintypename CallbackFactory<RET, RArgs...>::StaticCallback<CArgs...>(func, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class U, class... CArgs> constexpr std::unique_ptr<Callback_t<RET, RArgs...>>
  CallbackFactory<RET, RArgs...>::make_unique(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return std::unique_ptr<Callback_t<RET, RArgs...>>(make(owner, std::forward(cargs)..., func));
  }

  template<class RET, class... RArgs> template<class... CArgs> constexpr std::unique_ptr<Callback_t<RET, RArgs...>>
  CallbackFactory<RET, RArgs...>::make_unique(CArgs... cargs, RET(*func)(CArgs..., RArgs...)) {
    return std::unique_ptr<Callback_t<RET, RArgs...>>(make(std::forward(cargs)..., func));
  }

  template<class RET, class... RArgs> template<class U, class... CArgs> constexpr Callback_t<RET, RArgs...>&
  CallbackFactory<RET, RArgs...>::make_ref(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return * new wintypename CallbackFactory<RET, RArgs...>::Callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class... CArgs> constexpr Callback_t<RET, RArgs...>&
  CallbackFactory<RET, RArgs...>::make_ref(CArgs... cargs, RET(*func)(CArgs..., RArgs...)) {
    return * new wintypename CallbackFactory<RET, RArgs...>::StaticCallback<CArgs...>(func, std::forward<CArgs>(cargs)...);
  };

#define typedefCB(name, ...) typedef WITE::Util::CallbackFactory<__VA_ARGS__> name## _F; typedef typename name## _F::callback_t name ;// typedef typename std::unique_ptr<WITE::Util::Callback_t<__VA_ARGS__>> name## _unique;

  typedefCB(rawDataSource, int, void*, size_t)

};
