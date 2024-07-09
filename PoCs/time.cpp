#include <chrono>
#include <iostream>

//a very simple program to test linking with std libs
int main(int, char**) {
  std::cout << std::chrono::system_clock::now().time_since_epoch() << std::endl;
  return 0;
}

