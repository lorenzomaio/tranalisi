#ifndef _MINIMIZER_HPP
#define _MINIMIZER_HPP

#ifdef HAVE_CONFIG_H
 #include "config.hpp"
#endif

#ifndef EXTERN_MINIMIZER
 #define EXTERN_MINIMIZER extern
 #define INIT_TO(A)
#else
 #define INIT_TO(A) =A
#endif

#include <string>
#include <vector>

using namespace std;

//////////////////////////////////////////////// Minuit2 implementation //////////////////////////

#if MINIMIZER_TYPE == MINUIT

#include <cify.hpp>
#include <TMinuit.h>

//! host a single par
class par_t
{
 public:
  string name;
  double val;
  double err;
  bool is_fixed;
  
  par_t(const string &name,double val,double err) : name(name),val(val),err(err),is_fixed(false) {}
};

//! base class
class minimizer_fun_t
{
public:
  virtual ~minimizer_fun_t() {}
  virtual double operator()(const std::vector<double>& x) const=0;
  virtual double Up() const=0;
};

//! wrapper against minimization pars
class minimizer_pars_t
{
public:
  vector<par_t> pars;
  
  //! add a parameter
  void add(const string &name,double val,double err)
  {pars.push_back(par_t(name,val,err));}
  
  //! set a parameter
  void set(int ipar,double val)
  {pars[ipar].val=val;}
  
  //! fix a parameter
  void fix(int ipar)
  {pars[ipar].is_fixed=false;}
  
  //! fix a parameter to a given value
  void fix_to(int ipar,double val);
  
  //! return the number of parameters
  size_t size() const
  {return pars.size();}
};

//! pointer used by fcn
EXTERN_MINIMIZER const minimizer_fun_t *fun_ptr;
//! wrapper to the function
EXTERN_MINIMIZER void fcn(int &npar,double *fuf,double &ch,double *p,int flag);

//! wrapper against minimization class
class minimizer_t
{
  //! function to be minimized
  const minimizer_fun_t &fun;
  
  //! root minimizer
  TMinuit minu;
  minimizer_t();
  
 public:
  //construct from migrad
  minimizer_t(const minimizer_fun_t &fun,const minimizer_pars_t &pars) : fun(fun)
  {
    //set the external pointer
    fun_ptr=&fun;
    minu.SetFCN(fcn);
    for(size_t ipar=0;ipar<pars.size();ipar++)
      {
	minu.DefineParameter(ipar,pars.pars[ipar].name.c_str(),pars.pars[ipar].val,pars.pars[ipar].err,0.0,0.0);
	if(pars.pars[ipar].is_fixed) minu.FixParameter(ipar);
      }
  }
  
  //! minimizer
  vector<double> minimize()
  {
    int npars=minu.GetNumPars();
    vector<double> pars(npars);
    minu.Migrad();
    double dum;
    for(int it=0;it<npars;it++)
      minu.GetParameter(it,pars[it],dum);
    return pars;
  }
  
  //! evaluate the function
  double eval(const vector<double> &pars)
  {
    double res;
    double in_pars[pars.size()];
    for(size_t it=0;it<pars.size();it++) in_pars[it]=pars[it];
    minu.Eval(pars.size(),NULL,res,in_pars,0);
    return res;
  }
};

//! set the level of verbosity

inline void set_printlevel(int lev)
{// glb_print_level=lev;
}

#endif

//////////////////////////////////////////////// minuit2 ///////////////////////////////////////////

#if MINIMIZER_TYPE == MINUIT2

// #include <Math/MinimizerOptions.h>
#include <Minuit2/FCNBase.h>
#include <Minuit2/MnMigrad.h>
#include <Minuit2/MnUserParameters.h>
#include <Minuit2/FunctionMinimum.h>
#include <Minuit2/MnPrint.h>

using minimizer_fun_t=ROOT::Minuit2::FCNBase;

//! wrapper against minimization pars
class minimizer_pars_t
{
public:
  ROOT::Minuit2::MnUserParameters pars;
  
  //! add a parameter
  void add(const string &name,double val,double err)
  {pars.Add(name,val,err);}
  
  //! set a parameter
  void set(int ipar,double val)
  {pars.SetValue(ipar,val);}
  
  //! fix a parameter
  void fix(int ipar)
  {pars.Fix(ipar);}
  
  //! fix a parameter to a given value
  void fix_to(int ipar,double val);
  
  //! return the number of parameters
  size_t size()
  {return pars.Parameters().size();}
};

//! wrapper against minimization class
class minimizer_t
{
  ROOT::Minuit2::MnMigrad migrad;
  minimizer_t();
  
 public:
  //construct from migrad
  minimizer_t(const minimizer_fun_t &fun,const minimizer_pars_t &pars) : migrad(fun,pars.pars) {}
  
  //! minimizer
  vector<double> minimize()
  {
    //call minimizer
    ROOT::Minuit2::FunctionMinimum min=migrad();
    ROOT::Minuit2::MinimumParameters par_min=min.Parameters();
    
    //copy the result
    size_t size=par_min.Vec().size();
    vector<double> pars(size);
    for(size_t it=0;it<size;it++) pars[it]=par_min.Vec()[it];
    
    return pars;
  }
  
  //! evaluate the function
  double eval(const vector<double> &pars)
  {return migrad().Fval();}
};

//! set the level of verbosity
inline void set_printlevel(int lev)
{ROOT::Minuit2::MnPrint::SetLevel(lev);}

#endif

inline void minimizer_pars_t::fix_to(int ipar,double val)
{
  set(ipar,val);
  fix(ipar);
}

#undef INIT_TO
#undef EXTERN_MINIMIZER

#endif