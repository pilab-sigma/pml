#ifndef MATLIB_PML_BCPM_H
#define MATLIB_PML_BCPM_H

#include "pml.hpp"
#include "pml_rand.hpp"

#include <algorithm>

namespace pml {
  // ----------- POTENTIALS ----------- //

  class DirichletPotential{

    public:
      DirichletPotential(const Vector& alpha_, double log_c_ = 0) :
              alpha(alpha_), log_c(log_c_) {}

    public:
      void operator*=(const DirichletPotential &p){
        *this = this->operator*(p);
      }

      DirichletPotential operator*(const DirichletPotential &p) const{

        double delta = std::lgamma(sum(alpha)) - sum(lgamma(alpha)) +
                       std::lgamma(sum(p.alpha)) - sum(lgamma(p.alpha)) +
                       sum(lgamma(alpha + p.alpha-1)) -
                       std::lgamma(sum(alpha + p.alpha -1));

        return DirichletPotential(alpha + p.alpha - 1,
                                  log_c + p.log_c + delta);
      }

      void update(const Vector &obs){
        double log_c = std::lgamma(sum(obs)+1) - std::lgamma(sum(obs+1));
        this->operator*=(DirichletPotential(obs+1, log_c));
      }

      Vector rand() const{
        return dirichlet::rand(alpha);
      }

      Vector mean() const{
        return normalize(alpha);
      }

    public:
      Vector alpha;
      double log_c;   //log of normalizing constant
  };

  class GammaPotential{
    public:
      GammaPotential(double a_, double b_, double log_c_ = 0)
              : a(a_), b(b_), log_c(log_c_) {}

    public:
      void operator*=(const GammaPotential &other){
        *this = this->operator*(other);
      }

      GammaPotential operator*(const GammaPotential &other) const{
        double delta = std::lgamma(a + other.a - 1)
                       - std::lgamma(a) - std::lgamma(other.a)
                       + std::log(b + other.b)
                       + a * std::log(b/(b + other.b))
                       + other.a * std::log(other.b/(b + other.b));
        return GammaPotential(a + other.a - 1,
                              b + other.b,
                              log_c + other.log_c + delta);
      }

      Vector rand() const{
        return gamma::rand(a, b, 1);
      }

      Vector mean() const{
        return Vector(1, a / b);
      }

      void update(const Vector &obs){
        this->operator*=(GammaPotential(obs.first()+1, 1));
      }

    public:
      double a;
      double b;
      double log_c;   //log of normalizing constant
  };

  // ----------- OBSERVATION MODELS ----------- //

  struct PoissonRandom{
    Vector operator()(const Vector &param) const {
      return poisson::rand(param.first(), 1);
    }
  };

  struct MultinomialRandom{
    Vector operator()(const Vector &param) const {
      return multinomial::rand(param, 100);
    }
  };

  // ----------- MESSAGE----------- //

  template <class P>
  class Message {

    public:
      size_t size() const {
        return components.size();
      }

      void add_component(const P &potential){
        components.push_back(potential);
      }

      void add_component(const P &potential, double log_c){
        components.push_back(potential);
        components.back().log_c = log_c;
      }

      friend Message<P> operator*(const Message<P> &m1, const Message<P> &m2){
        Message<P> msg;
        for(auto &component1 : m1.components)
          for(auto &component2 : m2.components)
            msg.add_component(component1 * component2);
        return msg;
      }

      void update(const Vector& obs) {
        for(auto &component : components){
          component.update(obs);
        }
      }

      // Returns mean and cpp (Maybe renamed)
      std::pair<Vector, double> evaluate(int N = 1){
        Vector consts;
        Matrix params;
        for(auto &potential: components){
          consts.append(potential.log_c);
          params.appendColumn(potential.mean());
        }
        consts = normalizeExp(consts);
        // Calculate mean
        Vector mean = sumRows(transpose(transpose(params)*consts));
        // Calculate cpp as the sum of last N probabilities
        double cpp = 0;
        for(int i=0; i < N; ++i)
          cpp += consts(consts.size() -i - 1);
        return {mean, cpp};
      };

      void prune(int max_components){
        //ToDo: implement prune
      }

    public:
      std::vector<P> components;
  };


  // ----------- MODEL----------- //
  template <class P, class Obs>
  class Model{
    public:
      Model(const P &prior_, double p1_) : prior(prior_), p1(p1_){
        log_p1 = std::log(p1);
        log_p0 = std::log(1-p1);
      }

    public:
      // returns  "states" as first matrix and "obervations" as second
      std::pair<Matrix, Matrix> generateData(size_t T){
        Matrix states, obs;

        Vector state = prior.rand();
        for (size_t t=0; t<T; t++) {
          if (t > 0 && uniform::rand() < p1) {
            state = prior.rand();
          }
          states.appendColumn(state);
          obs.appendColumn(observation(state));
        }
        return {states, obs};
      }

      Message<P> initForward(){
        Message<P> first_message;
        first_message.add_component(prior, log_p0);
        first_message.add_component(prior, log_p1);
        return first_message;
      }

      Message<P> initBackward(){
        Message<P> first_message;
        first_message.add_component(prior, log_p0);
        first_message.add_component(prior, log_p1);
        return first_message;
      }

      Message<P> predict(const Message<P> &prev){
        Message<P> message = prev;
        Vector consts;
        for(auto &component : message.components){
          consts.append(component.log_c);
          component.log_c += log_p0;
        }
        message.add_component(prior, log_p1 + logSumExp(consts));
        return message;
      }

    private:
      P prior;
      Obs observation;
      double p1;                //  probability of change
      double log_p1, log_p0;
  };

  template <class P, class Obs>
  class ForwardBackward {
    public:
      ForwardBackward(const Model<P, Obs> &model_,
                      int lag_ = 0, int max_components_ = 100)
              : model(model_), lag(lag_), max_components(max_components_) {}


    public:
      void oneStepForward(std::vector<Message<P>> &alpha, const Vector& obs) {
        // predict step
        if (alpha.size() == 0) {
          alpha.push_back(model.initForward());
        }
        else {
          alpha.push_back(model.predict(alpha.back()));
        }
        // update step
        alpha.back().update(obs);
      }

      void oneStepBackward(std::vector<Message<P>> &beta, const Vector& obs) {
        // predict step
        if (beta.size()==0) {
          beta.push_back(model.initBackward());
        }
        else {
          beta.push_back(model.predict(beta.back()));
        }
        // update step
        beta.back().update(obs);
      }

      std::vector<Message<P>> forward(const Matrix& obs){
        std::vector<Message<P>> alpha;
        for (size_t i=0; i<obs.ncols(); i++) {
          oneStepForward(alpha, obs.getColumn(i));
          alpha.back().prune(max_components);
        }
        return alpha;
      }

      std::vector<Message<P>> backward(const Matrix& obs){
        std::vector<Message<P>> beta;
        for (size_t i=obs.ncols(); i>0; i--) {
          oneStepBackward(beta, obs.getColumn(i-1));
          beta.back().prune(max_components);
        }
        std::reverse(beta.begin(), beta.end());
        return beta;
      }

      // Returns mean and cpp
      std::pair<Matrix, Vector> filtering(const Matrix& obs) {
        // Run forward
        auto alpha = forward(obs);

        // Calculate mean and cpp
        Matrix mean;
        Vector cpp;
        for(auto &message : alpha){
          auto result = message.evaluate();
          mean.appendColumn(result.first);
          cpp.append(result.second);
        }
        return {mean, cpp};
      }

      // Returns mean and cpp
      std::pair<Matrix, Vector> smoothing(const Matrix& obs) {
        Matrix mean;
        Vector cpp;
        // Run Forward and Backward
        auto alpha = forward(obs);
        auto beta = backward(obs);

        // Calculate Smoothed density
        std::vector<Message<P>> gamma;
        for(size_t i=0; i < obs.ncols(); ++i) {
          gamma.push_back(alpha[i] * beta[i]);
          // evaluate mean and cpp
          auto result = gamma[i].evaluate(beta[i].size());
          mean.appendColumn(result.first);
          cpp.append(result.second);
        }
        return {mean, cpp};
      }

      // a.k.a. fixed-lag smoothing
      std::pair<Matrix, Vector> online_smoothing(const Matrix& obs) {
        if( lag == 0){
          return filtering(obs);
        }
        Matrix mean;
        Vector cpp;
        std::vector<Message<P>> alpha = forward(obs);
        std::vector<Message<P>> gamma;
        for (size_t t=0; t<obs.ncols()-lag+1; t++) {
          auto beta = backward(obs.getColumns(Range(t, t+lag)));
          gamma.push_back(alpha[t] * beta[0]);
          auto result = gamma.back().evaluate(beta[0].size());
          mean.appendColumn(result.first);
          cpp.append(result.second);
          // if T-lag is reached, smooth the rest
          if(t == obs.ncols()-lag){
            for(int i = 1; i < lag; ++i){
              gamma.push_back(alpha[t+i] * beta[i]);
              auto result = gamma.back().evaluate(beta[i].size());
              mean.appendColumn(result.first);
              cpp.append(result.second);
            }
          }
        }
        return {mean, cpp};
      }


    public:
      Model<P, Obs> model;
      int lag;
      int max_components;
  };
    /*


  // ----------- FORWARD-BACKWARD ----------- //

  class ForwardBackward{

      static double loglhood(Model* model, const Matrix& data) {
        ForwardBackward fb(model);
        fb.forwardRecursion(data);
        Vector consts;
        for(Component* component: fb.alpha_update.back()->components){
          consts.append(component->log_c);
        }
        return logSumExp(consts);
      }


    private:
      void freeVec(std::vector<Message*> &vec) {
        for(Message* m : vec) {
          delete m;
        }
        vec.clear();
      }

    // netas-related code
    public:
      size_t getTime() { return data.ncols(); }

      void processObs(const Vector& obs) {
        // predict & update
        oneStepForward(obs);
        // parameter updates
        data.appendColumn(obs);
        std::pair<Vector, double> mean_cpp = model->eval_mean_cpp(alpha_update.back());
        mean.appendColumn(mean_cpp.first);
        cpp.append(mean_cpp.second);
        // smoothing
        if (lag>0 && ((int)getTime()) >= lag) {
          fixedLagSmooth();
        }
        // pruning
        if ((int) alpha_update.back()->components.size() > max_components) {
          prun(alpha_update.back());
        }
      }

      // needs better implementation via heap
      void prun(Message* msg) {
        while (msg->components.size() > (unsigned) max_components) {
          std::vector<Component*> &comps = msg->components;
          double min_c = comps[0]->log_c;
          int min_id = 0;
          for (size_t i=1; i<comps.size(); i++) {
            if (comps[i]->log_c < min_c) {
              min_c = comps[i]->log_c;
              min_id = i;
            }
          }
          comps.erase(comps.begin() + min_id);
        }
      }

      void fixedLagSmooth() {
        size_t T = getTime();
        std::vector<Message*> _beta_postdict;
        std::vector<Message*> _beta_update;
        // lag step backward
        // TODO: implement below loop via ForwardBackward class and oneStepBackward()
        for (int t=0; t<lag; t++){
          if (t==0) {
            _beta_postdict.push_back(model->initBackward());
          }
          else {
            _beta_postdict.push_back(model->predict(_beta_update.back()));
          }
          _beta_update.push_back(model->update(_beta_postdict.back(), data.getColumn(T-1-t)));
        }
        // CHECK HERE AGAIN: posterior is calculated for t = T - Lag
        size_t t = T - lag;
        auto res = model->multiply(alpha_update[t], _beta_postdict.back());
        delete std::get<0>(res);
        mean.setColumn(t, std::get<1>(res));
        cpp(t) = std::get<2>(res);
        freeVec(_beta_postdict);
        freeVec(_beta_update);
      }

    public:
      Model *model;
      int lag;
      int max_components;
      // messages
      std::vector<Message*> alpha_predict;       // alpha_{t|t-1}
      std::vector<Message*> alpha_update;        // alpha_{t|t}
      std::vector<Message*> beta_postdict;       // beta_{t|t+1}
      std::vector<Message*> beta_update;         // beta_{t|t}
      // results
      Vector cpp;                                // p(r_t=1)
      Matrix mean;                               // for saving results
      Matrix data;                               // for saving results
      std::vector<Message*> smoothed_msgs;       // alpha_{t|t}*beta_{t|t+1}
    };
*/
}

#endif //MATLIB_PML_BCPM_H
