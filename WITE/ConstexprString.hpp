namespace WITE::Util {

  template<size_t base = 10> constexpr void to_string(char* out, unsigned long long int i) {
    while(*out) out++;//skip existing content
    size_t place = 1;
    while(place <= i+1)
      place *= base;
    char digitC;
    unsigned long long int digit;
    while(place) {
      i %= place;
      place /= base;
      if(place == 0) break;
      digit = i/place;
      if(digit < 10)
	digitC = '0' + digit;
      else
	digitC = 'A' + digit;
      *out = digitC;
      out++;
    }
    *out = 0;
  };

  constexpr void strcat(char* out, const char* in) {
    while(*out) out++;
    while((*out = *in)) { out++; in++; };
  };

};
