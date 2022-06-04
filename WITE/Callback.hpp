#pragma once

#include <tuple>

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
      std::tuple<CArgs...> cargs;
    public:
      Callback(T* t, RET(T::*x)(CArgs..., RArgs...), CArgs... pda);
      ~Callback() {};
      RET call(RArgs... rargs) const override {
        return (*owner.*(x))(std::get<CArgs>(cargs)..., rargs...);
      };
    };
    template<class... CArgs> class StaticCallback : public Callback_t<RET, RArgs...> {
    private:
      RET(*x)(CArgs..., RArgs...);
      std::tuple<CArgs...> cargs;
    public:
      StaticCallback(RET(*x)(CArgs..., RArgs...), CArgs... pda);
      ~StaticCallback() {};
      RET call(RArgs... rargs) const override {
        return (*x)(std::get<CArgs>(cargs)..., rargs...);
      };
    };
  public:
    typedef Callback_t<RET, RArgs...>* callback_t;
    template<class U, class... CArgs> static callback_t make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...));
    template<class... CArgs> static callback_t make(CArgs... cargs, RET(*func)(CArgs..., RArgs...));//for non-members or static members
  };
  template<class RET2, class... RArgs2> template<class T2, class... CArgs2>
  CallbackFactory<RET2, RArgs2...>::Callback<T2, CArgs2...>::Callback(T2* t, RET2(T2::*x)(CArgs2..., RArgs2...), CArgs2... pda) :
    x(x), owner(t), cargs(std::forward<CArgs2>(pda)...) {}

  template<class RET2, class... RArgs2> template<class... CArgs2>
  CallbackFactory<RET2, RArgs2...>::StaticCallback<CArgs2...>::StaticCallback(RET2(*x)(CArgs2..., RArgs2...), CArgs2... pda) :
    x(x), cargs(std::forward<CArgs2>(pda)...) {}

  template<class RET, class... RArgs> template<class U, class... CArgs> Callback_t<RET, RArgs...>*
  CallbackFactory<RET, RArgs...>::make(U* owner, CArgs... cargs, RET(U::*func)(CArgs..., RArgs...)) {
    return new wintypename CallbackFactory<RET, RArgs...>::Callback<U, CArgs...>(owner, func, std::forward<CArgs>(cargs)...);
  };

  template<class RET, class... RArgs> template<class... CArgs> Callback_t<RET, RArgs...>*
  CallbackFactory<RET, RArgs...>::make(CArgs... cargs, RET(*func)(CArgs..., RArgs...)) {
    return new wintypename CallbackFactory<RET, RArgs...>::StaticCallback<CArgs...>(func, std::forward<CArgs>(cargs)...);
  };

#define typedefCB(name, ...) typedef WITE::Util::CallbackFactory<__VA_ARGS__> name## _F; typedef typename name## _F::callback_t name ;

  typedefCB(rawDataSource, int, void*, size_t)

};
