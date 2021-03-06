#include "aliases.hpp"
#include "global.hpp"
#include "read.hpp"
#include "jack.hpp"
#include "Zq.hpp"
#include "Zbil.hpp"
#include "Dirac.hpp" //useless
#include "vertices.hpp"
#include <fstream>
#include <iostream>
#include <string>
#include <omp.h>
#include "fit.hpp"
#include <stdio.h>
#include <stdlib.h>
#include "subtraction.hpp"
#include "evolution.hpp"
#include "print.hpp"
#include "ave_err.hpp"
#include <chrono>

#define EXTERN_OPER

#include "operations.hpp"

#include "vertices.hpp"

using namespace std::chrono;

SCHEME_t get_scheme()
{
    if(scheme=="RI_MOM") return RI_MOM;
    if(scheme=="SMOM") return SMOM;
    
    return ERR;
}

void oper_t::set_moms()
{
    switch(get_scheme())
    {
        case RI_MOM:
            set_ri_mom_moms();
            break;
        case SMOM:
            cout<<"SMOM!"<<endl;
            set_smom_moms();
            break;
        case ERR:
            cout<<"Invalid scheme."<<endl;
            exit(0);
            break;
    }
    _linmoms=linmoms.size();
    _bilmoms=bilmoms.size();
//    moms=_linmoms;
}

void oper_t::set_ri_mom_moms()
{
    linmoms.resize(moms);
    bilmoms.resize(moms);
    
    for(int imom=0;imom<moms;imom++)
        if(filt_moms[imom])
        {
            linmoms[imom]={imom};
            bilmoms[imom]={imom,imom,imom};
        }
}

void oper_t::set_smom_moms()
{
    // http://xxx.lanl.gov/pdf/0901.2599v2 (Sturm et al.)
    
    linmoms.clear();
    bilmoms.clear();
    
    double eps=1e-10;
    
    for(int i=0;i<moms;i++)
        if(filt_moms[i])
            for(int j=0;j<moms;j++)
                if(filt_moms[j])
                {
                    if(2.0*fabs(p2[i]-p2[j])<(p2[i]+p2[j])*eps)
                    {
                        coords_t momk;
                        
                        p_t k_array, k_tilde_array;
                        double k_sqr=0.0, k_tilde_sqr=0.0;
                        double k_4=0.0, k_tilde_4=0.0;
                        
                        for(size_t mu=0;mu<4;mu++)
                        {
                            momk[mu]=mom_list[i][mu]-mom_list[j][mu];
                            
                            k_array[mu]=2*M_PI*momk[mu]/size[mu];
                            k_sqr+=k_array[mu]*k_array[mu];
                            k_4+=k_array[mu]*k_array[mu]*k_array[mu]*k_array[mu];

                            k_tilde_array[mu]=sin(k_array[mu]);
                            k_tilde_sqr+=k_tilde_array[mu]*k_tilde_array[mu];
                            k_tilde_4+=k_tilde_array[mu]*k_tilde_array[mu]*k_tilde_array[mu]*k_tilde_array[mu];
                        }
                        
                        if(2.0*fabs(p2[i]-k_sqr)<(p2[i]+k_sqr)*eps)
                        {
                            //search in mom_list
                            auto posk = find(mom_list.begin(),mom_list.end(),momk);
                            
                            //if not found, push into mom_list
                            if(posk==mom_list.end())
                            {
                                posk=mom_list.end();
                                
                                mom_list.push_back(momk);
                                p.push_back(k_array);
                                p_tilde.push_back(k_tilde_array);
                                p2.push_back(k_sqr);
                                p2_tilde.push_back(k_tilde_sqr);
                                p4.push_back(k_4);
                                p4_tilde.push_back(k_tilde_4);
                            }
                            
                            const int k=distance(mom_list.begin(),posk);
                            
                            vector<int> pos;
                            
                            //search in the linmoms: if found take the distance, otherwise add
                            for(const int ic : {i,j})
                            {
                                cout<<"searching for "<<ic<<endl;
                                auto pos_ic=find(linmoms.begin(),linmoms.end(),array<int,1>{ic});
                                size_t d;
                                if(pos_ic==linmoms.end())
                                {
                                    //the position will be the end
                                    d=linmoms.size();
                                    //include it
                                    linmoms.push_back({ic});
                                    
                                    cout<<" not found"<<endl;
                                }
                                else
                                {
                                    d=distance(linmoms.begin(),pos_ic);
                                    cout<<" found"<<endl;
                                }
                                
                                //add to the list
                                cout<<"Position: "<<d<<endl;
                                pos.push_back(d);
                            }
                            
                            //store
                            bilmoms.push_back({k,pos[0],pos[1]});
                            
                        } else cout<<"p2-k2 != 0"<<endl;
                    } else cout<<"p1^2-p2^2 != 0"<<endl;
                }
}

////////

void oper_t::create_basic()
{
    step = "basic";
    
    _nm=nm;
    _nr=nr;
    _nmr=_nm*_nr;
    
    set_moms();
    
    allocate();
    
    ifstream jZq_data("print/jZq");
    ifstream jZq_em_data("print/jZq_em");
    ifstream jG_0_data("print/jG_0");
    ifstream jG_em_data("print/jG_em");
    if(jZq_data.good() and jZq_em_data.good() and jG_0_data.good() and jG_em_data.good())
    {
        cout<<"Reading data from files"<<endl<<endl;
        
//        vector<int> Np(_linmoms);
        
        READ_BIN(jZq);
        READ_BIN(jZq_em);
//        READ_BIN(Np);
        READ_BIN(jG_0);
        READ_BIN(jG_em);
        
    }
    else
    {
        
        switch(get_scheme())
        {
            case RI_MOM:
                ri_mom();
                break;
            case SMOM:
                cout<<"SMOM!"<<endl;
                smom();
                break;
            case ERR:
                cout<<"Invalid scheme."<<endl;
                exit(0);
                break;
        }
    }
    
    compute_Zbil();
    
}

void oper_t::ri_mom()
{
    compute_prop();
    compute_bil();
}

void oper_t::smom()
{
    ri_mom();
}


//////////

void oper_t::allocate()
{
    jZq.resize(_linmoms);
    jZq_em.resize(_linmoms);
    
    jG_0.resize(_bilmoms);
    jG_em.resize(_bilmoms);
    
    jZ.resize(_bilmoms);
    jZ_em.resize(_bilmoms);
    
    for(auto &ijack : jZq)
    {
        ijack.resize(njacks);
        for(auto &mr : ijack)
            mr.resize(_nmr);
    }
    
    for(auto &ijack : jZq_em)
    {
        ijack.resize(njacks);
        for(auto &mr : ijack)
            mr.resize(_nmr);
    }
    
    
    for(auto &ibil : jG_0)
    {
        ibil.resize(nbil);
        for(auto &ijack : ibil)
        {
            ijack.resize(njacks);
            for(auto &mr1 : ijack)
            {
                mr1.resize(_nmr);
                for(auto &mr2 : mr1)
                    mr2.resize(_nmr);
            }
        }
    }
    
    for(auto &ibil : jG_em)
    {
        ibil.resize(nbil);
        for(auto &ijack : ibil)
        {
            ijack.resize(njacks);
            for(auto &mr1 : ijack)
            {
                mr1.resize(_nmr);
                for(auto &mr2 : mr1)
                    mr2.resize(_nmr);
            }
        }
    }
    
    for(auto &ibil : jZ)
    {
        ibil.resize(nbil);
        for(auto &ijack : ibil)
        {
            ijack.resize(njacks);
            for(auto &mr1 : ijack)
            {
                mr1.resize(_nmr);
                for(auto &mr2 : mr1)
                    mr2.resize(_nmr);
            }
        }
    }
    
    for(auto &ibil : jZ_em)
    {
        ibil.resize(nbil);
        for(auto &ijack : ibil)
        {
            ijack.resize(njacks);
            for(auto &mr1 : ijack)
            {
                mr1.resize(_nmr);
                for(auto &mr2 : mr1)
                    mr2.resize(_nmr);
            }
        }
    }
}

void oper_t::resize_output(oper_t out)
{
    (out.jZq).resize(out._linmoms);
    (out.jZq_em).resize(out._linmoms);
    
    (out.jG_0).resize(out._bilmoms);
    (out.jG_em).resize(out._bilmoms);
    
    (out.jZ).resize(out._bilmoms);
    (out.jZ_em).resize(out._bilmoms);
    
    for(auto &ijack : out.jZq)
        for(auto &mr : ijack)
            mr.resize(out._nmr);
    
    for(auto &ijack : out.jZq_em)
        for(auto &mr : ijack)
            mr.resize(out._nmr);
    
    for(auto &ibil : out.jG_0)
        for(auto &ijack : ibil)
            for(auto &mr1 : ijack)
            {
                mr1.resize(out._nmr);
                for(auto &mr2 : mr1)
                    mr2.resize(out._nmr);
            }
    
    for(auto &ibil : out.jG_em)
        for(auto &ijack : ibil)
            for(auto &mr1 : ijack)
            {
                mr1.resize(out._nmr);
                for(auto &mr2 : mr1)
                    mr2.resize(out._nmr);
            }
    
    for(auto &ibil : out.jZ)
        for(auto &ijack : ibil)
            for(auto &mr1 : ijack)
            {
                mr1.resize(out._nmr);
                for(auto &mr2 : mr1)
                    mr2.resize(out._nmr);
            }
    
    for(auto &ibil : out.jZ_em)
        for(auto &ijack : ibil)
            for(auto &mr1 : ijack)
            {
                mr1.resize(out._nmr);
                for(auto &mr2 : mr1)
                    mr2.resize(out._nmr);
            }
}

vvvprop_t build_prop(jprop_t &jS_0,jprop_t &jS_em,const vvvprop_t &S)
{
    vvvprop_t S_LO_and_EM(vvprop_t(vprop_t(prop_t::Zero(),nmr),njacks),2);
    
#pragma omp parallel for collapse(3)
    for(int m=0;m<nm;m++)
        for(int r=0;r<nr;r++)
            for(int ijack=0;ijack<njacks;ijack++)
            {
                int mr = r + nr*m;
                
                S_LO_and_EM[LO][ijack][mr] = S[ijack][0][mr];
                
                // Electromagnetic correction:  S_em = S_self + S_tad -+ deltam_cr*S_P
                if(r==0) S_LO_and_EM[EM][ijack][mr] = S[ijack][2][mr] + S[ijack][3][mr] + deltam_cr[ijack][m][m]*S[ijack][4][mr]; //r=0
                if(r==1) S_LO_and_EM[EM][ijack][mr] = S[ijack][2][mr] + S[ijack][3][mr] - deltam_cr[ijack][m][m]*S[ijack][4][mr]; //r=1
           
                jS_0[ijack][mr] += S_LO_and_EM[LO][ijack][mr];
                jS_em[ijack][mr] += S_LO_and_EM[EM][ijack][mr];
            }
    
    return S_LO_and_EM;
}

void oper_t::compute_prop()
{
    cout<<"Creating the propagators -- ";
    cout<<endl;

    // array of input files to be read in a given conf
    FILE* input[combo];
    vector<string> v_path = setup_read_prop(input);
    
    vvvd_t jZq_LO_and_EM(vvd_t(vd_t(0.0,_nmr),njacks),2);
    
    for(int ilinmom=0; ilinmom<_linmoms; ilinmom++)
    {
        cout<<"\r\t linmom = "<<ilinmom+1<<"/"<<_linmoms<<endl;
        
        // initialize propagators
        vvvprop_t S_LO_and_EM(vvprop_t(vprop_t(prop_t::Zero(),_nmr),njacks),2);
        
        // definition of jackknifed propagators
        jprop_t jS_0(valarray<prop_t>(prop_t::Zero(),_nmr),njacks);
        jprop_t jS_em(valarray<prop_t>(prop_t::Zero(),_nmr),njacks);
        
        for(int i_in_clust=0;i_in_clust<clust_size;i_in_clust++)
            for(int ihit=0;ihit<nhits;ihit++)
            {
                const vvvprop_t S=read_prop_mom(input,v_path,i_in_clust,ihit,ilinmom);
                
                S_LO_and_EM = build_prop(jS_0,jS_em,S);
            }
        
        vvprop_t S_0 = S_LO_and_EM[LO];
        vvprop_t S_em = S_LO_and_EM[EM];
        
        // jackknife average
        jS_0=jackknife(jS_0);
        jS_em=jackknife(jS_em);
        
        // invert propagator
        vvvprop_t jS_inv_LO_and_EM(vvprop_t(vprop_t(prop_t::Zero(),_nmr),njacks),2);
        
        jS_inv_LO_and_EM[LO] = invert_jprop(jS_0);
        jS_inv_LO_and_EM[EM] = jS_inv_LO_and_EM[LO]*jS_em*jS_inv_LO_and_EM[LO];
        
        // compute quark field RCs (Zq or Sigma1 established from input file!) and store
        jZq_LO_and_EM = compute_jZq(jS_inv_LO_and_EM,ilinmom);
        
        jZq[ilinmom] = jZq_LO_and_EM[LO];
        jZq_em[ilinmom] = - jZq_LO_and_EM[EM];
        
    } // close linmoms loop
    
    PRINT_BIN(jZq);
    PRINT_BIN(jZq_em);
}

void oper_t::compute_bil()
{
    cout<<"Creating the vertices -- ";
    
    // array of input files to be read in a given conf
    FILE* input[combo];
    
    const vector<string> v_path = setup_read_prop(input);
    
    for(int ibilmom=0;ibilmom<_bilmoms;ibilmom++)
    {
        high_resolution_clock::time_point t0=high_resolution_clock::now();

        cout<<endl;
        cout<<"\r\t bilmom = "<<ibilmom+1<<"/"<<_bilmoms<<endl;
        
        const int imom1=bilmoms[ibilmom][1]; // p1
        const int imom2=bilmoms[ibilmom][2]; // p2
        const bool read2=(imom1!=imom2);
        
        // definition of jackknifed propagators
        jprop_t jS1_0(valarray<prop_t>(prop_t::Zero(),_nmr),njacks);
        jprop_t jS1_em(valarray<prop_t>(prop_t::Zero(),_nmr),njacks);
        jprop_t jS2_0(valarray<prop_t>(prop_t::Zero(),_nmr),njacks);
        jprop_t jS2_em(valarray<prop_t>(prop_t::Zero(),_nmr),njacks);
        
        // definition of vertices
        valarray<jvert_t> jVert_LO_and_EM(jvert_t(vvvprop_t(vvprop_t(vprop_t(prop_t::Zero(),16),_nmr),_nmr),njacks),2);
        
        cout<<"- Building vertices"<<endl;
        
        double t_span1=0.0, t_span2=0.0, t_span3=0.0;
        
        for(int i_in_clust=0;i_in_clust<clust_size;i_in_clust++)
            for(int ihit=0;ihit<nhits;ihit++)
            {
                const int mom1=linmoms[imom1][0];
                const int mom2=linmoms[imom2][0];
                
                high_resolution_clock::time_point ta=high_resolution_clock::now();
                
                const vvvprop_t S1=read_prop_mom(input,v_path,i_in_clust,ihit,mom1);
                const vvvprop_t S2=(read2)?read_prop_mom(input,v_path,i_in_clust,ihit,mom2):S1;
                
                high_resolution_clock::time_point tb=high_resolution_clock::now();
                t_span1 += (duration_cast<duration<double>>(tb-ta)).count();
                
                ta=high_resolution_clock::now();
                
                vvvprop_t S1_LO_and_EM = build_prop(jS1_0,jS1_em,S1);
                vvvprop_t S2_LO_and_EM = (read2)?build_prop(jS2_0,jS2_em,S2):S1_LO_and_EM;
                
                tb=high_resolution_clock::now();
                t_span2 += (duration_cast<duration<double>>(tb-ta)).count();
                
                const vvprop_t S1_em = S1_LO_and_EM[EM];
                const vvprop_t S2_em = S2_LO_and_EM[EM];
                
                ta=high_resolution_clock::now();

                build_vert(S1,S2,S1_em,S2_em,jVert_LO_and_EM);
                
                tb=high_resolution_clock::now();
                t_span3 += (duration_cast<duration<double>>(tb-ta)).count();
            }
        cout<<"\t read: "<<t_span1<<" s"<<endl;
        cout<<"\t build prop: "<<t_span2<<" s"<<endl;
        cout<<"\t build vert: "<<t_span3<<" s"<<endl;

    
        cout<<"- Jackknife of propagators and vertices"<<endl;
        
        // jackknife averages
        jS1_0=jackknife(jS1_0);
        jS1_em=jackknife(jS1_em);
        jS2_0=(read2)?jackknife(jS2_0):jS1_0;
        jS2_em=(read2)?jackknife(jS2_em):jS1_em;
        
        jVert_LO_and_EM[LO]=jackknife(jVert_LO_and_EM[LO]);
        jVert_LO_and_EM[EM]=jackknife(jVert_LO_and_EM[EM]);
        
        
        cout<<"- Inverting propagators"<<endl;

        // invert propagators
        vvvprop_t jS1_inv_LO_and_EM(vvprop_t(vprop_t(prop_t::Zero(),_nmr),njacks),2);
        vvvprop_t jS2_inv_LO_and_EM(vvprop_t(vprop_t(prop_t::Zero(),_nmr),njacks),2);
        jS1_inv_LO_and_EM[LO] = invert_jprop(jS1_0);
        jS1_inv_LO_and_EM[EM] = jS1_inv_LO_and_EM[LO]*jS1_em*jS1_inv_LO_and_EM[LO];
        jS2_inv_LO_and_EM[LO] = (read2)?invert_jprop(jS2_0):jS1_inv_LO_and_EM[LO];
        jS2_inv_LO_and_EM[EM] = (read2)?jS2_inv_LO_and_EM[LO]*jS2_em*jS2_inv_LO_and_EM[LO]:jS1_inv_LO_and_EM[EM];
        
//        cout<<"- Computing Zq"<<endl;
//        
//        // compute Zq relative to imom1 and eventually to imom2
//        vvvd_t jZq1_LO_and_EM = compute_jZq(jS1_inv_LO_and_EM,imom1);
//        vvvd_t jZq2_LO_and_EM = (read2)?compute_jZq(jS2_inv_LO_and_EM,imom2):jZq1_LO_and_EM;
//        
//        jZq[imom1] = jZq1_LO_and_EM[LO];
//        jZq_em[imom1] = - jZq1_LO_and_EM[EM];
//        
//        if(read2)
//        {
//            jZq[imom2] = jZq2_LO_and_EM[LO];
//            jZq_em[imom2] = - jZq2_LO_and_EM[EM];
//        }
        
        cout<<"- Computing bilinears"<<endl;
        
        // compute the projected green function (S,V,P,A,T)
        vvvvvd_t jG_LO_and_EM = compute_pr_bil(jS1_inv_LO_and_EM,jVert_LO_and_EM,jS2_inv_LO_and_EM);
        
        jG_0[ibilmom] = jG_LO_and_EM[LO];
        jG_em[ibilmom] = jG_LO_and_EM[EM];
        
        high_resolution_clock::time_point t1=high_resolution_clock::now();
        duration<double> t_span = duration_cast<duration<double>>(t1-t0);
        cout<<"\t\t time: "<<t_span.count()<<" s"<<endl;
        
    } // close mom loop
    cout<<endl<<endl;
    
    PRINT_BIN(jG_0);
    PRINT_BIN(jG_em);
    
}

void oper_t::compute_Zbil()
{
    Zbil_computed=true;
    
    for(int ibilmom=0;ibilmom<_bilmoms;ibilmom++)
    {
        const int imom1=bilmoms[ibilmom][1]; // p1
        const int imom2=bilmoms[ibilmom][2]; // p2
        
        //compute Z's according to 'riqed.pdf', one for each momentum
#pragma omp parallel for collapse(4)
        for(int ijack=0;ijack<njacks;ijack++)
            for(int mr_fw=0;mr_fw<_nmr;mr_fw++)
                for(int mr_bw=0;mr_bw<_nmr;mr_bw++)
                    for(int ibil=0;ibil<nbil;ibil++)
                    {
                        jZ[ibilmom][ibil][ijack][mr_fw][mr_bw] = sqrt(jZq[imom1][ijack][mr_fw]*jZq[imom2][ijack][mr_bw])/jG_0[ibilmom][ibil][ijack][mr_fw][mr_bw];
                        
                        jZ_em[ibilmom][ibil][ijack][mr_fw][mr_bw] = jG_em[ibilmom][ibil][ijack][mr_fw][mr_bw]/jG_0[ibilmom][ibil][ijack][mr_fw][mr_bw] + 0.5*(jZq_em[imom1][ijack][mr_fw]/jZq[imom1][ijack][mr_fw] + jZq_em[imom2][ijack][mr_bw]/jZq[imom2][ijack][mr_bw]);
                    }
        
    }// close mom loop
}


oper_t oper_t::average_r(/*const bool recompute_Zbil*/)
{
    cout<<"Averaging over r"<<endl<<endl;
    
    oper_t out=(*this);
    
    out._nr=1;
    out._nm=_nm;
    out._nmr=(out._nm)*(out._nr);
    
    out.allocate();
    
    if(UseEffMass==1)
    {
        vvvd_t eff_mass_temp(vvd_t(vd_t(0.0,out._nmr),out._nmr),njacks);
        
        for(int ijack=0;ijack<njacks;ijack++)
            for(int mA=0; mA<_nm; mA++)
                for(int mB=0; mB<_nm; mB++)
                    for(int r=0; r<_nr; r++)
                        eff_mass_temp[ijack][mA][mB] += eff_mass[ijack][r+_nr*mA][r+_nr*mB]/_nr;
        
        eff_mass=eff_mass_temp;
    }
    
    
    for(int ilinmom=0;ilinmom<_linmoms;ilinmom++)
    {
        vvd_t jZq_mom_temp(vd_t(0.0,out._nmr),njacks);
        vvd_t jZq_em_mom_temp(vd_t(0.0,out._nmr),njacks);
        
        for(int m=0; m<_nm; m++)
            for(int r=0; r<_nr; r++)
            {
                //LO
                for(int ijack=0;ijack<njacks;ijack++) jZq_mom_temp[ijack][m] += jZq[ilinmom][ijack][r+_nr*m]/_nr;
                //EM
                for(int ijack=0;ijack<njacks;ijack++) jZq_em_mom_temp[ijack][m] += jZq_em[ilinmom][ijack][r+_nr*m]/_nr;
            }
        
        (out.jZq)[ilinmom] = jZq_mom_temp;
        (out.jZq_em)[ilinmom] = jZq_em_mom_temp;
        
    }
    for(int ibilmom=0;ibilmom<_bilmoms;ibilmom++)
    {
        jproj_t jG_0_mom_temp(vvvd_t(vvd_t(vd_t(0.0,out._nmr),out._nmr),njacks),nbil);
        jproj_t jG_em_mom_temp(vvvd_t(vvd_t(vd_t(0.0,out._nmr),out._nmr),njacks),nbil);
        
        for(int mA=0; mA<_nm; mA++)
            for(int mB=0; mB<_nm; mB++)
                for(int r=0; r<_nr; r++)
                {
                    //LO
                    for(int ijack=0;ijack<njacks;ijack++)
                        for(int ibil=0; ibil<5; ibil++)
                            jG_0_mom_temp[ibil][ijack][mA][mB] += jG_0[ibilmom][ibil][ijack][r+_nr*mA][r+_nr*mB]/_nr;
                    //EM
                    for(int ijack=0;ijack<njacks;ijack++)
                        for(int ibil=0; ibil<5; ibil++)
                            jG_em_mom_temp[ibil][ijack][mA][mB] += jG_em[ibilmom][ibil][ijack][r+nr*mA][r+nr*mB]/nr;
                }
        
        (out.jG_0)[ibilmom]=jG_0_mom_temp;
        (out.jG_em)[ibilmom]=jG_em_mom_temp;
    }
    
    out.compute_Zbil();
    
    return out;
}
    

oper_t oper_t::chiral_extr()
{
    cout<<"Chiral extrapolation"<<endl<<endl;
    
    oper_t out=(*this);
    
    out._nr=_nr;
    out._nm=1;
    out._nmr=(out._nm)*(out._nr);
    
//    resize_output(out);
    out.allocate();
    
    vvvvd_t G_0_err = get<1>(ave_err(jG_0));    //[imom][ibil][mr1][mr2]
    vvvvd_t G_em_err = get<1>(ave_err(jG_em));
    
    vvd_t Zq_err = get<1>(ave_err(jZq));        //[imom][mr]
    vvd_t Zq_em_err = get<1>(ave_err(jZq_em));
    
    //Sum of quark masses for the extrapolation
//    vd_t mass_sum(0.0,10);
//    int i_sum = 0;
//    for (int i=0; i<nm; i++)
//        for(int j=i;j<nm;j++)
//        {
//            mass_sum[i_sum] = mass_val[i]+mass_val[j];
//            i_sum++;
//        }

    // average of eff_mass
    vvd_t M_eff = get<0>(ave_err(eff_mass));
    
    //range for fit Zq
    int x_min_q=0;
    int x_max_q=_nm-1;
    
    // range for fit bilinears
    int x_min=0;
    int x_max=_nm*(_nm+1)/2-1;
    
    // number of fit parameters for bilinears
    int npar[5]={3,2,3,2,2};
    
    //extrapolate Zq
    for(int ilinmom=0;ilinmom<_linmoms;ilinmom++)
    {
        for(int r=0; r<_nr; r++)
        {
            vvd_t coord_q(vd_t(0.0,_nm),2); // coords at fixed r
            
            vvvd_t jZq_r(vvd_t(vd_t(0.0,_nm),njacks),_linmoms);
            vvvd_t jZq_em_r(vvd_t(vd_t(0.0,_nm),njacks),_linmoms);
            
            vvd_t Zq_err_r(vd_t(0.0,_nm),_linmoms);
            vvd_t Zq_em_err_r(vd_t(0.0,_nm),_linmoms);
            
            for(int m=0; m<_nm; m++)
            {
                int mr = r + _nr*m;
                
                coord_q[0][m] = 1.0;
                if(UseEffMass==0)
                    coord_q[1][m]= mass_val[m];
                else if(UseEffMass==0)
                    coord_q[1][m] = pow(M_eff[mr][mr],2.0);
                
                for(int ijack=0;ijack<njacks;ijack++)
                {
                    jZq_r[ilinmom][ijack][m]=jZq[ilinmom][ijack][mr];
                    jZq_em_r[ilinmom][ijack][m]=jZq_em[ilinmom][ijack][mr];
                }
                
                Zq_err_r[ilinmom][m]=Zq_err[ilinmom][mr];
                Zq_em_err_r[ilinmom][m]=Zq_em_err[ilinmom][mr];
            }
            
            vvd_t jZq_pars_mom_r = polyfit(coord_q,2,Zq_err_r[ilinmom],jZq_r[ilinmom],x_min_q,x_max_q);
            vvd_t jZq_em_pars_mom_r = polyfit(coord_q,2,Zq_em_err_r[ilinmom],jZq_em_r[ilinmom],x_min_q,x_max_q);
            
            for(int ijack=0; ijack<njacks; ijack++)
            {
                (out.jZq)[ilinmom][ijack][r]=jZq_pars_mom_r[ijack][0];
                (out.jZq_em)[ilinmom][ijack][r]=jZq_em_pars_mom_r[ijack][0];
            }
        }
    }
    
    //extrapolate bilinears
    for(int ibilmom=0;ibilmom<_bilmoms;ibilmom++)
    {
        for(int r1=0; r1<_nr; r1++)
            for(int r2=0; r2<_nr; r2++)
            {
                vvd_t coord_bil(vd_t(0.0,_nm*(_nm+1)/2),3); // coords at fixed r1 and r2
                
                vvvvd_t jG_0_r1_r2(vvvd_t(vvd_t(vd_t(0.0,_nm*(_nm+1)/2),njacks),nbil),_bilmoms);
                vvvvd_t jG_em_r1_r2(vvvd_t(vvd_t(vd_t(0.0,_nm*(_nm+1)/2),njacks),nbil),_bilmoms);
                
                vvvd_t G_0_err_r1_r2(vvd_t(vd_t(0.0,_nm*(_nm+1)/2),nbil),_bilmoms);
                vvvd_t G_em_err_r1_r2(vvd_t(vd_t(0.0,_nm*(_nm+1)/2),nbil),_bilmoms);

                int ieq=0;
                for(int m1=0; m1<_nm; m1++)
                    for(int m2=m1; m2<_nm; m2++)
                    {
                        int mr1 = r1 + _nr*m1;
                        int mr2 = r2 + _nr*m2;

                        coord_bil[0][ieq] = 1.0;
                        if(UseEffMass==0)
                        {
                            coord_bil[1][ieq] = mass_val[m1]+mass_val[m2];  // (am1+am2)
                            coord_bil[2][ieq] = 1.0/coord_bil[1][ieq];    // 1/(am1+am2)
                        }
                        else if(UseEffMass==1)
                        {
                            coord_bil[1][ieq] = pow((M_eff[mr1][mr2]+M_eff[mr2][mr1])/2.0,2.0);   //M^2 (averaged over equivalent combinations)
                            coord_bil[2][ieq] = 1.0/coord_bil[1][ieq];  //1/M^2
                        }
                    
                        for(int ibil=0;ibil<nbil;ibil++)
                        {
                            for(int ijack=0;ijack<njacks;ijack++)
                            {
                                jG_0_r1_r2[ibilmom][ibil][ijack][ieq] = (jG_0[ibilmom][ibil][ijack][mr1][mr2]/*+jG_0[ibilmom][ibil][ijack][r1+_nr*m2][r2+_nr*m1])/2.0*/);
                                jG_em_r1_r2[ibilmom][ibil][ijack][ieq] = (jG_em[ibilmom][ibil][ijack][mr1][mr2]/*+jG_em[ibilmom][ibil][ijack][r1+_nr*m2][r2+_nr*m1])/2.0*/);
                            }
                            
                            G_0_err_r1_r2[ibilmom][ibil][ieq] = (G_0_err[ibilmom][ibil][mr1][mr2]/* + G_0_err[ibilmom][ibil][r1+_nr*m2][r2+_nr*m1])/2.0*/);
                            G_em_err_r1_r2[ibilmom][ibil][ieq] = (G_em_err[ibilmom][ibil][mr1][mr2] /*+ G_em_err[ibilmom][ibil][r1+_nr*m2][r2+_nr*m1])/2.0*/);
                        }
                        
                        ieq++;
                    }
                
                for(int ibil=0;ibil<nbil;ibil++)
                {
                    vvd_t jG_0_pars_mom_ibil_r1_r2 = polyfit(coord_bil,npar[ibil],G_0_err_r1_r2[ibilmom][ibil],jG_0_r1_r2[ibilmom][ibil],x_min,x_max);
                    vvd_t jG_em_pars_mom_ibil_r1_r2 = polyfit(coord_bil,npar[ibil],G_em_err_r1_r2[ibilmom][ibil],jG_em_r1_r2[ibilmom][ibil],x_min,x_max);
                    
                    for(int ijack=0;ijack<njacks;ijack++)
                    {
//                        if(ibil==0 or ibil==2)
//                            for(int ieq=0;ieq<neq;ieq++)
//                            {
//                                // Goldstone pole subtraction from bilinears
//                                jG_0_ave_r[imom][ibil][ijack][ieq] -= jG_0_pars_mom[ibil][ijack][2];
//                                jG_em_ave_r[imom][ibil][ijack][ieq] -= jG_em_pars_mom[ibil][ijack][2];
//                            }
                        
                        // extrapolated value
                        (out.jG_0)[ibilmom][ibil][ijack][r1][r2] = jG_0_pars_mom_ibil_r1_r2[ijack][0];
                        (out.jG_em)[ibilmom][ibil][ijack][r1][r2] = jG_em_pars_mom_ibil_r1_r2[ijack][0];
                    }
                }
            }
    }
    
    out.compute_Zbil();
    
    return out;
}

oper_t oper_t::subtract()
{
    cout<<"Subtracting the O(a2) effects"<<endl<<endl;
    
    oper_t out=(*this);
    
//    resize_output(out);
    out.allocate();
    
#pragma omp parallel for collapse(3)
    for(int ilinmom=0;ilinmom<_linmoms;ilinmom++)
        for(int ijack=0;ijack<njacks;ijack++)
            for(int mr1=0; mr1<_nmr; mr1++)
            {
                (out.jZq)[ilinmom][ijack][mr1] = jZq[ilinmom][ijack][mr1] - subtraction_q(ilinmom,LO);
                (out.jZq_em)[ilinmom][ijack][mr1] = jZq_em[ilinmom][ijack][mr1] + /*(!)*/ subtraction_q(ilinmom,EM)*jZq[ilinmom][ijack][mr1];
                // N.B.: the subtraction gets an extra minus sign due to the definition of the e.m. expansion!
            }
    
#pragma omp parallel for collapse(5)
    for(int ibilmom=0;ibilmom<_bilmoms;ibilmom++)
        for(int ibil=0;ibil<5;ibil++)
            for(int ijack=0;ijack<njacks;ijack++)
                for(int mr1=0; mr1<_nmr; mr1++)
                    for(int mr2=0; mr2<_nmr; mr2++)
                    {
                        (out.jG_0)[ibilmom][ibil][ijack][mr1][mr2] = jG_0[ibilmom][ibil][ijack][mr1][mr2] - subtraction(ibilmom,ibil,LO);
                        (out.jG_em)[ibilmom][ibil][ijack][mr1][mr2] = jG_em[ibilmom][ibil][ijack][mr1][mr2] - subtraction(ibilmom,ibil,EM)*jG_0[ibilmom][ibil][ijack][mr1][mr2];
                    }
    
    out.compute_Zbil();
    
    return out;
}

oper_t oper_t::evolve()
{
    cout<<"Evolving the Z's to the scale 1/a"<<endl<<endl;
    
    oper_t out=(*this);

    double cq=0.0;
    vd_t cO(0.0,5);
    
    for(int ilinmom=0;ilinmom<_linmoms;ilinmom++)
    {
        cq=q_evolution_to_RIp_ainv(Nf,ainv,p2[ilinmom]);
        
        for(int ijack=0;ijack<njacks;ijack++)
            for(int mr1=0; mr1<_nmr; mr1++)
            {
                (out.jZq)[ilinmom][ijack][mr1] = jZq[ilinmom][ijack][mr1]/cq;
                (out.jZq_em)[ilinmom][ijack][mr1] = jZq_em[ilinmom][ijack][mr1]/cq;
            }
    }

    for(int ibilmom=0;ibilmom<_bilmoms;ibilmom++)
    {
        // Note that ZV  ZA are RGI because they're protected by the WIs
        cO[0]=S_evolution_to_RIp_ainv(Nf,ainv,p2[ibilmom]); //S
        cO[1]=1.0;                                       //A
        cO[2]=P_evolution_to_RIp_ainv(Nf,ainv,p2[ibilmom]); //P
        cO[3]=1.0;                                       //V
        cO[4]=T_evolution_to_RIp_ainv(Nf,ainv,p2[ibilmom]); //T
        
        for(int ibil=0;ibil<5;ibil++)
            for(int ijack=0;ijack<njacks;ijack++)
                for(int mr1=0; mr1<_nmr; mr1++)
                    for(int mr2=0; mr2<_nmr; mr2++)
                    {
                        (out.jZ)[ibilmom][ibil][ijack][mr1][mr2] = jZ[ibilmom][ibil][ijack][mr1][mr2]/cO[ibil];
                        (out.jZ_em)[ibilmom][ibil][ijack][mr1][mr2] = jZ_em[ibilmom][ibil][ijack][mr1][mr2]/cO[ibil];
                    }
        
    }
    
    return out;
}

int mom_list_xyz(const size_t imom)
{
    return abs(mom_list[imom][1])*abs(mom_list[imom][2])*abs(mom_list[imom][3]);
}

oper_t oper_t::average_equiv_moms()
{
    cout<<"Averaging over the equivalent momenta -- ";
    
    oper_t out=(*this);
    
    // Find equivalent linmoms
    int tag=0, tag_aux=0;
    double eps=1.0e-15;
    
    vector<int> tag_lin_vector;
    tag_lin_vector.push_back(0);
    
    // Tag assignment to linmoms
    for(int imom=0;imom<_linmoms;imom++)
    {
        int count_no=0;
        
        for(int j=0;j<imom;j++)
        {
            if( 2.0*abs(p2_tilde[j]-p2_tilde[imom])<eps*(p2_tilde[j]+p2_tilde[imom]) && mom_list_xyz(j)==mom_list_xyz(imom) &&
               2.0*abs(abs(p[j][0])-abs(p[imom][0]))<eps*(abs(p[j][0])+abs(p[imom][0])) )
            {
                tag_aux = tag_lin_vector[j];
            }else count_no++;
            
            if(count_no==imom)
            {
                tag++;
                tag_lin_vector.push_back(tag);
            }else if(j==imom-1)
            {
                tag_lin_vector.push_back(tag_aux);
            }
        }
    }
    
    // number of equivalent linmoms
    int neq_lin_moms = tag+1;
    neqmoms = neq_lin_moms;
    
    out._linmoms=neq_lin_moms;
    cout<<"found: "<<out._linmoms<<" equivalent linmoms ";
    (out.linmoms).resize(out._linmoms);
    
    vector<double> p2_tilde_eqmoms(out._linmoms,0.0);


    // count the different tags
    vector<int> count_tag_lin_vector(out._linmoms);
    int count=0;
    for(int tag=0;tag<out._linmoms;tag++)
    {
        count=0;
        for(int imom=0;imom<_linmoms;imom++)
        {
            if(tag_lin_vector[imom]==tag) count++;
        }
        count_tag_lin_vector[tag]=count;
    }
    
    for(int tag=0;tag<out._linmoms;tag++)
        for(int imom=0;imom<_linmoms;imom++)
        {
            if(tag_lin_vector[imom]==tag)
            {
                // fill the new linmoms and p2tilde
                out.linmoms[tag] = {imom};
                p2_tilde_eqmoms[tag] = p2_tilde[imom];
//                cout<<"{"<<tag<<"}"<<endl;
            }
        }
    
    PRINT(p2_tilde_eqmoms);

    
    // Find equivalent bilmoms
    tag=0, tag_aux=0;
    
    vector<int> tag_bil_vector;
    tag_bil_vector.push_back(0);
    
    
    //Tag assignment to bilmoms
    for(int ibilmom=0;ibilmom<_bilmoms;ibilmom++)
    {
        int count_no=0;
        
        const int imom1=bilmoms[ibilmom][1]; // p1
        const int imom2=bilmoms[ibilmom][2]; // p2
        
        for(int j=0;j<ibilmom;j++)
        {
            const int imomA=bilmoms[j][1]; // p1
            const int imomB=bilmoms[j][2]; // p2
            
            if( (tag_lin_vector[imom1]==tag_lin_vector[imomA] and tag_lin_vector[imom2]==tag_lin_vector[imomB])
               or (tag_lin_vector[imom1]==tag_lin_vector[imomB] and tag_lin_vector[imom2]==tag_lin_vector[imomA]))
//            if(tag_lin_vector[imom1]+tag_lin_vector[imom2]==tag_lin_vector[imomA]+tag_lin_vector[imomB] and
//               tag_lin_vector[imom1]*tag_lin_vector[imom2]==tag_lin_vector[imomA]*tag_lin_vector[imomB])
            {
                tag_aux=tag_bil_vector[j];
            }else count_no++;
            
            if(count_no==ibilmom)
            {
                tag++;
                tag_bil_vector.push_back(tag);
            }else if(j==ibilmom-1)
            {
                tag_bil_vector.push_back(tag_aux);
            }
        }
    }
    
    // number of equivalent bilmoms
    int neq_bil_moms = tag+1;
    
    out._bilmoms=neq_bil_moms;
    cout<<"and "<<neq_bil_moms<<" equivalent bilmoms "<<endl<<endl;
    (out.bilmoms).resize(out._bilmoms);
    
    // count the different tags
    vector<int> count_tag_bil_vector(out._bilmoms);
    count=0;
    for(int tag=0;tag<out._bilmoms;tag++)
    {
        count=0;
        for(int imom=0;imom<_bilmoms;imom++)
        {
            if(tag_bil_vector[imom]==tag) count++;
        }
        count_tag_bil_vector[tag]=count;
    }
    
    for(int tag=0;tag<out._bilmoms;tag++)
        for(int ibilmom=0;ibilmom<_bilmoms;ibilmom++)
        {
            if(tag_bil_vector[ibilmom]==tag)
            {
                // fill the new bilmoms
                const int imom0=bilmoms[ibilmom][0]; // k
                const int imom1=bilmoms[ibilmom][1]; // p1
                const int imom2=bilmoms[ibilmom][2]; // p2
                
                out.bilmoms[tag] = {imom0,imom1,imom2};
//                cout<<tag<<" {"<<imom0<<","<<imom1<<","<<imom2<<"}"<<endl;
            }
        }
    
//    resize_output(out);
    out.allocate();
    
    // initialize to zero
#pragma omp parallel for collapse(3)
    for(int tag=0;tag<neq_lin_moms;tag++)
        for(int ijack=0;ijack<njacks;ijack++)
            for(int mr1=0; mr1<_nmr; mr1++)
            {
                (out.jZq)[tag][ijack][mr1]=0.0;
                (out.jZq_em)[tag][ijack][mr1]=0.0;
            }
#pragma omp parallel for collapse(5)
    for(int tag=0;tag<neq_bil_moms;tag++)
        for(int ibil=0;ibil<5;ibil++)
            for(int ijack=0;ijack<njacks;ijack++)
                for(int mr1=0; mr1<_nmr; mr1++)
                    for(int mr2=0; mr2<_nmr; mr2++)
                    {
                        (out.jZ)[tag][ibil][ijack][mr1][mr2]=0.0;
                        (out.jZ_em)[tag][ibil][ijack][mr1][mr2]=0.0;
                    }
    
    // average over the equivalent momenta
    for(int tag=0;tag<neq_lin_moms;tag++)
        for(int imom=0;imom<_linmoms;imom++)
        {
            if(tag_lin_vector[imom]==tag)
            {
                for(int ijack=0;ijack<njacks;ijack++)
                    for(int mr1=0; mr1<_nmr; mr1++)
                    {
                        (out.jZq)[tag][ijack][mr1]+=jZq[imom][ijack][mr1]/count_tag_lin_vector[tag];
                        (out.jZq_em)[tag][ijack][mr1]+=jZq_em[imom][ijack][mr1]/count_tag_lin_vector[tag];
                    }
            }
        }
    for(int tag=0;tag<neq_bil_moms;tag++)
        for(int imom=0;imom<_bilmoms;imom++)
        {
            if(tag_bil_vector[imom]==tag)
            {
                for(int ibil=0;ibil<5;ibil++)
                    for(int ijack=0;ijack<njacks;ijack++)
                        for(int mr1=0; mr1<_nmr; mr1++)
                            for(int mr2=0; mr2<_nmr; mr2++)
                            {
                                (out.jZ)[tag][ibil][ijack][mr1][mr2]+=jZ[imom][ibil][ijack][mr1][mr2]/count_tag_bil_vector[tag];
                                (out.jZ_em)[tag][ibil][ijack][mr1][mr2]+=jZ_em[imom][ibil][ijack][mr1][mr2]/count_tag_bil_vector[tag];
                            }
            }
        }
    
    return out;
}

//! To be used after the average over the equivalent momenta! (*)
void continuum_limit(oper_t out, const int LO_or_EM)
{
    //! (*)
//    int neq_moms = (out.jZq).size();
    int _linmoms=out._linmoms;
    int _bilmoms=out._bilmoms;
    
    vector<double> p2_tilde_eqmoms(_linmoms);
    READ(p2_tilde_eqmoms);
    
    vvd_t jZq_out(vd_t(0.0,_linmoms),njacks);
    vvvd_t jZ_out(vvd_t(vd_t(0.0,_bilmoms),njacks),nbil);
    
    vd_t Zq_err(0.0,_linmoms);
    vvd_t Z_err(vd_t(0.0,_bilmoms),nbil);
    
    if(LO_or_EM==0)
    {
        cout<<"-- Leading Order --"<<endl;
        
#pragma omp parallel for collapse(2)
        for(int imom=0; imom<_linmoms; imom++)
            for(int ijack=0; ijack<njacks; ijack++)
                jZq_out[ijack][imom] = out.jZq[imom][ijack][0];
        
#pragma omp parallel for collapse(3)
        for(int imom=0; imom<_bilmoms; imom++)
            for(int ijack=0; ijack<njacks; ijack++)
                for(int ibil=0; ibil<nbil; ibil++)
                    jZ_out[ibil][ijack][imom] = out.jZ[imom][ibil][ijack][0][0];
        
        vvd_t Zq_err_tmp = get<1>(ave_err(out.jZq));
        vvvvd_t Z_err_tmp = get<1>(ave_err(out.jZ));
        
        for(int imom=0; imom<_linmoms; imom++)
            Zq_err[imom] = Zq_err_tmp[imom][0];
        
        for(int imom=0; imom<_bilmoms; imom++)
            for(int ibil=0; ibil<nbil; ibil++)
                Z_err[ibil][imom] = Z_err_tmp[imom][ibil][0][0];
        
    }
    else if(LO_or_EM==1)
    {
        cout<<"-- EM Correction --"<<endl;
        
#pragma omp parallel for collapse(2)
        for(int imom=0; imom<_linmoms; imom++)
            for(int ijack=0; ijack<njacks; ijack++)
                jZq_out[ijack][imom] = out.jZq_em[imom][ijack][0];
        
#pragma omp parallel for collapse(3)
        for(int imom=0; imom<_bilmoms; imom++)
            for(int ijack=0; ijack<njacks; ijack++)
                for(int ibil=0; ibil<nbil; ibil++)
                    jZ_out[ibil][ijack][imom] = out.jZ_em[imom][ibil][ijack][0][0];
        
        vvd_t Zq_err_tmp = get<1>(ave_err(out.jZq_em));
        vvvvd_t Z_err_tmp = get<1>(ave_err(out.jZ_em));
        
        for(int imom=0; imom<_linmoms; imom++)
            Zq_err[imom] = Zq_err_tmp[imom][0];
        
        for(int imom=0; imom<_bilmoms; imom++)
            for(int ibil=0; ibil<nbil; ibil++)
                Z_err[ibil][imom] = Z_err_tmp[imom][ibil][0][0];
    }
    
    //linear fit Zq
    int range_min=0;  //a2p2~1
    int range_max=_linmoms;
    double p_min_value=p2min;
    
    vvd_t coord_lin_linear(vd_t(0.0,_linmoms),2);
    
    for(int i=0; i<range_max; i++)
    {
        coord_lin_linear[0][i] = 1.0;  //costante
        coord_lin_linear[1][i] = p2_tilde_eqmoms[i];   //p^2
    }
    
    vd_t jZq_out_par_ijack(0.0,2);
    
    double Zq_ave_cont=0.0, sqr_Zq_ave_cont=0.0, Zq_err_cont=0.0;
    
    for(int ijack=0; ijack<njacks; ijack++)
    {
        jZq_out_par_ijack=fit_continuum(coord_lin_linear,Zq_err,jZq_out[ijack],range_min,range_max,p_min_value);
        
        Zq_ave_cont += jZq_out_par_ijack[0]/njacks;
        sqr_Zq_ave_cont += jZq_out_par_ijack[0]*jZq_out_par_ijack[0]/njacks;
    }
    
    Zq_err_cont=sqrt((double)(njacks-1))*sqrt(sqr_Zq_ave_cont-Zq_ave_cont*Zq_ave_cont);
    
    cout<<"ZQ = "<<Zq_ave_cont<<" +/- "<<Zq_err_cont<<endl;
    
    //linear fit Z
    range_min=0;  //a2p2~1
    range_max=_bilmoms;
    
    vvd_t coord_bil_linear(vd_t(0.0,_bilmoms),2);
    
    for(int i=0; i<range_max; i++)
    {
//        int imomk = (out.bilmoms)[i][0];  //(!!!!!!!)
        int imomk = i;      /// it will work temporarily only for RIMOM
        
        coord_bil_linear[0][i] = 1.0;  //costante
        coord_bil_linear[1][i] = p2_tilde_eqmoms[imomk];   //p^2
    }
    
    vvd_t jZ_out_par_ijack(vd_t(0.0,2),nbil);
    vd_t Z_ave_cont(0.0,nbil), sqr_Z_ave_cont(0.0,nbil), Z_err_cont(0.0,nbil);
    
    for(int ijack=0; ijack<njacks; ijack++)
        for(int ibil=0; ibil<nbil; ibil++)
        {
            jZ_out_par_ijack[ibil]=fit_continuum(coord_bil_linear,Z_err[ibil],jZ_out[ibil][ijack],range_min,range_max,p_min_value);
            
            Z_ave_cont[ibil] += jZ_out_par_ijack[ibil][0]/njacks;
            sqr_Z_ave_cont[ibil] += jZ_out_par_ijack[ibil][0]*jZ_out_par_ijack[ibil][0]/njacks;
        }
    
    for(int ibil=0; ibil<nbil;ibil++)
        Z_err_cont[ibil]=sqrt((double)(njacks-1))*sqrt(fabs(sqr_Z_ave_cont[ibil]-Z_ave_cont[ibil]*Z_ave_cont[ibil]));
    
    vector<string> bil={"S","A","P","V","T"};
    
    for(int ibil=0; ibil<nbil;ibil++)
    {
        cout<<"Z"<<bil[ibil]<<" = "<<Z_ave_cont[ibil]<<" +/- "<<Z_err_cont[ibil]<<endl;
    }
    
//    vector<double> pert={-0.0695545,-0.100031,-0.118281,-0.130564,-0.108664};
//    
//    if(LO_or_EM==1)
//    {
//        cout<<"Z divided by the perturbative estimates (to be evolved in MSbar"
//    for(int ibil=0;i<nbil;ibil++)
//    {
//        cout<<"Z"<<bil[ibil]<<"(fact) = "<<A_bil[ibil]/pert[ibil]<<" +/- "<<A_err[ibil]/pert[ibil]<<endl;
//    }
//    }
    
    cout<<endl;
}

void oper_t::plot(const string suffix)
{
    oper_t in=(*this);
    
    Zq_tup Zq_ave_err = ave_err(in.jZq);
    Zq_tup Zq_em_ave_err = ave_err(in.jZq_em);
    
    Zbil_tup Zbil_ave_err = ave_err(in.jZ);
    Zbil_tup Zbil_em_ave_err = ave_err(in.jZ_em);
   
    vvd_t Zq_ave = get<0>(Zq_ave_err);        //[imom][mr]
    vvd_t Zq_em_ave = get<0>(Zq_em_ave_err);
    
    vvd_t Zq_err = get<1>(Zq_ave_err);        //[imom][mr]
    vvd_t Zq_em_err = get<1>(Zq_em_ave_err);
    
    vvvvd_t Z_ave = get<0>(Zbil_ave_err);    //[imom][ibil][mr1][mr2]
    vvvvd_t Z_em_ave = get<0>(Zbil_em_ave_err);
    
    vvvvd_t Z_err = get<1>(Zbil_ave_err);    //[imom][ibil][mr1][mr2]
    vvvvd_t Z_em_err = get<1>(Zbil_em_ave_err);
    
    vector<string> bil={"S","A","P","V","T"};
    
    ofstream Zq_data, Zq_em_data;
    vector<ofstream> Zbil_data(nbil), Zbil_em_data(nbil);
    
    Zq_data.open("plots/Zq"+(suffix!=""?("_"+suffix):string(""))+".txt");
    Zq_em_data.open("plots/Zq_EM"+(suffix!=""?("_"+suffix):string(""))+".txt");
    
    vector<double> p2t;
    
    if(in._linmoms==moms)
    {
//        cout<<"A"<<endl;
        p2t.resize(in._linmoms);
        READ2(p2t,p2_tilde)
    }
    else
    {
//        cout<<"B"<<endl;
        p2t.resize(in._linmoms);
        READ2(p2t,p2_tilde_eqmoms);
    }
    
    for(int imom=0; imom<in._linmoms; imom++)
    {
        Zq_data<<p2t[imom]<<"\t"<<Zq_ave[imom][0]<<"\t"<<Zq_err[imom][0]<<endl;
        Zq_em_data<<p2t[imom]<<"\t"<<Zq_em_ave[imom][0]<<"\t"<<Zq_em_err[imom][0]<<endl;
    }
    
    for(int ibil=0;ibil<nbil;ibil++)
    {
        Zbil_data[ibil].open("plots/Z"+bil[ibil]+(suffix!=""?("_"+suffix):string(""))+".txt");
        Zbil_em_data[ibil].open("plots/Z"+bil[ibil]+"_EM"+(suffix!=""?("_"+suffix):string(""))+".txt");
        
        for(int imom=0; imom<in._bilmoms; imom++)
        {
//            int imomq = in.bilmoms[imom][0];
//            cout<<"imomq: "<<imomq<<endl;
//            int imomk = in.linmoms[imomq][0];
            int imomk = imom;
            
            Zbil_data[ibil]<<p2t[imomk]<<"\t"<<Z_ave[imom][ibil][0][0]<<"\t"<<Z_err[imom][ibil][0][0]<<endl;
            Zbil_em_data[ibil]<<p2t[imomk]<<"\t"<<Z_em_ave[imom][ibil][0][0]<<"\t"<<Z_em_err[imom][ibil][0][0]<<endl;
        }
    }
}
