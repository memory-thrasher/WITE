#include "IteratorWrapper.hpp"
#include "Callback.hpp"

namespace WITE::Collections {

  template<class I1, class I2>
  class IteratorMultiplexer {
  public:
    using fetcher_cb_return = IteratorWrapper<I2>;
    using fetcher_cb_param = I1;
    typedefCB(fetcher_cb, fetcher_cb_return, fetcher_cb_param);
  private:
    IteratorWrapper<I1> upper;
    IteratorWrapper<I2> lower;
    fetcher_cb p;
  public:
    IteratorMultiplexer() : upper(), lower(), p() {}
    IteratorMultiplexer(IteratorWrapper<I1> i1, fetcher_cb p) :
      upper(i1),
      lower(p->call((fetcher_cb_param)upper)),
      p(p) {
    };
    inline operator bool() const { return (bool)upper; };
    typename std::conditional<std::is_pointer_v<I2>,
			      typename std::add_rvalue_reference<std::remove_pointer_t<I2>>::type,
			      decltype(*lower)>::type
    inline operator*() { return *lower; };
    inline auto& operator->() { return *lower; };
    inline bool operator==(const IteratorMultiplexer<I1, I2>& r) const {
      return (!r && !*this) || (r.upper == upper && r.lower == lower);
    };
    //inline bool operator!=(const IteratorMultiplexer<I1, I2>& r) { return !operator==(r); };//implied now?
    inline IteratorMultiplexer operator++() {
      lower++;
      if(!lower && upper) {
	upper++;
	if(upper)
	  lower = p->call((fetcher_cb_param)upper);
      }
      return *this;
    };
    inline IteratorMultiplexer operator++(int) {
      IteratorMultiplexer old = *this;
      operator++();
      return old;
    };
    //begin and end so this can be the target of a foreach
    inline IteratorMultiplexer<I1, I2> begin() const { return *this; };//invokes copy constructor
    inline IteratorMultiplexer<I1, I2> end() const { return IteratorMultiplexer<I1, I2>(); };
  };

};
