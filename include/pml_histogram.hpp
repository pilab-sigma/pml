#ifndef PML_HISTOGRAM_H_
#define PML_HISTOGRAM_H_

#include "pml_vector.hpp"

namespace pml{

class Histogram {

  public:
    Histogram() {}

    Histogram(const Vector &range_) : range(range_) {
      reset();
    }

    void reset() {
      bins = Vector::zeros(range.size()-1);
    }

    void accumulate(double x, double value = 1) {
      bins[find_bin(x)] += value;
    }

    double get(double x) const {
      return bins[find_bin(x)];
    }

    size_t size() const {
      return bins.size();
    }

    void set(double x, double value) {
      bins[find_bin(x)] = value;
    }

    const Vector& get_range() const {
      return range;
    }

    const Vector& get_bins() const {
      return bins;
    }

    int find_bin(double x) const {
      ASSERT_TRUE(x >= range.first() && x < range.last(),
                  "Histogram Error: Requested bin is out-of-range\n");
      // binary search of course
      int low = 0, high = size();
      while( low < high -1 ){
        int mid = low + 0.5 *(high - low);
        if( x >= range[mid] ){
          low = mid;
        } else {
          high = mid;
        }
      }
      return low;
    }

  private:
    Vector range;
    Vector bins;
};


} // namespace

#endif // PML_HISTOGRAM_H_