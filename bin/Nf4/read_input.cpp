#include "read_input.hpp"
#include "aliases.hpp"
#include "global.hpp"
#include "operations.hpp"

enum ERR_t{NO_FAIL,FAILED_READ,FAILED_CONVERSION,FAILED_OPEN,MISPLACED_TK,UNINITIALIZED_PAR};

enum TK_t{FEOF_TK,VALUE_TK,MOM_LIST_TK,NCONFS_TK,NJACKS_TK,L_TK,T_TK,CONF_INIT_TK,CONF_STEP_TK,ACT_TK,PLAQ_TK,PATH_TK,BETA_TK,MU_SEA_TK,KAPPA_TK,NMASSES_VAL_TK,MASSES_VAL_TK,NR_TK,NTYPES_TK,NHITS_TK,USE_SIGMA_TK,SCHEME_TK};

#define DEFAULT_STR_VAL ""
#define DEFAULT_INT_VAL -1
#define DEFAULT_DOUBLE_VAL 1.2345

// define global variables
int nconfs, njacks, clust_size, L, T, conf_init, conf_step, nm, neq, neq2, nbil, nr, nmr, nt, nhits, moms, neqmoms, Nf, ntypes, combo, UseSigma1;
double beta, kappa, mu_sea, plaquette, g2, g2_tilde;
string mom_path, action, path_ensemble_str, scheme;
vector<double> mass_val(10);
coords_t size;

const char nconfs_tag[]="NConfs";
const char mom_list_tag[]="MomList";
const char njacks_tag[]="NJacks";
const char L_tag[]="L";
const char T_tag[]="T";
const char conf_init_tag[]="ConfInit";
const char conf_step_tag[]="ConfStep";
const char act_tag[]="Action";
const char path_ensemble_tag[]="PathEnsemble";
const char beta_tag[]="Beta";
const char mu_sea_tag[]="MuSea";
const char kappa_tag[]="Kappa";
const char nm_tag[]="NMassesVal";
const char mass_val_tag[]="MassesVal";
const char nr_tag[]="Nr";
const char plaquette_tag[]="Plaquette";
const char ntypes_tag[]="NTypes";
const char nhits_tag[]="NHits";
const char UseSigma1_tag[]="UseSigma1";
const char scheme_tag[]="Scheme";

char tok[128];


TK_t get_TK(FILE *fin)
{
    //read a token
    int rc=fscanf(fin,"%s",tok);
    if(rc!=1)
    {
        if(feof(fin)) return FEOF_TK;
        else
        {
            fprintf(stderr,"Getting %d while reading token\n",rc);
            exit(FAILED_READ);
        }
    }
    
    //parse the token
    if(strcasecmp(tok,mom_list_tag)==0) return MOM_LIST_TK;
    if(strcasecmp(tok,nconfs_tag)==0) return NCONFS_TK;
    if(strcasecmp(tok,njacks_tag)==0) return NJACKS_TK;
    if(strcasecmp(tok,L_tag)==0) return L_TK;
    if(strcasecmp(tok,T_tag)==0) return T_TK;
    if(strcasecmp(tok,conf_init_tag)==0) return CONF_INIT_TK;
    if(strcasecmp(tok,conf_step_tag)==0) return CONF_STEP_TK;
    if(strcasecmp(tok,act_tag)==0) return ACT_TK;
    if(strcasecmp(tok,path_ensemble_tag)==0) return PATH_TK;
    if(strcasecmp(tok,beta_tag)==0) return BETA_TK;
    if(strcasecmp(tok,mu_sea_tag)==0) return MU_SEA_TK;
    if(strcasecmp(tok,kappa_tag)==0) return KAPPA_TK;
    if(strcasecmp(tok,nm_tag)==0) return NMASSES_VAL_TK;
    if(strcasecmp(tok,mass_val_tag)==0) return MASSES_VAL_TK;
    if(strcasecmp(tok,nr_tag)==0) return NR_TK;
    if(strcasecmp(tok,plaquette_tag)==0) return PLAQ_TK;
    if(strcasecmp(tok,ntypes_tag)==0) return NTYPES_TK;
    if(strcasecmp(tok,nhits_tag)==0) return NHITS_TK;
    if(strcasecmp(tok,UseSigma1_tag)==0) return USE_SIGMA_TK;
    if(strcasecmp(tok,scheme_tag)==0) return SCHEME_TK;

    
    return VALUE_TK;
}

//parse the value string
template <class T>
void get_value(FILE *fin,T &ret,const char *t)
{
    TK_t tk=get_TK(fin);
    if(tk!=VALUE_TK)
    {
        fprintf(stderr,"Getting token %s in the wrong place\n",tok);
        exit(MISPLACED_TK);
    }
    
    int rc=sscanf(tok,t,&ret);
    
    if(rc!=1)
    {
        fprintf(stderr,"Converting %s to %s failed\n",tok,t);
        exit(FAILED_CONVERSION);
    }
}

//check
void check_str_par(const char *str,const char *name)
{
    if(strcasecmp(str,DEFAULT_STR_VAL)==0)
    {
        fprintf(stderr,"%s not initialized\n",name);
        exit(UNINITIALIZED_PAR);
    }
}

void check_int_par(const int val,const char *name)
{
    if(val==DEFAULT_INT_VAL)
    {
        fprintf(stderr,"%s not initialized\n",name);
        exit(UNINITIALIZED_PAR);
    }
}

void check_double_par(const double val,const char *name)
{
    if(val==DEFAULT_DOUBLE_VAL)
    {
        fprintf(stderr,"%s not initialized\n",name);
        exit(UNINITIALIZED_PAR);
    }
}

//factorial
int fact(int n)
{
    if(n > 1)
        return n * fact(n - 1);
    else
        return 1;
}

// reads the input file
void read_input(const char path[])
{
    FILE *fin=fopen(path,"r");
    if(not fin)
    {
        fprintf(stderr,"Failed to open \"%s\"\n",path);
        exit(FAILED_OPEN);
    }
    
    char mom_list_path[128]=DEFAULT_STR_VAL;
    char act[128]=DEFAULT_STR_VAL;
    char sch[128]=DEFAULT_STR_VAL;
    char path_ensemble[128]=DEFAULT_STR_VAL;
    
    nconfs=DEFAULT_INT_VAL;
    njacks=DEFAULT_INT_VAL;
    L=DEFAULT_INT_VAL;
    T=DEFAULT_INT_VAL;
    conf_init=DEFAULT_INT_VAL;
    conf_step=DEFAULT_INT_VAL;
    nm=DEFAULT_INT_VAL;
    nr=DEFAULT_INT_VAL;
    ntypes=DEFAULT_INT_VAL;
    nhits=DEFAULT_INT_VAL;
    
    UseSigma1=DEFAULT_INT_VAL;
    
    beta=DEFAULT_DOUBLE_VAL;
    kappa=DEFAULT_DOUBLE_VAL;
    mu_sea=DEFAULT_DOUBLE_VAL;
    plaquette=DEFAULT_DOUBLE_VAL;
    
    for(auto &m : mass_val) m=DEFAULT_DOUBLE_VAL;
    
    while(not feof(fin))
    {
        TK_t tk=get_TK(fin);
        switch(tk)
        {
            case VALUE_TK:
                fprintf(stderr,"Invalid token %s found\n",tok);
                exit(1);
                break;
            case MOM_LIST_TK:
                get_value(fin,mom_list_path,"%s");
                break;
            case NCONFS_TK:
                get_value(fin,nconfs,"%d");
                break;
            case NJACKS_TK:
                get_value(fin,njacks,"%d");
                break;
            case L_TK:
                get_value(fin,L,"%d");
                break;
            case T_TK:
                get_value(fin,T,"%d");
                break;
            case CONF_INIT_TK:
                get_value(fin,conf_init,"%d");
                break;
            case CONF_STEP_TK:
                get_value(fin,conf_step,"%d");
                break;
            case ACT_TK:
                get_value(fin,act,"%s");
                break;
            case PLAQ_TK:
                get_value(fin,plaquette,"%lf");
                break;
            case PATH_TK:
                get_value(fin,path_ensemble,"%s");
                break;
            case BETA_TK:
                get_value(fin,beta,"%lf");
                break;
            case KAPPA_TK:
                get_value(fin,kappa,"%lf");
                break;
            case MU_SEA_TK:
                get_value(fin,mu_sea,"%lf");
                break;
            case NMASSES_VAL_TK:
                get_value(fin,nm,"%d");
                break;
            case MASSES_VAL_TK:
                mass_val.resize(nm);
                for (int i=0; i<nm; i++)
                    get_value(fin,mass_val[i],"%lf");
                break;
            case NR_TK:
                get_value(fin,nr,"%d");
                break;
            case NTYPES_TK:
                get_value(fin,ntypes,"%d");
                break;
            case NHITS_TK:
                get_value(fin,nhits,"%d");
                break;
            case USE_SIGMA_TK:
                get_value(fin,UseSigma1,"%d");
                break;
            case SCHEME_TK:
                get_value(fin,sch,"%s");
                break;
                
            case FEOF_TK:
                break;
        }
    }
    
    //check initialization
    check_str_par(mom_list_path,mom_list_tag);
    check_int_par(nconfs,nconfs_tag);
    check_int_par(njacks,njacks_tag);
    check_int_par(L,L_tag);
    check_int_par(T,T_tag);
    check_int_par(conf_init,conf_init_tag);
    check_int_par(conf_step,conf_step_tag);
    check_str_par(act,act_tag);
    check_double_par(plaquette,plaquette_tag);
    check_double_par(beta,beta_tag);
    check_double_par(kappa,kappa_tag);
    check_int_par(nm,nm_tag);
    for (int i=0; i<nm; i++)
        check_double_par(mass_val[i],mass_val_tag);
    check_int_par(nr,nr_tag);
    check_int_par(ntypes,ntypes_tag);
    check_int_par(nhits,nhits_tag);
    check_int_par(nhits,UseSigma1_tag);
    check_str_par(sch,scheme_tag);
    
    fclose(fin);
    
    //print input parameters
    printf("%s = %s\n",scheme_tag,sch);
    printf("%s = \"%s\"\n",mom_list_tag,mom_list_path);
    printf("%s = %d  [from %d to %d]\n",nconfs_tag,nconfs,conf_init,conf_init+nconfs*conf_step-1);
    printf("%s = %d\n",njacks_tag,njacks);
    printf("%s = %d\n",L_tag,L);
    printf("%s = %d\n",T_tag,T);
    printf("%s = %s\n",act_tag,act);
    printf("%s = %lf\n",plaquette_tag,plaquette);
    printf("%s = \"%s\"\n",path_ensemble_tag,path_ensemble);
    printf("%s = %.2lf\n",beta_tag,beta);
    printf("%s = %.6lf\n",kappa_tag,kappa);
    printf("%s = %.4lf\n",mu_sea_tag,mu_sea);
    printf("%s = %d\n",nm_tag,nm);
    printf("%s = ",mass_val_tag);
    for (int i=0; i<nm; i++)
        printf("%.4lf  ",mass_val[i]);
    printf("\n");
    printf("%s = %d\n",nr_tag,nr);
    printf("%s = %d\n",nhits_tag,nhits);
    
    clust_size=nconfs/njacks;
    nmr=nm*nr;
    
    combo=nm*nr*ntypes*nhits*nconfs;
    neq=fact(nm+nr-1)/fact(nr)/fact(nm-1);
    neq2=nm;
    
    size={T,L,L,L};
    
    nbil=5;
    
    mom_path = string(mom_list_path);
    action = string(act);
    path_ensemble_str=string(path_ensemble);
    scheme = string(sch);
    
    //factors enabling average over r
    int c1, c2;
    if(nr==2)
    {
        c1 = 1;
        c2 = 1;
    }
    
    //g2_tilde
    double g2=6.0/beta;
    double g2_tilde=g2/plaquette;

}
