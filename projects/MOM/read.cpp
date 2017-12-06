#ifdef HAVE_CONFIG_H
 #include <config.hpp>
#endif

#include <tranalisi.hpp>

#define EXTERN_READ
 #include <read.hpp>

#include <corrections.hpp>
#include <evolutions.hpp>
#include <ingredients.hpp>
#include <prop.hpp>

void read_input(const string &input_path)
{
  using namespace glb;
  
  raw_file_t input(input_path,"r");
  
  const size_t Ls=input.read<size_t>("L"); //!< lattice spatial size
  L[0]=input.read<size_t>("T");
  
  const string act_str=input.read<string>("Action"); //!< action name
  auto act_key=gaz::decr.find(act_str); //!< key in the map of act
  if(act_key==gaz::decr.end()) CRASH("Unable to decript %s",act_str.c_str());
  act=act_key->second;
  
  beta=input.read<double>("Beta");
  plaq=input.read<double>("Plaq");
  nm=input.read<double>("Nm");
  am.resize(nm);
  for(size_t im=0;im<nm;im++) am[im]=input.read<double>();
  am_max=*max_element(am.begin(),am.end())*1.1;
  am_min=*min_element(am.begin(),am.end())/1.1;
  nr=input.read<double>("Nr");
  
  im_sea=input.read<double>("ImSea");
  
  const string mom_list_path=input.read<string>("MomList"); //!< list of momenta
  set_njacks(input.read<size_t>("NJacks")); //!< number of jacks
  
  input.expect("ConfRange");
  conf_range.start=input.read<size_t>();
  conf_range.each=input.read<size_t>();
  conf_range.end=input.read<size_t>();
  
  nhits=input.read<size_t>("NHits");
  nhits_to_use=input.read<size_t>("NHitsToUse");
  
  tmin=input.read<size_t>("Tmin");
  tmax=input.read<size_t>("Tmax");
  
  use_QED=input.read<bool>("UseQED");
  print_each_mom=input.read<int>("PrintEachMom");
  
  ainv=input.read<double>("aInv");
  Nf=ev::Nf_t_of_Nf(input.read<int>("Nf"));
  
  ord=input.read<size_t>("Ord");
  
  //////////////////////////////////////////////////
  
  //! sufffix if a number of hits is different from 1
  if(nhits>1) suff_hit="_hit_%zu";
  
  g2=6.0/beta;
  g2tilde=g2/plaq;
  cout<<"g2tilde: "<<g2tilde<<endl;
  
  //set spatial sizes
  V=L[0]*pow(Ls,NDIM-1);
  for(size_t mu=1;mu<NDIM;mu++) L[mu]=Ls;
  
  //initialize momenta
  set_glb_list_of_moms(mom_list_path);
}

void prepare_list_of_confs()
{
  using namespace glb;
  
  //create the list of all confs available
  string test_path="out/%04zu/fft_S_M0_R0_0";
  if(nhits>1) test_path+="_hit_0";
  conf_list=get_existing_paths_in_range(test_path,conf_range);
  if(conf_list.size()==0) CRASH("list of configurations is empty! check %s ",test_path.c_str());
  clust_size=trim_to_njacks_multiple(conf_list,true);
  
  conf_ind.set_ranges({{"ijack",njacks},{"i_in_clust",clust_size}});
  im_r_ind.set_ranges({{"m",nm},{"r",nr}});
  i_in_clust_ihit_ind.set_ranges({{"i_in_clust",clust_size},{"ihit",nhits_to_use}});
  im_r_im_r_igam_ind=im_r_ind*im_r_ind*index_t({{"igamma",nGamma}});
  r_r_iZbil_ind.set_ranges({{"r",nr},{"r",nr},{"iZbil",nZbil}});
  im_r_ijack_ind=im_r_ind*index_t({{"ijack",njacks}});
  im_r_ijackp1_ind=im_r_ind*index_t({{"ijack",njacks+1}});
}

vector<task_list_t> prepare_read_prop_taks(vector<m_r_mom_conf_props_t> &props,const vector<size_t> &conf_list)
{
  using namespace glb;
  
  const index_t im_r_ijack_ind=im_r_ind*index_t({{"ijack",njacks}}); //!< index of im,r,ijack combo
  props.resize(im_r_ijack_ind.max());
  
  //! tasks
  vector<task_list_t> read_tasks(i_in_clust_ihit_ind.max());
  
  for(size_t ijack=0;ijack<njacks;ijack++)
    {
      for(size_t im=0;im<nm;im++)
	for(size_t r=0;r<nr;r++)
	  {
	    size_t im_r_ijack=im_r_ijack_ind({im,r,ijack});
	    m_r_mom_conf_props_t &l=props[im_r_ijack];
	    
	    //add EM if asked
	    using tup_in_t=tuple<prop_t*,string,dcompl_t>;
	    vector<tup_in_t> list={{&l.LO,"0",1}};
	    if(use_QED)
	      {
		list.push_back(tup_in_t(&l.P,"P",dcompl_t(0,tau3[r])));
		list.push_back(tup_in_t(&l.S,"S",dcompl_t(-1,0)));
		list.push_back(tup_in_t(&l.T,"T",dcompl_t(1,0)));
		list.push_back(tup_in_t(&l.F,"F",dcompl_t(1,0)));
		list.push_back(tup_in_t(&l.FF,"FF",dcompl_t(1,0)));
	      }
	    
	    for(auto &psc : list)
	      for(size_t i_i_in_clust_ihit=0;i_i_in_clust_ihit<i_in_clust_ihit_ind.max();i_i_in_clust_ihit++)
		{
		  const vector<size_t> i_in_clust_ihit=i_in_clust_ihit_ind(i_i_in_clust_ihit);
		  const size_t i_in_clust=i_in_clust_ihit[0],ihit=i_in_clust_ihit[1];
		  const size_t iconf=conf_ind({ijack,i_in_clust});
		  const string path=combine("out/%04zu/fft_",conf_list[iconf])+get_prop_tag(im,r,get<1>(psc))+combine(suff_hit.c_str(),ihit);
		  read_tasks[i_i_in_clust_ihit].push_back(incapsulate_task(read_prop,get<0>(psc),raw_file_t(path,"r"),get<2>(psc)));
		}
	  }
    }
  
  return read_tasks;
}
