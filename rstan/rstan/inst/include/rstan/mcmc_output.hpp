#ifndef __RSTAN__MCMC_OUTPUT__HPP__
#define __RSTAN__MCMC_OUTPUT__HPP__

#include <ostream>
#include <iomanip>

#include <vector>
#include <string>

#include <stan/mcmc/sample.hpp>
#include <stan/mcmc/base_mcmc.hpp>
#include <stan/model/prob_grad.hpp>

// ref: 
// #include <stan/io/mcmc_writer.hpp>

namespace rstan {
  template <class M>
  class mcmc_output {
  private:
    std::ostream* psample_stream_;
    std::ostream* pdiagnostic_stream_;
    std::vector<std::string> sample_names_;
    std::vector<std::string> sampler_names_;
    std::vector<std::string> param_names_;
    size_t index_for_lp_;
  public:
    mcmc_output(std::ostream* psample_stream, 
                std::ostream* pdiagnostic_stream) : 
      psample_stream_(psample_stream), 
      pdiagnostic_stream_(pdiagnostic_stream) {
    }

    void set_output_names(stan::mcmc::sample& s, 
                          stan::mcmc::base_mcmc& sampler,
                          M& model,
                          std::vector<std::string>& iter_param_names,
                          std::vector<std::string>& sampler_param_names) { 
      sample_names_.clear();
      s.get_sample_param_names(sample_names_);
      index_for_lp_ = find_index(sample_names_, std::string("lp__")); 
      sampler_names_.clear();
      sampler.get_sampler_param_names(sampler_names_);
      param_names_.clear();
      model.constrained_param_names(param_names_, true, true);
      iter_param_names = sample_names_;
      sampler_param_names = sampler_names_;
    }

    void init_sampler_params(std::vector<Rcpp::NumericVector>& vv, int iter_save) {
      for (size_t i = 0; i < sampler_names_.size(); i++)
        vv.push_back(Rcpp::NumericVector(iter_save));
    } 

    void init_iter_params(std::vector<Rcpp::NumericVector>& vv, int iter_save) {
      for (size_t i = 0; i < sample_names_.size(); i++)
        vv.push_back(Rcpp::NumericVector(iter_save));
    }

    void print_sample_names() { 
      if (!psample_stream_) return;
      (*psample_stream_) << sample_names_.at(0);
      for (size_t i = 1; i < sample_names_.size(); ++i) {
        (*psample_stream_) << "," << sample_names_[i];
      }
      for (size_t i = 0; i < sampler_names_.size(); i++) 
        (*psample_stream_) << "," << sampler_names_[i];
      for (size_t i = 0; i < param_names_.size(); i++) 
        (*psample_stream_) << "," << param_names_[i];
      (*psample_stream_) << std::endl;
    }
    
    void output_sample_params(stan::mcmc::sample& s,
                              stan::mcmc::base_mcmc& sampler, 
                              M& model, 
                              std::vector<Rcpp::NumericVector>& chains, 
                              bool warmup,
                              std::vector<double>& sum_pars,
                              std::vector<Rcpp::NumericVector>& sampler_params, 
                              double& sum_lp,
                              const std::vector<size_t>& qoi_idx,
                              const std::vector<size_t>& midx,
                              int iter_save_i,
                              std::ostream* pstream) {
      std::vector<double> values;
      s.get_sample_params(values);

      std::vector<double> sampler_values;
      sampler.get_sampler_params(sampler_values);
        
      std::vector<double> param_values;
      model.write_array_params_all(const_cast<std::vector<double>&>(s.cont_params()),
                                   const_cast<std::vector<int>&>(s.disc_params()),
                                   param_values, pstream);

      size_t z = 0;
      if (!warmup) {
        for (z = 0; z < param_values.size(); z++)
          sum_pars[z] += param_values[z];
        sum_lp += values.at(index_for_lp_);
      } 
      
      for (z = 0; z < qoi_idx.size() - 1; ++z) {
        chains[z][iter_save_i] = param_values[qoi_idx[z]];
      } 
      chains[z][iter_save_i] = values.at(index_for_lp_);
      for (z = 0; z < sampler_values.size(); z++)
        sampler_params[z][iter_save_i] = sampler_values[z];
      
      if (!psample_stream_) return;
      (*psample_stream_) << values.at(0);
      for (size_t i = 1; i < values.size(); ++i) {
        (*psample_stream_) << "," << values[i];
      }
      for (size_t i = 0; i < sampler_values.size(); ++i)
        (*psample_stream_) << "," << sampler_values[i];
      for (size_t i = 0; i < param_values.size(); ++i)
        (*psample_stream_) << "," << param_values[midx[i]];
      (*psample_stream_) << std::endl;
    }

      
    void output_adapt_finish(stan::mcmc::base_mcmc& sampler,
                             std::string& ainfo) {
      std::stringstream ss;
      ss << "# Adaptation terminated" << std::endl;
      sampler.write_sampler_state(&ss);
      ainfo = ss.str();
      if (psample_stream_) *psample_stream_ << ainfo;
      if (pdiagnostic_stream_) *pdiagnostic_stream_ << ainfo;
    }
      
    void output_diagnostic_names(stan::mcmc::sample& s,
                                 stan::mcmc::base_mcmc& sampler, 
                                 M& model) {
      if (!pdiagnostic_stream_) return;

      std::vector<std::string> names;
      s.get_sample_param_names(names);
      sampler.get_sampler_param_names(names);
      std::vector<std::string> model_names;
      model.unconstrained_param_names(model_names, false, false);
      sampler.get_sampler_diagnostic_names(model_names, names);
        
      (*pdiagnostic_stream_) << names.at(0);
      for (size_t i = 1; i < names.size(); ++i) {
        (*pdiagnostic_stream_) << "," << names.at(i);
      }
      (*pdiagnostic_stream_) << std::endl;
    }
      
    void output_diagnostic_params(stan::mcmc::sample& s, 
                                  stan::mcmc::base_mcmc& sampler) { 
      if (!pdiagnostic_stream_) return;

      std::vector<double> values;
      s.get_sample_params(values);
      sampler.get_sampler_params(values);
      sampler.get_sampler_diagnostics(values); // FIXME,col/row-major order??
      (*pdiagnostic_stream_) << values.at(0);
      for (size_t i = 1; i < values.size(); ++i) {
        (*pdiagnostic_stream_) << "," << values.at(i);
      }
      (*pdiagnostic_stream_) << std::endl;
    }
      
    void output_timing(double warmDeltaT, 
                      double sampleDeltaT, 
                      std::ostream* stream, 
                      std::string comment_prefix = "") {
      if (!stream) return;

      std::string prefix = comment_prefix + "Elapsed Time: ";
      *stream << std::endl
              << prefix << warmDeltaT
              << " seconds (Warm-up)"  << std::endl
              << comment_prefix << std::string(prefix.size() - 1, ' ') << sampleDeltaT
              << " seconds (Sampling)"  << std::endl
              << comment_prefix << std::string(prefix.size() - 1, ' ') << warmDeltaT + sampleDeltaT
              << " seconds (Total)"  << std::endl
              << std::endl;
    }
      
  };

} // rstan

#endif
