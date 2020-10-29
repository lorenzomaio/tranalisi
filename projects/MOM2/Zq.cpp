#ifdef HAVE_CONFIG_H
 #include <config.hpp>
#endif

#ifdef USE_OMP
 #include <omp.h>
#endif

#include <MOM2/Zq.hpp>

#include <MOM2/analysis.hpp>
#include <MOM2/perens.hpp>

vector<perens_t::task_t> perens_t::get_Zq_tasks(const vector<const perens_t*>& ens)
{
  vector<const djvec_t*> in_Zq,in_Zq_QED_rel;
  vector<const djvec_t*> in_Zq_RI,in_Zq_RI_QED_rel;
  for(auto &e : ens)
    {
      in_Zq.push_back(&e->Zq);
      if(pars::use_QED)
	in_Zq_QED_rel.push_back(&e->Zq_QED_rel);
      if(pars::compute_RI)
	{
	  in_Zq_RI.push_back(&e->Zq_RI);
	  if(pars::use_QED)
	    in_Zq_RI_QED_rel.push_back(&e->Zq_RI_QED_rel);
	}
    }
  
  vector<task_t> Zq_tasks={{&Zq,in_Zq,im_r_ilinmom_ind,"Zq",QCD_task}};
  if(pars::use_QED)
    Zq_tasks.push_back({&Zq_QED_rel,in_Zq_QED_rel,im_r_ilinmom_ind,"Zq"+QED_tag_suffix(),QED_task});
  
  if(pars::compute_RI)
    {
      Zq_tasks.push_back({&Zq_RI,in_Zq_RI,im_r_ilinmom_ind,"Zq_RI",QCD_task});
      if(pars::use_QED)
	Zq_tasks.push_back({&Zq_RI_QED_rel,in_Zq_RI_QED_rel,im_r_ilinmom_ind,"Zq_RI"+QED_tag_suffix(),QED_task});
    }
  
  return Zq_tasks;
}

void perens_t::evolve_QED_Zq_mixed_to_1_ov_a(perens_t& out) const
{
  cout<<"Evolving Zq"<<endl;
  
  for(size_t im_r_ilinmom=0;im_r_ilinmom<im_r_ilinmom_ind.max();im_r_ilinmom++)
    {
      const vector<size_t> im_r_ilinmom_comps=im_r_ilinmom_ind(im_r_ilinmom);
      const size_t ilinmom=im_r_ilinmom_comps[2];
      const double a2p2=all_moms[linmoms[ilinmom][0]].p(L).norm2();
      const double p2=a2p2*sqr(ainv);
      
      // CRASH("0.00428368 %lg  3.22561 %lg",evolve_QED_mixed_alpha(0.00428368,gamma),
      // 	    evolve_QED_mixed_alpha(3.22561,gamma));
      // cout<<"evolve_QED_mixed_alpha(a2p2,gamma): "<<evolve_QED_mixed_alpha(a2p2,gamma)<<" "<<a2p2<<" "<<gamma<<endl;
      
      const double gamma_s0=0.0; /* in the Landau gauge the one-loop strong an. dim. is zero */
      const double gamma_e0=2.0;
      const double gamma_se1=-8.0;
      
      const double evolution_to_RI=evol::evolution_Zq_to_RIp(evol::NF4,/*ord*/3,ainv,p2);
      cout<<"AAA "<<ainv<<" "<<a2p2<<" "<<evolution_to_RI<<endl;
      
      out.Zq_QED_rel[im_r_ilinmom]=
	Zq_QED_rel[im_r_ilinmom]+
	evolve_QED_mixed_alpha(a2p2,gamma_s0,gamma_se1,gamma_e0,evolution_to_RI);
    }
}

void perens_t::compute_Zq(const bool also_QCD,const bool also_QED)
{
  cout<<"Computing Zq"<<endl;
  
#pragma omp parallel for
  for(size_t im_r_ilinmom=0;im_r_ilinmom<im_r_ilinmom_ind.max();im_r_ilinmom++)
    {
      const vector<size_t> comps=im_r_ilinmom_ind(im_r_ilinmom);
      const size_t im=comps[0];
      const size_t r=comps[1];
      const size_t ilinmom=comps[2];
      
      using namespace sigma;
      auto sigma1=sigma_ins_getter(im,r,ilinmom,SIGMA1);
      auto sigma_RI=sigma_ins_getter(im,r,ilinmom,SIGMA_RI);
      auto sigma_RI_VT=sigma_ins_getter(im,r,ilinmom,SIGMA_RI_VT);
      auto sigma_RI_VX=sigma_ins_getter(im,r,ilinmom,SIGMA_RI_VX);
      auto sigma_RI_VY=sigma_ins_getter(im,r,ilinmom,SIGMA_RI_VY);
      auto sigma_RI_VZ=sigma_ins_getter(im,r,ilinmom,SIGMA_RI_VZ);
      
      if(also_QCD) Zq[im_r_ilinmom]=sigma1(LO);
      
      if(pars::use_QED and also_QED)
	{
	  needs_to_read_assembled_QED_greenfunctions();
	  Zq_QED_rel[im_r_ilinmom]=sigma1(QED)/sigma1(LO);
	}
      
      switch(pars::compute_RI)
	{
	case 0:
	  break;
	case 1:
	  Zq_RI[im_r_ilinmom]=-sigma_RI(RI)/4.0;
	  if(also_QED) CRASH("Not implemented yet");
	  break;
	case 2:
	  Zq_RI[im_r_ilinmom]=-(sigma_RI_VT(RI_VT)+
				sigma_RI_VX(RI_VX)+
				sigma_RI_VY(RI_VY)+
				sigma_RI_VZ(RI_VZ))/4.0;
	  if(also_QED) CRASH("Not implemented yet");
	  break;
	}
    }
}

void perens_t::interpolate_Zq_to_p2ref(perens_t &out) const
{
  cout<<"Interpolating to reference p2 Zq"<<endl;
  
  //get ranges
  double a2p2=pars::p2ref/sqr(ainv);
  pair<double,double> a2p2minmax=get_a2p2tilde_range_bracketting(linmoms,a2p2);
  const double p2min=a2p2minmax.first*sqr(ainv);
  const double p2max=a2p2minmax.second*sqr(ainv);
  cout<<"p2 range:   "<<p2min<<" - "<<p2max<<endl;
  
  //fit points
  vector<double> x(linmoms.size());
  djvec_t y(linmoms.size());
  
  for(auto &t : out.get_Zq_tasks({this}))
    {
      const djvec_t &in=*t.in.front();
      djvec_t &out=*t.out;
      const string &tag=t.tag;
      
      if(fabs(in[0].ave()-1)<1e-7 and in[0].err()<1e-7)
	{
	  cout<<"Skipping interpolation for "<<tag<<endl;
	  out=in[0];
	}
      else
	for(size_t im_r=0;im_r<im_r_ind.max();im_r++)
	  {
	    const vector<size_t> comps=im_r_ind(im_r);
	    
	    //take physical units for plot
	    for(size_t imom=0;imom<linmoms.size();imom++)
	      {
		vector<size_t> comps_with_mom=comps;
		comps_with_mom.push_back(imom);
		
		x[imom]=all_moms[linmoms[imom][0]].p(L).tilde().norm2()*sqr(ainv);
		y[imom]=in[im_r_ilinmom_ind(comps_with_mom)];
	      }
	    
	    //fit and interpolate
	    const djvec_t coeffs=poly_fit(x,y,2,p2min,p2max);
	    out[im_r]=poly_eval(coeffs,pars::p2ref);
	    
	    //produce plot
	    const string path=dir_path+"/plots/interpolate_to_p2ref_"+tag+
	      "_m"+to_string(comps[0])+"_r"+to_string(comps[1])+".xmg";
	    grace_file_t plot(path);
	    write_fit_plot(plot,p2min,p2max,bind(poly_eval<djvec_t,double>,coeffs,_1),x,y);
	    plot.write_ave_err(pars::p2ref,out[im_r].ave_err());
	  }
    }
}

void perens_t::plot_Zq(const string &suffix)
{
  cout<<"Plotting all Zq of "<<dir_path<<" for suffix: \""<<suffix<<"\""<<endl;
  
  for(auto &t : get_Zq_tasks())
    {
      const djvec_t &Z=*t.out;
      const string &tag=t.tag;
      
      grace_file_t out(dir_path+"/plots/"+tag+(suffix!=""?("_"+suffix):string(""))+".xmg");
      
      for(size_t im=0;im<nm;im++)
  	for(size_t r=0;r<nr;r++)
	  {
	    out.new_data_set();
	    for(size_t imom=0;imom<linmoms.size();imom++)
	      {
		const double p2tilde=all_moms[linmoms[imom][0]].p(L).tilde().norm2();
		out.write_ave_err(p2tilde,Z[im_r_ilinmom_ind({im,r,imom})].ave_err());
	      }
	  }
    }
}
