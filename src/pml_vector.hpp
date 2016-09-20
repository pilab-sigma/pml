#ifndef PML_VECTOR_H_
#define PML_VECTOR_H_

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <gsl/gsl_cblas.h>
#include <gsl/gsl_sf_psi.h>
#include <iomanip>
#include <iostream>
#include <vector>

#define DEFAULT_PRECISION 6

namespace pml {

  // Helpers:
  inline bool fequal(double a, double b) {
    return fabs(a - b) < 1e-6;
  }

  inline void ASSERT_TRUE(bool condition, const std::string &message) {
    if (!condition) {
      std::cout << "FATAL ERROR: " << message << std::endl;
      exit(-1);
    }
  }

  // Defines a series from start to stop exclusively with step size step:
  // [start, start+step, start+2*step, ...]
  struct Range {
    Range(size_t start_, size_t stop_, size_t step_ = 1)
        : start(start_), stop(stop_), step(step_) { }
    size_t start, stop, step;
  };

  class Vector {
    public:
      // Empty Vector
      Vector() { }

      // Vector of given length and default value.
      explicit Vector(size_t length, double value = 0)
          : data_(length, value) { }

      // Vector from given array
      Vector(size_t length, const double *values)
          : data_(length) {
        memcpy(this->data(), values, sizeof(double) * length);
      }

      // Vector from initializer lsit
      Vector(const std::initializer_list<double> &values)
          : data_(values) { }

      // Vector from range
      explicit Vector(Range range) {
        for (double d = range.start; d < range.stop; d += range.step) {
          data_.push_back(d);
        }
      }

      // Vector of zeros of given length.
      static Vector zeros(size_t length) {
        return Vector(length, 0.0);
      }

      // Vector of ones of given length.
      static Vector ones(size_t length) {
        return Vector(length, 1.0);
      }

    public:
      // Returns length of the Vector.
      size_t size() const {
        return data_.size();
      }

      // Checks empty.
      bool empty() const {
        return data_.empty();
      }

      // Vector resize. If new size is smaller, the data_ is cropped.
      // If new size is larger, garbage values are appended.
      void resize(size_t new_size) {
        data_.resize(new_size);
      }

    public:

      // Push to the end.
      void push_back(double value) {
        data_.push_back(value);
      }

      // Pop from end.
      void pop_back() {
        data_.pop_back();
      }

      // Append a single value. (same as push_back)
      void append(double value) {
        data_.push_back(value);
      }

      // Append a Vector
      void append(const Vector &v) {
        data_.insert(data_.end(), v.data_.begin(), v.data_.end());
      }


    public:
      friend bool operator==(const Vector &x, double v) {
        for(double d : x)
          if (!fequal(d, v)) return false;
        return true;
      }

      friend bool operator==(const Vector &x, const Vector &y) {
        // Check sizes
        if (x.size() != y.size()) return false;
        // Check element-wise
        for (size_t i = 0; i < x.size(); ++i) {
          if (!fequal(x(i), y(i))) return false;
        }
        return true;
      }

    public:
      //  -------- Iterators--------
      std::vector<double>::iterator begin() {
        return data_.begin();
      }

      std::vector<double>::const_iterator begin() const {
        return data_.cbegin();
      }

      std::vector<double>::iterator end() {
        return data_.end();
      }

      std::vector<double>::const_iterator end() const {
        return data_.cend();
      }

      // ------- Accessors -------

      inline double &operator[](const size_t i0) {
        return data_[i0];
      }

      inline double operator[](const size_t i0) const {
        return data_[i0];
      }

      inline double &operator()(const size_t i0) {
        return data_[i0];
      }

      inline double operator()(const size_t i0) const {
        return data_[i0];
      }

      double* data() {
        return data_.data();
      }

      const double *data() const {
        return data_.data();
      }

      double first() const {
        return data_.front();
      }

      double& first() {
        return data_.front();
      }

      double last() const {
        return data_.back();
      }

      double& last() {
        return data_.back();
      }

    public:
      // Apply x[i] = f(x[i]) to each element.
      // Function signature: double f(double x)
      void apply(double (*func)(double)) {
        for (auto &value : data_) {
          value = func(value);
        }
      }

      // Apply f to a new Vector
      friend Vector apply(const Vector &x, double (*func)(double)) {
        Vector result(x);
        result.apply(func);
        return result;
      }

      // ------ Self Assignment Operators -------

      void operator+=(double value) {
        for (auto &d : data_) { d += value; }
      }

      // A = A - b
      void operator-=(double value) {
        for (auto &d : data_) { d -= value; }
      }

      // A = A * b
      void operator*=(double value) {
        for (auto &d : data_) { d *= value; }
      }

      // A = A / b
      void operator/=(double value) {
        for (auto &d : data_) { d /= value; }
      }

      // A = A + B
      void operator+=(const Vector &other) {
        ASSERT_TRUE(size() == other.size(),
                    "Vector::operator+=:: Size mismatch.");
        for (size_t i = 0; i < data_.size(); ++i) {
          data_[i] += other[i];
        }
      }

      // A = A - B
      void operator-=(const Vector &other) {
        ASSERT_TRUE(size() == other.size(),
                    "Vector::operator-=:: Size mismatch.");
        for (size_t i = 0; i < data_.size(); ++i) {
          data_[i] -= other[i];
        }
      }

      // A = A * B (elementwise)
      void operator*=(const Vector &other) {
        ASSERT_TRUE(size() == other.size(),
                    "Vector::operator*=:: Size mismatch.");
        for (size_t i = 0; i < data_.size(); ++i) {
          data_[i] *= other[i];
        }
      }

      // A = A / B (elementwise)
      void operator/=(const Vector &other) {
        ASSERT_TRUE(size() == other.size(),
                    "Vector::operator/=:: Size mismatch.");
        for (size_t i = 0; i < data_.size(); ++i) {
          data_[i] /= other[i];
        }
      }

      // ------ Vector - Double Operations -------

    public:

      // Returns A + b
      friend Vector operator+(const Vector &x, double value) {
        Vector result(x);
        result += value;
        return result;
      }

      // Returns b + A
      friend Vector operator+(double value, const Vector &x) {
        return x + value;
      }

      // Returns A * b
      friend Vector operator*(const Vector &x, double value) {
        Vector result(x);
        result *= value;
        return result;
      }

      // Returns b * A
      friend Vector operator*(double value, const Vector &x) {
        return x * value;
      }

      // Returns A - b
      friend Vector operator-(const Vector &x, double value) {
        Vector result(x);
        result -= value;
        return result;
      }

      // Returns b - A
      friend Vector operator-(double value, const Vector &x) {
        return (-1 * x) + value;
      }

      // returns A / b
      friend Vector operator/(const Vector &x, double value) {
        Vector result(x);
        result /= value;
        return result;
      }

      // returns b / A
      friend Vector operator/(double value, const Vector &x) {
        Vector result(x);
        for (auto &d : result) { d = value / d; }
        return result;
      }

      // ------ Vector - Vector Operations -------

      // R = A + B
      friend Vector operator+(const Vector &x, const Vector &y) {
        ASSERT_TRUE(x.size() == y.size(), "Vector::operator+:: Size mismatch.");
        Vector result(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
          result.data_[i] = x.data_[i] + y.data_[i];
        }
        return result;
      }

      // R = A - B
      friend Vector operator-(const Vector &x, const Vector &y) {
        ASSERT_TRUE(x.size() == y.size(), "Vector::operator-:: Size mismatch.");
        Vector result(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
          result.data_[i] = x.data_[i] - y.data_[i];
        }
        return result;
      }

      // R = A * B (elementwise)
      friend Vector operator*(const Vector &x, const Vector &y) {
        ASSERT_TRUE(x.size() == y.size(), "Vector::operator*:: Size mismatch.");
        Vector result(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
          result.data_[i] = x.data_[i] * y.data_[i];
        }
        return result;
      }

      // R = A / B (elementwise)
      friend Vector operator/(const Vector &x, const Vector &y) {
        ASSERT_TRUE(x.size() == y.size(), "Vector::operator/:: Size mismatch.");
        Vector result(x.size());
        for (size_t i = 0; i < x.size(); ++i) {
          result.data_[i] = x.data_[i] / y.data_[i];
        }
        return result;
      }

      // Load and Save
      friend std::ostream &operator<<(std::ostream &out,
                                      const Vector &x) {
        out << std::setprecision(DEFAULT_PRECISION) << std::fixed;
        for (auto &value : x) {
          out << value << "  ";
        }
        return out;
      }

      friend std::istream &operator>>(std::istream &in, Vector &x) {
        for (auto &value : x) {
          in >> value;
        }
        return in;
      }

      void saveTxt(const std::string &filename,
                   int precision = DEFAULT_PRECISION) const {
        std::ofstream ofs(filename);
        if (ofs.is_open()) {
          ofs << 1 << std::endl;      // dimension
          ofs << size() << std::endl; // size
          ofs << std::setprecision(precision) << std::fixed;
          for (auto &value : data_) {
            ofs << value << std::endl;
          }
          ofs.close();
        }
      }

      static Vector loadTxt(const std::string &filename) {
        Vector result;
        std::ifstream ifs(filename);
        size_t buffer;
        if (ifs.is_open()) {
          // Read dimension
          ifs >> buffer;
          ASSERT_TRUE(buffer == 1, "Vector::LoadTxt:: Dimension mismatch.");
          ifs >> buffer;
          // Allocate memory
          result.data_.resize(buffer);
          ifs >> result;
          ifs.close();
        }
        return result;
      }

    public:
      std::vector<double> data_;
  };

  // Sum
  inline double sum(const Vector &x){
    return std::accumulate(x.begin(), x.end(), 0.0);
  }

  inline Vector pow(const Vector &x, double p = 2){
    Vector result(x);
    for(double &d : result)
      d = std::pow(d, p);
    return result;
  }

  //Min
  inline double min(const Vector &x) {
    return *(std::min_element(x.begin(), x.end()));
  }

  // Max
  inline double max(const Vector &x) {
    return *(std::max_element(x.begin(), x.end()));
  }

  // Dot product
  inline double dot(const Vector &x, const Vector &y) {
    return sum(x * y);
  }

  // Mean
  inline double mean(const Vector &x){
    return sum(x) / x.size();
  }

  // Variance
  inline double var(const Vector &x){
    return sum(pow(x - mean(x), 2)) / (x.size() - 1);
  }

  // Standard deviation
  inline double stdev(const Vector &x){
    return std::sqrt(var(x));
  }

  // Absolute value of x
  inline Vector abs(const Vector &x){
    return apply(x, std::fabs);
  }

  // Round to nearest integer
  inline Vector round(const Vector &x){
    return apply(x, std::round);
  }

  // Log Gamma function.
  inline Vector lgamma(const Vector &x){
    return apply(x, std::lgamma);
  }

  // Polygamma Function.
  inline Vector psi(const Vector &x, int n = 1){
    Vector y(x.size());
    for(size_t i=0; i<y.size(); i++) {
      y(i) = gsl_sf_psi_n(n, x(i));
    }
    return y;
  }

  // Exponential
  inline Vector exp(const Vector &x){
    return apply(x, std::exp);
  }

  // Logarithm
  inline Vector log(const Vector &x){
    return apply(x, std::log);
  }

  // Normalize
  inline Vector normalize(const Vector &x) {
    return x / sum(x);
  }

  // Safe normalize(exp(x))
  inline Vector normalizeExp(const Vector &x) {
    return normalize(exp(x - max(x)));
  }

  // Safe log(sum(exp(x)))
  inline double logSumExp(const Vector &x) {
    double x_max = max(x);
    return x_max + std::log(sum(exp(x - x_max)));
  }

  // Vector slice
  inline Vector slice(const Vector &v, const Range &range){
    Vector result;
    for(size_t i = range.start; i < range.stop; i+=range.step){
      result.append(v[i]);
    }
    return result;
  }

  // KL Divergence Between Vectors
  // ToDo: Maybe move to linalgebra
  inline double kl_div(const Vector &x, const Vector &y) {
    ASSERT_TRUE(x.size() == y.size(), "kl_div:: Size mismatch.");
    double result = 0;
    for (size_t i = 0; i < x.size(); ++i) {
      if(x(i) > 0 && y(i) > 0){
        result += x(i) * (std::log(x(i)) - std::log(y(i))) - x(i) + y(i);
      } else if(x(i) == 0 && y(i) >= 0){
        result += y(i);
      } else {
        result += std::numeric_limits<double>::infinity();
        break;
      }
    }
    return result;
  }

} // namespace pml

#endif