#pragma once

namespace WITE::configuration {

  void setOptions(int argc, char** argv);//ideally passed through from main BEFORE calling init
  char* getOption(const char* key);//options should be supplied as key=value
  bool getOptionBool(const char* key);
  //generally speaking options will be optional. So allow the type of the default to provide the type of the output.
  int getOption(const char* key, int def);
  long getOption(const char* key, long def);
  long long getOption(const char* key, long long def);
  unsigned int getOption(const char* key, unsigned int def);
  unsigned long getOption(const char* key, unsigned long def);
  unsigned long long getOption(const char* key, unsigned long long def);

};
