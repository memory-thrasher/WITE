#include "IteratorWrapper.hpp"
#include "Callback.hpp"

namespace WITE::Collections {

  template<class I1, class I2>
  class IteratorMultiplexer {
  public:
    typedefCB(fetcher_cb, I1&, I2);
  private:
    IteratorWrapper<I1> upper;
    IteratorWrapper<I2> lower;
    fetcher_cb p;
  public:
    IteratorMultiplexer() : upper(), lower(), p() {}
    IteratorMultiplexer(IteratorWrapper<I1> i1, fetcher_cb p) :
      upper(i1),
      lower(p(*i1)),
      p(p) {
    };
    inline operator bool() { return (bool)upper; };
    inline auto operator*() { return *lower; };
    inline auto& operator->() { return *lower; };
    inline bool operator==(const IteratorMultiplexer<I1, I2>& r) {
      return (!r && !*this) || (r.upper == upper && r.lower == lower);
    };
    inline bool operator!=(const IteratorMultiplexer<I1, I2>& r) { return !operator==(r); };
    inline IteratorMultiplexer operator++() {
      lower++;
      if(!lower && upper) {
	upper++;
	if(upper)
	  lower = p(*upper);
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
