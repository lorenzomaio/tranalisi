#include <tranalisi.hpp>

int T,L;
int TH;

vector<dcompl_t> read_vector(const string &path,int n,int nskip=0)
{
  raw_file_t data(path,"r");
  
  //cout<<"Reading from "<<path<<" "<<n<<" lines after skipping "<<nskip<<" words"<<endl;
  for(int iskip=0;iskip<nskip;iskip++)
    data.skip_line();
  
  vector<dcompl_t> d;
  
  for(int i=0;i<n;i++)
    {
      double re=data.read<double>();
      double im=data.read<double>();
      
      d.emplace_back(re,im);
    }
  
  return d;
}

template <typename T>
T average(const vector<T>& v)
{
  return accumulate(v.begin(),v.end(),dcompl_t{0,0})/(double)v.size();
}

int main(int narg,char **arg)
{
  string name="input.txt";
  if(narg>=2) name=arg[1];
  raw_file_t input(name,"r");
  
  cout.precision(16);
  
  T=input.read<int>("T");
  TH=T/2;
  L=input.read<int>("L");
  range_t conf_range;
  input.expect("ConfRange");
  conf_range.start=input.read<size_t>();
  conf_range.each=input.read<size_t>();
  conf_range.end=input.read<size_t>();
  
  vector<size_t> confs_p=get_existing_paths_in_range("out/%04d/mes_contr_Pion",conf_range,SILENT);
  vector<size_t> confs_1=get_existing_paths_in_range("out/%04d/EU1_stoch",conf_range,SILENT);
  vector<size_t> confs;
  for(auto &_p : confs_p)
    for(auto & _1 : confs_1)
      if(_p==_1)
	{
	  //cout<<_p<<endl;
	  confs.push_back(_p);
	}
  
  set_njacks(input.read<int>("NJacks"));
  
  const int nposs_confs=confs.size();
  cout<<"Npossible confs: "<<nposs_confs<<endl;
  const int clust_size=nposs_confs/njacks;
  cout<<"Cluster size: "<<clust_size<<endl;
  const int nconfs=clust_size*njacks;
  cout<<"Nconfs: "<<nconfs<<endl;
  
  const int nhits=input.read<int>("NHits");
  
  const int ndiag=5;
  enum {EU1,EU2,EU4,EU5,EU6};
  const int diag[ndiag]={1,2,4,5,6};
  
  /// Allocate diagrams and initializes it
  array<array<djack_t,2>,ndiag> EU;
  djvec_t pion(T);
  array<djvec_t,ndiag> pion_EU;
  pion_EU.fill(djvec_t{(size_t)T});
  for(auto &E : EU)
    for(auto &Eri : E)
      Eri=0.0;
  
  for(int iconf=0;iconf<nconfs;iconf++)
    {
      /// Finds the jack index
      int ijack=iconf/clust_size;
      
      /// Computes the configuration id
      const int conf=confs[iconf];
      
      /// Reads all diagrams
      array<vector<dcompl_t>,ndiag> EU_stoch;
      vector<dcompl_t> pion_stoch=read_vector(combine("out/%04d/mes_contr_Pion",conf),T,5);
      array<dcompl_t,ndiag> EU_tot;
      
      for(int t=0;t<T;t++)
	pion[t][ijack]+=pion_stoch[t].real();
      
      for(int idiag=0;idiag<ndiag;idiag++)
	{
	  EU_stoch[idiag]=read_vector(combine("out/%04d/EU%d_stoch",conf,diag[idiag]),nhits);
	  
	  // Computes the average over stochastich estimates
	  EU_tot[idiag]=average(EU_stoch[idiag]);
	  
	  // Copy real and imaginary part of the disconnected diagram
	  for(int ri=0;ri<2;ri++)
	    EU[idiag][ri][ijack]+=((double*)&(EU_tot[idiag]))[ri];
	  
	  // Computes the connected and disconnected X connected
	  for(int t=0;t<T;t++)
	    pion_EU[idiag][t][ijack]+=pion_stoch[t].real()*EU_tot[idiag].real();
	}
    }
  
  // Prepares the jackknives of disconnected diagrams and print it
  for(int idiag=0;idiag<ndiag;idiag++)
    for(int ri=0;ri<2;ri++)
      {
	EU[idiag][ri].clusterize(clust_size);
	
	cout<<"EU"<<diag[idiag]<<": "<<EU[idiag][ri].ave_err()<<endl;
      }
  
  pion.clusterize(clust_size).symmetrize();
  pion.ave_err().write("plots/pion.xmg");
  
  djvec_t SL(ndiag);
  
  // Creates the jackknife of the connected and disconnected diagrams and plots them
  for(int idiag=0;idiag<ndiag;idiag++)
    {
      pion_EU[idiag].clusterize(clust_size).symmetrize();
      pion_EU[idiag].ave_err().write(combine("plots/pion_EU%d.xmg",diag[idiag]));
      
      /// Subtract the fully disconnected part and plots it
      djvec_t pion_EU_sub=pion_EU[idiag]-pion*EU[idiag][0];
      pion_EU_sub.ave_err().write(combine("plots/EU%d_sub.xmg",diag[idiag]));
      
      /// Computes the ratio with the purely connected and plots it
      djvec_t pion_EU_rat=pion_EU_sub/pion;
      
      djack_t Z2,M,DZ2_fr_Z2;
      two_pts_with_ins_ratio_fit(Z2,M,DZ2_fr_Z2,SL[idiag],pion,pion_EU_sub,TH,12,TH,"plots/pion.xmg",combine("plots/EU%d_rat.xmg",diag[idiag]));
    }
  
  cout<<"Derivative of the diagram:"<<endl;
  for(int idiag=0;idiag<ndiag;idiag++)
    cout<<"EU"<<diag[idiag]<<": "<<SL[idiag].ave_err()<<endl;
  
  // pion_EU1_rat.ave_err().write("plots/EU1_rat.xmg");
  // pion_EU2_rat.ave_err().write("plots/EU2_rat.xmg");
  // pion_EU4_rat.ave_err().write("plots/EU4_rat.xmg");
  // pion_EU5_rat.ave_err().write("plots/EU5_rat.xmg");
  // pion_EU6_rat.ave_err().write("plots/EU6_rat.xmg");
  
  return 0;
}
