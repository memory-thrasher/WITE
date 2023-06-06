#include <assert.h>
#include <string.h> //memset
#include <cmath>

#include "../WITE/LinkedTree.hpp"

int main(int argc, char** argv) {
  WITE::Collections::LinkedTree<double> lt;
  static constexpr size_t CNT = 256;
  double data[CNT];
  for(size_t i = 0;i < CNT;i++) {
    data[i] = i;
    assert(!lt.contains(&data[i]));
    lt.insert(&data[i]);
    assert(lt.contains(&data[i]));
  }
  for(size_t i = 1;i < CNT;i+=2) {//note: odd only
    assert(lt.contains(&data[i]));
    lt.remove(&data[i]);
    assert(!lt.contains(&data[i]));
  }
  for(size_t i = 0;i < CNT;i++) {
    assert(lt.contains(&data[i]) ^ (i & 1));
  }
  size_t roster[CNT];
  memset(roster, 0, sizeof(roster));
  auto iter = lt.iterate();//nonstandard iterator, never ends, may sporadically skip elements, resists concurrent modification
  for(size_t i = 0;i < CNT*4;i++) {
    double* ptr = iter();
    assert(ptr);//should never return null unless the list is empty
    size_t idx = (size_t)lround(*ptr + 0.4);
    roster[idx]++;
  }
  for(size_t i = 0;i < CNT;i++) {
    assert((roster[i] > 0) ^ (i & 1));
  }
  memset(roster, 0, sizeof(roster));
  size_t idx = 0;
  while(true) {
    double* d = iter();
    if(!d) break;
    if((idx & 5) == 0)
      lt.remove(d);
    idx++;
    assert(idx < 10000000);
  }
  assert(lt.isEmpty());
  assert(lt.iterate()() == NULL);
  for(size_t i = 0;i < CNT;i++) {
    assert(!lt.contains(&data[i]));
    lt.insert(&data[i]);
    assert(lt.contains(&data[i]));
  }
  for(size_t i = 0;i < CNT;i++) {
    assert(lt.contains(&data[i]));
  }
  auto tempIter = lt.iterate();
  for(size_t w = 0;w < 100;w++) {
    for(size_t i = 0;i < CNT;i++) {
      for(size_t j = 0;j < i;j++)
	tempIter();
      lt.remove(tempIter());//remove one, deterministically
      auto temp = iter();
      for(size_t j = i+2;j < CNT;j++)
	iter();
      assert(temp == iter());
    }
    for(size_t i = 0;i < CNT;i++) {
      assert(!lt.contains(&data[i]));
      lt.insert(&data[i]);
      assert(lt.contains(&data[i]));
    }
  }
};



