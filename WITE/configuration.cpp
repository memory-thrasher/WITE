#include <vector>
#include <string>

#include "configuration.hpp"

namespace WITE::configuration {

  std::vector<std::unique_ptr<char[]>> options;

  void setOptions(int argc, char** argv) {//static
    for(int i = 0;i < argc;i++) {
      char* temp = new char[std::strlen(argv[i])];
      std::strcpy(temp, argv[i]);
      WARN("CLI option: ", temp);
      options.emplace_back(temp);//constructs unique_pointer around char array
    }
  };

  char* getOption(const char* key) {//static
    int keyLen = strlen(key);
    for(const auto& up : options) {
      char* kvp = up.get();
      if(strlen(kvp) > keyLen && kvp[keyLen] == '=' && strncmp(key, kvp, keyLen) == 0)
	return kvp+keyLen+1;//if no value given, this is a 0-length string (pointer to 0x00)
    }
    return NULL;
  };

  bool getOptionBool(const char* key) {
    char* v = getOption(key);
    if(v == NULL) return false;
    static constexpr const char* yesValues[] = { "yes", "y", "Y", "t", "T", "1" };
    for(const char* c : yesValues)
      if(strcmp(c, v) == 0)
	return true;
    return false;
  };

  int getOption(const char* key, int def) {
    char* option = getOption(key);
    if(option) return std::stoi(option);
    else return def;
  };

  long getOption(const char* key, long def) {
    char* option = getOption(key);
    if(option) return std::stol(option);
    else return def;
  };

  long long getOption(const char* key, long long def) {
    char* option = getOption(key);
    if(option) return std::stoll(option);
    else return def;
  };

  unsigned int getOption(const char* key, unsigned int def) {
    char* option = getOption(key);
    if(option) return std::stoui(option);
    else return def;
  };

  unsigned long getOption(const char* key, unsigned long def) {
    char* option = getOption(key);
    if(option) return std::stoul(option);
    else return def;
  };

  unsigned long long getOption(const char* key, unsigned long long def) {
    char* option = getOption(key);
    if(option) return std::stoull(option);
    else return def;
  };

};
