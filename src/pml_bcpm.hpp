#ifndef MATLIB_PML_BCPM_H
#define MATLIB_PML_BCPM_H

#include "pml.hpp"
#include "pml_rand.hpp"

using namespace std;

namespace pml {

  /*****************************
  ********* COMPONENT **********
  *****************************/
  class Component {
    public:
      double log_c;
      virtual ~Component() {} // bu ve child destructor'lar olmadıgında leak veriyor...
      Component(double _log_c_) : log_c(_log_c_) {}
    };

  class DirichletComponent : public Component {
    public:
      Vector alpha;
      DirichletComponent(const Vector& _alpha_, double _log_c_) : Component(_log_c_), alpha(_alpha_) { }
      ~DirichletComponent() {  }
    };

  class GammaComponent : public Component {
    public:
      double a;
      double b;
      GammaComponent(double _a_, double _b_, double _log_c_) : Component(_log_c_), a(_a_), b(_b_) {}
      ~GammaComponent() {}
    };

  /*****************************
  ********** MESSAGE ***********
  *****************************/
  class Message {
    public:
      ~Message() { for (Component* c : components) delete c;  }
      size_t size() { return components.size(); }
    public:
      vector<Component*> components;
    };

  class DirichletMessage : public Message {
    public:
      DirichletMessage() {}
      DirichletMessage(const Vector &param) {
        DirichletComponent* dc = new DirichletComponent(param, 0);
        components.push_back(dc);
      }
    };

  class GammaMessage : public Message {
    public:
      GammaMessage(double a, double b) {
        GammaComponent* gc = new GammaComponent(a,b,0);
        components.push_back(gc);
      }
    };

  /*****************************
  *********** MODEL ************
  *****************************/
  class Model{
    public:
      Model(double _p1_) : p1(_p1_){
        log_p1 = std::log(p1);
        log_p0 = std::log(1-p1);
      }
    public:
      virtual pair<Matrix, Vector> generateData(size_t T) = 0;
      virtual Message* init() = 0;
      virtual Message* predict(const Message* message) = 0;
      virtual Message* update(const Message* message, const Vector& data) = 0;
      virtual pair<Vector,double> eval_mean_cpp(const Message* message) = 0;
      virtual pair<Vector,double> multiply(const Message* forward, const Message* backward) = 0;
  
    public:
      double p1;           //  probability of change
      double log_p1;       //  log probability of change
      double log_p0;       //  log probability of no change
  };

  class DirichletModel : public Model{
    public:
      DirichletModel(double _p1_, const Vector &_alpha_)
            : Model(_p1_), alpha(_alpha_){}
  
    public:
      pair<Matrix, Vector> generateData(size_t T) {
        Vector cps = Vector::zeros(T);                  // change points
        Matrix pi = Matrix::zeros(alpha.size(), T);     // parameters that generates data
        Matrix data = Matrix::zeros(alpha.size(), T);   // data
  
        Vector pi_0 = dirichlet::rand(alpha);
        for (size_t t=0; t<T; t++) {
          // change point
          if (uniform::rand() < p1) {
            cps(t) = 1;
            pi.setColumn(t, dirichlet::rand(alpha));
          }
          // not a change point
          else {
            cps(t) = 0;
            if (t==0) {
              pi.setColumn(t, pi_0);
            }
            else {
              pi.setColumn(t, pi.getColumn(t-1));
            }
          }
          data.setColumn(t, multinomial::rand(pi.getColumn(t),uniform::randi(40,60)));
        }
        return make_pair(data, cps);
      }
      Message* init() {
        Message* ptr = new DirichletMessage(alpha);
        return ptr;
      }
  
      Message* predict(const Message* message) {
        DirichletMessage* dm = new DirichletMessage();
        for (size_t i=0; i<message->components.size(); ++i) {
          DirichletComponent* d = static_cast<DirichletComponent*>(message->components[i]);
          dm->components.push_back(new DirichletComponent(d->alpha, d->log_c));
        }
        Vector consts(dm->components.size());
        for (size_t i=0; i<dm->components.size(); i++) {
          consts(i) = dm->components[i]->log_c;
        }
        DirichletComponent* dc = new DirichletComponent(alpha, log_p1 + logSumExp(consts));
        dm->components.push_back(dc);
        return dm;
      };
  
      // http://stackoverflow.com/questions/332030/when-should-static-cast-dynamic-cast-const-cast-and-reinterpret-cast-be-used
      Message* update(const Message* message, const Vector& data) {
        DirichletMessage* dm = new DirichletMessage();
        for (size_t i=0; i<message->components.size(); ++i) {
          DirichletComponent* d = static_cast<DirichletComponent*>(message->components[i]);
          dm->components.push_back(new DirichletComponent(d->alpha, d->log_c));
        }
        for (size_t i=0; i<dm->components.size(); ++i) {
          DirichletComponent* d = static_cast<DirichletComponent*>(dm->components[i]);
          d->log_c += dirMultNormConsant(data,d->alpha);
          d->alpha += data;
        }
        return dm;
      }
    
      pair<Vector,double> eval_mean_cpp(const Message* message) {
        Vector consts;
        Matrix norm_params;
        for(size_t i=0; i < message->components.size(); ++i) {
          DirichletComponent* d = static_cast<DirichletComponent*>(message->components[i]);
          consts.append(d->log_c);
          norm_params.appendColumn(normalize(d->alpha));
        }
        Vector exp_consts = exp(consts - max(consts));
        Vector norm_const = normalize(exp_consts);
        Vector mean = sum(transpose(transpose(norm_params)*norm_const), 1);
        return make_pair(mean, norm_const.last());
      }
  
  
      pair<Vector,double> multiply(const Message* forward, const Message* backward) {
  
        Vector noChangeNormConstant, changeNormConstant;
        Message* smoothed_msg = new DirichletMessage();
        // no change particles
        for (size_t i=0; i<forward->components.size()-1; i++) {
          DirichletComponent* fc = static_cast<DirichletComponent*>(forward->components[i]);
          for (size_t j=0; j<backward->components.size(); j++) {
            DirichletComponent* bc = static_cast<DirichletComponent*>(backward->components[j]);
            double c = fc->log_c  + bc->log_c + dirDirNormConsant(fc->alpha, bc->alpha);
            noChangeNormConstant.append(c);
            Vector alpha = fc->alpha + bc->alpha - 1;
            smoothed_msg->components.push_back(new DirichletComponent(alpha,c));
          }
        }
        // change particles
        DirichletComponent* last = static_cast<DirichletComponent*>(forward->components.back());
        for (size_t i=0; i<backward->components.size(); i++) {
          DirichletComponent *bc = static_cast<DirichletComponent *>(backward->components[i]);
          double c = last->log_c  + bc->log_c + dirDirNormConsant(last->alpha, bc->alpha);
          changeNormConstant.append(c);
          Vector alpha = last->alpha + bc->alpha - 1;
          smoothed_msg->components.push_back(new DirichletComponent(alpha,c));
        }
        Vector mean = eval_mean_cpp(smoothed_msg).first;
        delete smoothed_msg;
        double logPNoChange = logSumExp(noChangeNormConstant);
        double logPChange = logSumExp(changeNormConstant);
        Vector tmp = normalizeExp(Vector{logPChange, logPNoChange});
        return make_pair(mean,tmp(0));
      };


    private:
      // Elementwise Log Gamma And Sum
      // passing lgamma as parameter to apply function not working -- return value is float, not double. sorry baris.
      double ELGAS(const Vector& vec) {
        Vector tmp(vec.size());
        for (size_t i=0; i<vec.size(); i++) {
          tmp(i) = lgamma(vec(i));
        }
        return sum(tmp);
      }

      double dirMultNormConsant(const Vector &data, const Vector &params) {
        double term1 = lgamma(sum(data) + 1);
        double term2 = ELGAS(data+1);
        double term3 = lgamma(sum(params));
        double term4 = ELGAS(params);
        double term5 = ELGAS(data + params);
        double term6 = lgamma(sum(data + params));
        // cout << term1 << endl << term2 << endl << term3 << endl << term4 << endl << term5 << endl << term6 << endl;
        return term1 - term2 + term3 - term4 + term5 - term6;
      }

      double dirDirNormConsant(const Vector &params1, const Vector &params2){
        double term1 = lgamma(sum(params1));
        double term2 = ELGAS(params1);
        double term3 = lgamma(sum(params2));
        double term4 = ELGAS(params2);
        double term5 = ELGAS(params1 + params2 -1);
        double term6 = lgamma(sum(params1 + params2 -1));
        // cout << term1 << endl << term2 << endl << term3 << endl << term4 << endl << term5 << endl << term6 << endl;
        return term1 - term2 + term3 - term4 + term5 - term6;
      }
  
    public:
      Vector alpha;
  };

  class GammaModel : public Model{
    public:
      GammaModel(double _p1_, double a_, double b_)
            : Model(_p1_), a(a_), b(b_){}
  
    public:
      pair<Matrix, Vector> generateData(size_t T) {
        return make_pair(uniform::rand(20,20),uniform::rand(20));
      }

      Message* init() {
        return new GammaMessage(a,b);
      }

      Message* predict(const Message* message) {
        return new GammaMessage(a,b);
        // ToDo: Taha'nin ellerinden oper.
      }

      Message* update(const Message* message, const Vector& data) {
        return new GammaMessage(a,b);
        // ToDo: Taha'nin ellerinden oper.
      }

      pair<Vector,double> eval_mean_cpp(const Message* message) {
        return make_pair(Vector::ones(10),10);
        // ToDo: Taha'nin ellerinden oper.
      }

      pair<Vector,double> multiply(const Message* forward, const Message* backward) {
        return make_pair(Vector::ones(10),10);
        // ToDo: Taha'nin ellerinden oper.
      }
  
    public:
      double a;  // shape parameter
      double b;  // rate parameter
  };

  /*****************************
  ****** FORWARD-BACKWARD ******
  *****************************/
  class ForwardBackward{
    public:
      ForwardBackward(Model *model_, int lag_,
                      int max_components_=100):
              model(model_), lag(lag_),
              max_components(max_components_) { }

      ~ForwardBackward() {
        for(Message* m : alpha_update) {
          delete m;
        }
        for(Message* m : alpha_predict) {
          delete m;
        }
        for(Message* m : beta_postdict) {
          delete m;
        }
        for(Message* m : beta_update) {
          delete m;
        }
      }

    // forward-backward in general
    public:
      void oneStepForward(const Vector& obs) {
        // predict step
        if (alpha_predict.size() == 0) {
          Message* message = model->init();        // start forward recursion
          alpha_predict.push_back(message);
        }
        else {
          // calculate \alpha_{t+1,t}
          Message* new_pred_msg = model->predict(alpha_update.back());
          alpha_predict.push_back(new_pred_msg);
        }

        // update step, calculate \alpha_{t+1,t+1}
        Message* new_update_msg = model->update(alpha_predict.back(), obs);
        alpha_update.push_back(new_update_msg);
      }

      void oneStepBackward(const Vector& obs) {
        if (beta_postdict.size()==0) {
          Message* message = model->init();
          beta_postdict.push_back(message);
        }
        else {
          Message* new_pred_msg = model->predict(beta_update.back());
          beta_postdict.push_back(new_pred_msg);
        }
        Message* new_update_msg = model->update(beta_postdict.back(), obs);
        beta_update.push_back(new_update_msg);
      }

      void forwardRecursion(const Matrix& data) {
        for (size_t i=0; i<data.ncols(); i++) {
          oneStepForward(data.getColumn(i));
        }
      }

      void backwardRecursion(const Matrix& data) {
        for (size_t i=data.ncols(); i>0; i--) {
          oneStepBackward(data.getColumn(i-1));
        }
      }

      void smoothing(const Matrix &data) {
        forwardRecursion(data);
        backwardRecursion(data);

        pair<Vector,double> res;
        size_t T = data.ncols();

        res = model->eval_mean_cpp(beta_postdict.back());
        mean.appendColumn(res.first);
        cpp.append(res.second);

        for (size_t t=1; t<T-1; t++) {
          res = model->multiply(alpha_update[t],beta_postdict[T-t-1]);
          mean.appendColumn(res.first);
          cpp.append(res.second);
        }

        res = model->eval_mean_cpp(alpha_update.back());
        mean.appendColumn(res.first);
        cpp.append(res.second);

      }

    // netas-related code
    public:
      size_t getTime() { return data.ncols(); }

      void processObs(const Vector& obs) {
        // predict & update
        oneStepForward(obs);

        // parameter updates
        data.appendColumn(obs);
        pair<Vector, double> mean_cpp = model->eval_mean_cpp(alpha_update.back());
        mean.appendColumn(mean_cpp.first);
        cpp.append(mean_cpp.second);

        // smoothing
        if (lag>0 && ((int)getTime()) >= lag) {
          fixedLagSmooth();
        }

        // pruning
      }

      void fixedLagSmooth() {
        size_t T = getTime();
        vector<Message*> _beta_postdict;
        vector<Message*> _beta_update;
        for (int t=0; t<lag; t++){
          if (t==0) {
            Message* message = model->init();
            _beta_postdict.push_back(message);
          }
          else {
            Message* new_pred_msg = model->predict(_beta_update.back());
            _beta_postdict.push_back(new_pred_msg);
          }
          Message* new_update_msg = model->update(_beta_postdict.back(), data.getColumn(T-1-t));
          _beta_update.push_back(new_update_msg);
        }
        // CHECK HERE AGAIN: posterior is calculated for t = T - Lag
        size_t t = T - lag;
        pair<Vector,double> mean_cpp = model->multiply(alpha_update[t], _beta_postdict[T-1-t]);
        mean.setColumn(t, mean_cpp.first);
        cpp(t) = mean_cpp.second;

        for (size_t i=0; i<_beta_postdict.size(); i++) {
          delete _beta_postdict[i];
          delete _beta_update[i];
        }
      }
      /*
       * TODO: implement fixedLagSmooth() via ForwardBackward class and oneStepBackward()
       */

    public:
      Model *model;
      int lag;
      int max_components;

      vector<Message*> alpha_predict;       // alpha_{t|t-1}
      vector<Message*> alpha_update;        // alpha_{t|t}
      vector<Message*> beta_postdict;       // beta_{t+1|t}
      vector<Message*> beta_update;         // beta_{t|t}

      Vector cpp;                           // p(r_t=1)
      Matrix mean;                          // for saving results
      Matrix data;                          // for saving results
    };
}

#endif //MATLIB_PML_BCPM_H