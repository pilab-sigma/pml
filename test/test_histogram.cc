#include <cassert>

#include "pml_histogram.hpp"

using namespace pml;


void test_histogram_creation() {
  std::cout << "test_histogram_creation...\n";

  Vector range = {0, 5, 10};

  Histogram hist(range);

  assert(all(hist.get_range() == range));
  assert(hist.size() == range.size()-1);

  std::cout << "OK.\n";
}

void test_histogram_range_calculation() {
  std::cout << "test_histogram_range_calculation\n";

  Histogram hist({-5, 0, 5, 6, 10});

  assert(hist.find_bin(-5) == 0);
  assert(hist.find_bin(-4) == 0);
  assert(hist.find_bin(0) == 1);
  assert(hist.find_bin(5) == 2);
  assert(hist.find_bin(6) == 3);
  assert(hist.find_bin(9) == 3);

  std::cout << "OK.\n";
}

void test_histogram_operations() {
  std::cout << "test_histogram_operations...\n";

  Histogram hist({0,1,2,3});

  // Can increment by default value
  hist.accumulate(0);
  assert(hist.get(0) == 1);

  // Can increment by any value
  hist.accumulate(0, 2);
  assert(hist.get(0) == 3);

  // Can decrement by -1
  hist.accumulate(0, -1);
  assert(hist.get(0) == 2);

  // Can set value
  hist.set(2, 17);
  assert(hist.get(2) == 17);

  // Can reset
  hist.reset();
  assert(all(hist.get_bins() == Vector::zeros(hist.size())));


  std::cout << "OK.\n";
}

int main(){

  test_histogram_creation();
  test_histogram_range_calculation();
  test_histogram_operations();
  return 0;

}