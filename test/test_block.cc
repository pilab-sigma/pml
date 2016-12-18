#include <cassert>
#include <cmath>
#include <iostream>

#include "pml_memory.hpp"

using namespace pml;
using namespace std;

std::string test_dir = "/tmp/";


inline bool fequal(double a, double b) {
  return std::fabs(a - b) < 1e-6;
}

void test_constructors(){

  std::cout << "test_constructors...\n";

  // Empty block
  Block b;
  assert(b.capacity() == Block::INITIAL_CAPACITY);
  assert(b.size() == 0);

  // Populate with numbers [0, 9]
  for(int i=0; i < 10; ++i)
    b.push_back(i);
  assert(b.size() == 10);


  // Copy constructor (deep copy)
  Block b2(b);
  assert(b2.capacity() == Block::INITIAL_CAPACITY);
  assert(b2.size() == 10);
  assert(b2.data() != b.data());


  // Assigment operator
  Block b3;
  b3 = b;
  assert(b3.capacity() == Block::INITIAL_CAPACITY);
  assert(b3.size() == 10);
  assert(b3.data() != b.data());

  std::cout << "OK.\n";
}


void test_size(){

  std::cout << "test_size...\n";

  Block b(10);
  for(size_t i=0; i<10; ++i)
    b[i] = i;

  assert(b.size() == 10);
  assert(b.capacity() == Block::INITIAL_CAPACITY);
  for(size_t i=0; i<10; ++i)
    assert(fequal(b[i], i));

  // try to shrink but fail
  b.reserve(128);
  assert(b.size() == 10);
  assert(b.capacity() == Block::INITIAL_CAPACITY);
  for(size_t i=0; i<10; ++i)
    assert(fequal(b[i], i));

  // try to shrink with success
  b.shrink_to_fit();
  assert(b.size() == 10);
  assert(b.capacity() == 10);
  for(size_t i=0; i<10; ++i)
    assert(fequal(b[i], i));

  // try to reserve with success
  b.reserve(2048);
  assert(b.size() == 10);
  assert(b.capacity() == 2048);
  for(size_t i=0; i<10; ++i)
    assert(fequal(b[i], i));

  std::cout << "OK.\n";
}

void test_push_back(){

  std::cout << "test_push_back...\n";

  // PART 1: push_back double values
  Block b;
  size_t n = 1025;
  for(size_t i=0; i < n; ++i)
    b.push_back(i);

  // test
  assert(b.size() == n);
  assert(b.capacity() == Block::INITIAL_CAPACITY * Block::GROWTH_RATIO);
  for(size_t i=0; i<n; ++i)
    assert(fequal(b[i], i));


  // PART 2: push_back two small vectors
  Block b2(10);
  for(size_t i=0; i < b2.size(); ++i)
    b2[i] = i;

  Block b3(5);
  for(size_t i=0; i < b3.size(); ++i)
    b3[i] = i;

  b2.push_back(b3);
  assert(b2.size() == 15);
  assert(b3.size() == 5);
  assert(b2.capacity() == Block::INITIAL_CAPACITY);
  for(size_t i=0; i < 10; ++i)
    assert(fequal(b2[i], i));
  for(size_t i=0; i < 5; ++i) {
    assert(fequal(b2[10 + i], i));
    assert(fequal(b3[i], i));
  }

  // PART 2: push_back two large vectors
  Block b4(600), b5(600);
  for(size_t i=0; i<b4.size(); ++i){
    b4[i] = i;
    b5[i] = i;
  }
  b4.push_back(b5);
  assert(b4.size() == 1200);
  assert(b4.capacity() == b4.size() * Block::GROWTH_RATIO);
  for(size_t i=0; i<b5.size(); ++i){
    assert(fequal(b4[i], i));
    assert(fequal(b4[b5.size()+i], i));
  }

  // PART 3: push_back self
  Block b6(10);
  for(size_t i=0; i<b6.size(); ++i)
    b6[i] = i;
  b6.push_back(b6);
  assert(b6.size() == 20);
  assert(b6.capacity() == Block::INITIAL_CAPACITY);
  for(size_t i=0; i<10; ++i){
    assert(fequal(b6[i], i));
    assert(fequal(b6[10+i], i));
  }

  // PART 4: push_back self (larger)
  Block b7(600);
  for(size_t i=0; i<b7.size(); ++i)
    b7[i] = i;
  b7.push_back(b7);
  assert(b7.size() == 1200);
  assert(b7.capacity() == 1200 * Block::GROWTH_RATIO);
  for(size_t i=0; i<600; ++i){
    assert(fequal(b7[i], i));
    assert(fequal(b7[600+i], i));
  }

  std::cout << "OK.\n";
}

void test_iterator(){

  Block b(5);
  for(size_t i=0; i<b.size(); ++i)
    b[i] = i;

  // Test iterator
  for( auto &d : b)
    d += 1;

  for(size_t i=0; i<b.size(); ++i)
    assert(fequal(b[i], i+1));

  // Test const iterator
  for(Block::iterator it = b.begin(); it != b.end(); ++it){
    std::cout << *it << std::endl;
  }

  // Test const iterator
  for(Block::const_iterator it = b.cbegin(); it != b.cend(); ++it){
    std::cout << *it << std::endl;
  }

  Block::iterator it = b.begin();
  Block::iterator it2(it);
  it2 = it;
  Block::const_iterator it4(it);
  it4 = it;


}

int main(){
  test_constructors();
  test_size();
  test_push_back();
  test_iterator();
}