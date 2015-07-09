//--------------------------------------------------------------------
// galfit.cpp: Members functions of the Galfit class.
//--------------------------------------------------------------------

/*-----------------------------------------------------------------------
 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the
 Free Software Foundation; either version 2 of the License, or (at your
 option) any later version.

 Bbarp;p is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License
 along with Bbarolo; if not, write to the Free Software Foundation,
 Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA

 Correspondence concerning Bbarolo may be directed to:
    Internet email: enrico.diteodoro@unibo.it
-----------------------------------------------------------------------*/

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <iterator>
#include <cmath>
#include <ctime>
#include <string>
#include "../Arrays/cube.hh"
#include "../Arrays/image.hh"
#include "galfit.hh"
#include "galmod.hh"
#include "utils.hh"
#include "lsqfit.hh"
#include "../Map/detection.hh"
#include "progressbar.hh"
#include "smooth3D.hh"
#include "paramguess.hh"

#define VROT  0
#define VDISP 1
#define DENS  2
#define Z0    3
#define INC   4
#define PA    5
#define XPOS  6
#define YPOS  7
#define VSYS  8
#define MAXPAR 9

namespace Model {
	
template <class T>
void Galfit<T>::defaults() {
	
	inDefined 	= false;
	outDefined	= false;
    line_imDefined = false;
	maskDefined = false;
	cfieldAllocated = false;
    chan_noiseAllocated = false;
	wpow = 1;
	anglepar = 3;
	second = false;
	details = false;
    flagErrors=false;
    global=false;
    verb = true;
    func_norm = &Model::Galfit<T>::normalize;
}
template void Galfit<float>::defaults();
template void Galfit<double>::defaults();


template <class T>
Galfit<T>::Galfit() {
	
	defaults();
}
template Galfit<float>::Galfit();
template Galfit<double>::Galfit();


template <class T>
Galfit<T>::Galfit(Cube<T> *c) {
	
	defaults();
    in = c;
    Param *p = &c->pars();
    verb = p->isVerbose();

    arcconv = arcsconv(in->Head().Cunit(0));
    distance = in->pars().getDistance();

	if (in->Head().BeamArea()==0) {
		cout << "\n Beam information is not available in the header: assuming a "
			 << in->pars().getBeamFWHM()*3600 << " arcsec beam. \n You can set the beam "
			 << "with BeamFWHM parameter (in arcsec).\n\n";
		in->Head().setBmaj(in->pars().getBeamFWHM());
		in->Head().setBmin(in->pars().getBeamFWHM());
		in->Head().calcArea();
	}

    chan_noise = new float[in->DimZ()];
    chan_noiseAllocated = true;
    for (int z=0; z< in->DimZ(); z++) chan_noise[z]=1;

    // Try to read ring information from an input file
    Rings<T> file_rings;
    bool radii_b,xpos_b,ypos_b,vsys_b,vrot_b,vdisp_b,z0_b,dens_b,inc_b,pa_b;
    radii_b = getDataColumn(file_rings.radii,p->getRADII());
    xpos_b  = getDataColumn(file_rings.xpos,p->getXPOS());
    ypos_b  = getDataColumn(file_rings.ypos,p->getYPOS());
    vsys_b  = getDataColumn(file_rings.vsys,p->getVSYS());
    vrot_b  = getDataColumn(file_rings.vrot,p->getVROT());
    vdisp_b = getDataColumn(file_rings.vdisp,p->getVDISP());
    z0_b    = getDataColumn(file_rings.z0,p->getZ0());
    dens_b  = getDataColumn(file_rings.dens,p->getDENS());
    inc_b   = getDataColumn(file_rings.inc,p->getINC());
    pa_b 	= getDataColumn(file_rings.phi,p->getPHI());
    bool onefile = radii_b||xpos_b||ypos_b||vsys_b||vrot_b||vdisp_b||z0_b||dens_b||inc_b||pa_b;

    int size[10] = {file_rings.radii.size(),file_rings.xpos.size(),
                    file_rings.ypos.size(), file_rings.vsys.size(),
                    file_rings.vrot.size(),file_rings.vdisp.size(),
                    file_rings.z0.size(),file_rings.dens.size(),
                    file_rings.inc.size(),file_rings.phi.size()};

    int max_size=INT_MAX;
    for (int i=0; i<10; i++) if (size[i]!=0 && size[i]<max_size) max_size=size[i];

    int nr=0;
    T radsep, xpos, ypos, vsys, vrot, vdisp, z0, dens, inc, pa;
    std::string pos[2]={p->getXPOS(),p->getYPOS()};
    std::string ctype[2]={in->Head().Ctype(0),in->Head().Ctype(1)};
    double *pix_center = getCenterCoordinates(pos,ctype);

    bool toEstimate = (p->getRADII()=="-1" && p->getNRADII()==-1) || p->getRADSEP()==-1 ||
                       p->getXPOS()=="-1" || p->getYPOS()=="-1" || p->getVSYS()=="-1" ||
                       p->getVROT()=="-1" || p->getPHI()=="-1"  || p->getINC()=="-1";

    createMask();

    if (toEstimate) {

        if (!in->getIsSearched()) in->Search();
        Detection<T> *largest = in->LargestDetection();

        if (largest==NULL) {
            std::cout << "3DFIT error: No sources detected in the datacube. Cannot fit!!! \n";
            std::terminate();
        }

        if (verb) std::cout << "\n Estimating initial parameters... " << std::flush;
        ParamGuess<T> *init_par = new ParamGuess<T>(in,largest);
        init_par->findInitial();
        init_par->fitEllipse();
        init_par->plotGuess();
        if (verb) std::cout << "Done" << std::endl;

        nr 	  = p->getNRADII()!=-1 ? p->getNRADII() : init_par->nrings;
        radsep= p->getRADSEP()!=-1 ? p->getRADSEP() : init_par->radsep;
        xpos  = p->getXPOS()!="-1" ? getCenterCoord(p->getXPOS(),in->Head().Ctype(0)) : init_par->xcentre;
        ypos  = p->getYPOS()!="-1" ? getCenterCoord(p->getYPOS(),in->Head().Ctype(1)) : init_par->ycentre;
        //xpos  = p->getXPOS()!="-1" ? pix_center[0] : init_par->xcentre;
        //ypos  = p->getYPOS()!="-1" ? pix_center[1] : init_par->ycentre;
        vsys  = p->getVSYS()!="-1" ? atof(p->getVSYS().c_str()) : init_par->vsystem;
        if (distance==-1) distance = VeltoDist(fabs(vsys));
        vrot  = p->getVROT()!="-1" ? atof(p->getVROT().c_str()) : init_par->vrot;
        vdisp = p->getVDISP()!="-1" ? atof(p->getVDISP().c_str()): 8.;					// default is 8 km/s
        z0    = p->getZ0()!="-1" ? atof(p->getZ0().c_str()) : 0.15/KpcPerArc(distance);	// default is 150 parsec
        dens  = p->getDENS()!="-1" ? atof(p->getDENS().c_str()) : 1.;
        inc   = p->getINC()!="-1" ? atof(p->getINC().c_str()) : init_par->inclin;
        pa    = p->getPHI()!="-1" ? atof(p->getPHI().c_str()) : init_par->posang;
        delete init_par;
    }
    else {
        nr 	  = p->getNRADII();
        radsep= p->getRADSEP();
        xpos = getCenterCoord(p->getXPOS(),in->Head().Ctype(0));
        ypos = getCenterCoord(p->getYPOS(),in->Head().Ctype(1));
        //xpos  = pix_center[0];
        //ypos  = pix_center[1];
        vsys  = atof(p->getVSYS().c_str());
        if (distance==-1) distance = VeltoDist(fabs(vsys));
        vrot  = atof(p->getVROT().c_str());
        vdisp = p->getVDISP()!="-1" ? atof(p->getVDISP().c_str()): 8.;					// default is 8 km/s
        z0    = p->getZ0()!="-1" ? atof(p->getZ0().c_str()) : 0.15/KpcPerArc(distance);	// default is 150 parsec
        dens  = p->getDENS()!="-1" ? atof(p->getDENS().c_str()) : 1.;
        inc   = atof(p->getINC().c_str());
        pa    = atof(p->getPHI().c_str());
    }

    if (nr==0) {
        std::cout << "\n 3DFIT ERROR: The number of radii must be > 0! " << std::endl;
        std::terminate();
    }

    nr = nr>0 && nr<max_size ? nr : max_size;
    if (radii_b) {
        radsep = 0;
        for (uint i=1; i<file_rings.radii.size()-1; i++)
            radsep += file_rings.radii[i+1]-file_rings.radii[i];
        radsep/=(file_rings.radii.size()-2);
    }

    inr = new Rings<T>;
    inDefined = true;
    inr->nr 	= nr;
    inr->radsep = radsep;
    for (int i=0; i<inr->nr; i++) {
        if (radii_b) inr->radii.push_back(file_rings.radii[i]);
        else inr->radii.push_back(i*radsep);
        if (vrot_b) inr->vrot.push_back(file_rings.vrot[i]);
        else inr->vrot.push_back(vrot);
        if (vdisp_b) inr->vdisp.push_back(file_rings.vdisp[i]);
        else inr->vdisp.push_back(vdisp);
        if (z0_b) inr->z0.push_back(file_rings.z0[i]);
        else inr->z0.push_back(z0);
        if (dens_b) inr->dens.push_back(file_rings.dens[i]*1.E20);
        else inr->dens.push_back(dens*1.E20);
        if (inc_b) inr->inc.push_back(file_rings.inc[i]);
        else inr->inc.push_back(inc);
        if (pa_b) inr->phi.push_back(file_rings.phi[i]);
        else inr->phi.push_back(pa);
        if (xpos_b) inr->xpos.push_back(file_rings.xpos[i]);
        else inr->xpos.push_back(xpos);
        if (ypos_b) inr->ypos.push_back(file_rings.ypos[i]);
        else inr->ypos.push_back(ypos);
        if (vsys_b) inr->vsys.push_back(file_rings.vsys[i]);
        else inr->vsys.push_back(vsys);
    }

    if (inr->radii[0]!=0) inr->radii[0]=0;
	
    setFree();

    wpow = p->getWFUNC();
    string polyn = makelower(p->getPOLYN());
    if (polyn=="bezier") anglepar=-1;
    else anglepar = 1+atoi(polyn.c_str());
    tol = p->getTOL();
    flagErrors = p->getflagErrors();
	
	if (!in->pars().getflagGalMod()) {
		if (!onefile) showInitial(inr, std::cout);
        else printInitial(inr);
        input(c, inr, mpar, tol);
	}

    outr = new Rings<T>;
    *outr = *inr;
    outDefined = true;

    if (p->getNORM()=="AZIM") func_norm = &Model::Galfit<T>::residuals;
    else func_norm = &Model::Galfit<T>::normalize;
}
template Galfit<float>::Galfit(Cube<float>*);
template Galfit<double>::Galfit(Cube<double>*);


template <class T>
Galfit<T>::~Galfit () {
	if (outDefined) delete outr;
	if (inDefined) delete inr;
    if (line_imDefined) delete line_im;
	if (maskDefined) delete [] mask;
	if (cfieldAllocated) delete [] cfield; 
    if (chan_noiseAllocated) delete [] chan_noise;
}
template Galfit<float>::~Galfit();
template Galfit<double>::~Galfit();


template <class T>
Galfit<T>::Galfit(const Galfit<T> &g) {

	operator=(g);
}
template Galfit<float>::Galfit(const Galfit<float>&);
template Galfit<double>::Galfit(const Galfit<double>&);


template <class T>
Galfit<T>& Galfit<T>::operator=(const Galfit &g) {
	
	if(this==&g) return *this;
	
	this->in	= g.in;
	
	if (inDefined) delete inr;
	this->inDefined = g.inDefined;
	if (inDefined) *inr = *g.inr;
	else this->inr = g.inr;
	
	if (outDefined) delete outr;
	this->outDefined = g.outDefined;
	if (outDefined) {
		outr = new Rings<T>;
		*outr = *g.outr;
	}
	
	if (maskDefined) delete [] mask;
	this->maskDefined = g.maskDefined;
	if (maskDefined) {
		mask = new short[in->NumPix()];
		for (int i=0; i<in->NumPix(); i++)
			this->mask[i] = g.mask[i];
	}
	
	for (int i=0; i<MAXPAR; i++) {
		this->mpar[i] = g.mpar[i];
		this->maxs[i] = g.maxs[i];
		this->mins[i] = g.mins[i];
    }
		
	this->tol		= g.tol;
    this->nfree     = g.nfree;
	this->arcconv	= g.arcconv;							
	this->distance	= g.distance;	
    this->NconX     = g.NconX;
    this->NconY     = g.NconY;
	this->wpow		= g.wpow;
    this->global    = g.global;

	if (cfieldAllocated) delete [] cfield;
	this->cfieldAllocated = g.cfieldAllocated;
	if (cfieldAllocated) {
		this->cfield = new double[NconX*NconY];
		for (int i=0; i<this->NconX*this->NconY; i++)
			this->cfield[i] = g.cfield[i];
    }

    if (chan_noiseAllocated) delete [] chan_noise;
    this->chan_noiseAllocated = g.chan_noiseAllocated;
    if (chan_noiseAllocated) {
        this->chan_noise = new float[this->in->DimZ()];
        for (int i=0; i<this->in->DimZ(); i++)
            this->chan_noise[i] = g.chan_noise[i];
    }
	
    func_norm = g.func_norm;

    if (line_imDefined) delete line_im;
    this->line_imDefined = g.line_imDefined;
    if (this->line_imDefined) {
        this->line_im = g.line_im;
    }

    return *this;
}
template Galfit<float>& Galfit<float>::operator=(const Galfit<float>&);
template Galfit<double>& Galfit<double>::operator=(const Galfit<double>&);


template <class T>
void Galfit<T>::input (Cube<T> *c, Rings<T> *inrings, bool *maskpar, double TOL) {

	in = c;
	inr = inrings;
	tol = TOL;
	
	int nfixed=0;
    for (int i=0; i<MAXPAR; i++) {
		mpar[i] = maskpar[i];
		nfixed += (1-mpar[i]);
	}
    if (nfixed == MAXPAR) {
        std::cout << "GALFIT error: NO free parameters!\n";
        std::terminate();
	}
	nfree = MAXPAR-nfixed;
	
	double kpcperarc = KpcPerArc(distance);
	// Max scaleheight allowed is 2 Kpc.
	maxs[Z0] = 2/kpcperarc;					
	// Min scaleheight allowed is 50 pc.
	mins[Z0] = 0.05/kpcperarc;					
//	if ((inr->z0.front()>maxs[Z0] || inr->z0.front()<mins[Z0]) && !mpar[Z0]) {
//		std::cout << "GALFIT warning: you set a scale height for the disk of "
//				  << inr->z0.front()*kpcperarc*1000 << " pc.\n Do you want to "
//				  << "continue with this value? [Y/N] ";
//		std::string a;
//		std::cin >> a;
//		if (a=="n" || a=="N" || a=="no" || a=="NO") std::terminate();
//	}
	
	maxs[VROT]	= 500;
	mins[VROT]	= 0;
	maxs[VDISP]	= 150;
    mins[VDISP]	= 3;
	maxs[INC] = *max_element(inr->inc.begin(),inr->inc.end())+in->pars().getDELTAINC();
	mins[INC] = *min_element(inr->inc.begin(),inr->inc.end())-in->pars().getDELTAINC();
	maxs[PA]  = *max_element(inr->phi.begin(),inr->phi.end())+in->pars().getDELTAPHI();
	mins[PA]  = *min_element(inr->phi.begin(),inr->phi.end())-in->pars().getDELTAPHI(); 
	if (maxs[INC]>90) maxs[INC] = 90;
	if (mins[INC]<0)  mins[INC] = 0;
	if (maxs[PA]>360) maxs[PA]  = 360;
	if (mins[PA]<0)   mins[PA]  = 0;
	maxs[XPOS] = *max_element(inr->xpos.begin(),inr->xpos.end())+10;
	mins[XPOS] = *min_element(inr->xpos.begin(),inr->xpos.end())-10;
	maxs[YPOS] = *max_element(inr->ypos.begin(),inr->ypos.end())+10;
	mins[YPOS] = *min_element(inr->ypos.begin(),inr->ypos.end())-10;
    maxs[VSYS] = AlltoVel(in->getZphys(in->DimZ()-1), in->Head());
    mins[VSYS] = AlltoVel(in->getZphys(0), in->Head());
	if (maxs[XPOS]>in->DimX()) maxs[XPOS] = in->DimX();
	if (maxs[YPOS]>in->DimY()) maxs[YPOS] = in->DimY();
	if (mins[XPOS]<0)  mins[XPOS] = 0;
	if (mins[YPOS]<0)  mins[YPOS] = 0;

    if (in->pars().getSM()) {
		if (!setCfield()) {
            std::cout << "GALFIT warning: can not set an appropriate convolution "
					  << "field. Turning off the convolution step.\n";
			in->pars().setSM(false);
		}
	}

}
template void Galfit<float>::input(Cube<float>*,Rings<float>*,bool*,double);
template void Galfit<double>::input(Cube<double>*,Rings<double>*,bool*,double);


template <class T>
void Galfit<T>::galfit() {
	
	using namespace std;
	verb = in->pars().isVerbose();
	
	static int n=0;
	n++;
    std::string fileo = in->pars().getOutfolder()+"ringlog"+to_string(n)+".txt";
    remove(fileo.c_str());

    std::ofstream fileout;
    fileout.open(fileo.c_str(), std::ios_base::app);
	
	int m=10;
	fileout	<< left << setw(m) << "RAD(Kpc)"
			<< setw(m)   << "RAD(arcs)"
			<< setw(m+1) << "VROT(km/s)"
			<< setw(m+1) << "DISP(km/s)"
			<< setw(m)   << "INC(deg)" 
			<< setw(m)   << "P.A.(deg)" 
            << setw(m)   << "Z0(pc)"
			<< setw(m)   << "Z0(arcs)"
			<< setw(m)   << "SIG(E20)"
            << setw(m)   << "XPOS(pix)"
			<< setw(m)   << "YPOS(pix)"
			<< setw(m+1) << "VSYS(km/s)";
			
	if (flagErrors) {		
		if (mpar[VROT])  fileout << setw(m) << "E_VROT1" << setw(m) << "E_VROT2";
		if (mpar[VDISP]) fileout << setw(m) << "E_DISP1" << setw(m) << "E_DISP2";
		if (mpar[DENS])  fileout << setw(m) << "E_DENS1" << setw(m) << "E_DENS2";
		if (mpar[Z0])    fileout << setw(m) << "E_Z01"   << setw(m) << "E_Z02";
		if (mpar[INC])   fileout << setw(m) << "E_INC1"  << setw(m) << "E_INC2";
		if (mpar[PA])    fileout << setw(m) << "E_PA1"   << setw(m) << "E_PA2";
		if (mpar[XPOS])  fileout << setw(m) << "E_XPOS1" << setw(m) << "E_XPOS2";
		if (mpar[YPOS])  fileout << setw(m) << "E_YPOS1" << setw(m) << "E_YPOS2";
		if (mpar[VSYS])  fileout << setw(m) << "E_VSYS1" << setw(m) << "E_VSYS2";
	}
		
	fileout << endl; 
	
		
	if (verb) {	
		in->pars().setVerbosity(false);
		cout << showpoint << fixed << setprecision(2) << endl ;
        cout << setfill('=') << setw(40) << right << " 3DFIT " << setw(34) << " ";
		cout << setfill(' ') << endl;
	}

    *outr = *inr;
//    global=false;
//    if (global) {
//        Rings<T> *dring = new Rings<T>;
//        *dring = *inr;
//        T minimum=0;
//        T pmin[nfree*dring->nr];
//        if (!minimize(dring, minimum, pmin)) cout << "DIOCANEEEEEE" << endl;

//        int k=0;
//        if (mpar[VROT])  for (int ir=0; ir<inr->nr; ir++) outr->vrot[ir]=pmin[k++];
//        if (mpar[VDISP]) for (int ir=0; ir<inr->nr; ir++) outr->vdisp[ir]=pmin[k++];
//        if (mpar[DENS])  for (int ir=0; ir<inr->nr; ir++) outr->dens[ir]=pmin[k++];
//        if (mpar[Z0])    for (int ir=0; ir<inr->nr; ir++) outr->z0[ir]=pmin[k++];
//        if (mpar[INC])   for (int ir=0; ir<inr->nr; ir++) outr->inc[ir]=pmin[k++];
//        if (mpar[PA])    for (int ir=0; ir<inr->nr; ir++) outr->phi[ir]=pmin[k++];
//        if (mpar[XPOS])  for (int ir=0; ir<inr->nr; ir++) outr->xpos[ir]=pmin[k++];
//        if (mpar[YPOS])  for (int ir=0; ir<inr->nr; ir++) outr->ypos[ir]=pmin[k++];
//        if (mpar[VSYS])  for (int ir=0; ir<inr->nr; ir++) outr->vsys[ir]=pmin[k++];

//        for (int ir=0; ir<inr->nr; ir++) {
//            float radius = ir==0 ?  inr->radsep/4. : inr->radii[ir];
//            double toKpc = KpcPerArc(distance);
//            fileout << setprecision(3) << fixed << left;
//            fileout << setw(m) << radius*toKpc
//                    << setw(m) << radius
//                    << setw(m+1) << outr->vrot[ir]
//                    << setw(m+1) << outr->vdisp[ir]
//                    << setw(m) << outr->inc[ir]
//                    << setw(m) << outr->phi[ir]
//                    << setw(m) << outr->z0[ir]*toKpc*1000
//                    << setw(m) << outr->z0[ir]
//                    << setw(m) << outr->dens[ir]/1E20
//                    << setw(m) << outr->xpos[ir]
//                    << setw(m) << outr->ypos[ir]
//                    << setw(m+1) << outr->vsys[ir]
//                    << endl;

//        }
//    }
//    else {
        int start_rad = in->pars().getStartRad()<inr->nr ? in->pars().getStartRad() : 0;
        for (int ir=start_rad; ir<inr->nr; ir++) {
            w_r = ir;
            float radius = ir==0 ?  inr->radsep/4. : inr->radii[ir];
            double toKpc = KpcPerArc(distance);
            if (verb) {
                time_t t = time(NULL);
                char Time[11] = "          ";
                strftime (Time,11,"[%T]",localtime(&t));
                cout << fixed << setprecision(2)<<"\n\n Working on ring #"
                     << ir+1 << " with radius " << radius << " arcsec ("
                     << radius*toKpc << " Kpc)... " << Time << std::endl;
            }

            if (ir!=inr->nr-1) {
                if (inr->radii[ir+1]<=inr->radii[ir]) {
                    cout << "3DFIT error: Radii not in increasing order.\n";
                    std::terminate();
                }
            }
            if (inr->radii[ir]<0) {
                cout << "3DFIT error: Negative radius!!!\n";
                std::terminate();
            }

            details = true;
            Rings<T> *dring = new Rings<T>;
            dring->nr = 2;

            if (ir==0) {
                double radsep = (inr->radii[1]-inr->radii[0])/2;
                dring->radii.push_back(inr->radii[0]);
                dring->radii.push_back(inr->radii[0]+radsep);
            }
            else if (ir==inr->nr-1) {
                double radsep = (inr->radii[ir]-inr->radii[ir-1])/2;
                dring->radii.push_back(inr->radii[ir]-radsep);
                dring->radii.push_back(inr->radii[ir]+radsep);
            }
            else {
                double radsep1 = (inr->radii[ir]-inr->radii[ir-1])/2;
                double radsep2 = (inr->radii[ir+1]-inr->radii[ir])/2;
                dring->radii.push_back(inr->radii[ir]-radsep1);
                dring->radii.push_back(inr->radii[ir]+radsep2);
            }

            for (int i=0; i<dring->nr; i++) {
                dring->vrot.push_back(inr->vrot[ir]);
                dring->vdisp.push_back(inr->vdisp[ir]);
                dring->dens.push_back(inr->dens[ir]);
                dring->z0.push_back(inr->z0[ir]);
                dring->inc.push_back(inr->inc[ir]);
                dring->phi.push_back(inr->phi[ir]);
                dring->xpos.push_back(inr->xpos[ir]);
                dring->ypos.push_back(inr->ypos[ir]);
                dring->vsys.push_back(inr->vsys[ir]);
            }

            T minimum=0;
            T pmin[nfree];

            if (!minimize(dring, minimum, pmin)) continue;

            int k=0;
            if (mpar[VROT])  outr->vrot[ir]=pmin[k++];
            if (mpar[VDISP]) outr->vdisp[ir]=pmin[k++];
            if (mpar[DENS])  outr->dens[ir]=pmin[k++];
            if (mpar[Z0])    outr->z0[ir]=pmin[k++];
            if (mpar[INC])   outr->inc[ir]=pmin[k++];
            if (mpar[PA])    outr->phi[ir]=pmin[k++];
            if (mpar[XPOS])  outr->xpos[ir]=pmin[k++];
            if (mpar[YPOS])  outr->ypos[ir]=pmin[k++];
            if (mpar[VSYS])  outr->vsys[ir]=pmin[k++];
            /*
        //////// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        long axis[3] = {in->DimX(),in->DimY(),in->DimZ()};

        Model::Galmod<T> *mod = new Model::Galmod<T>;
        int bhi[2] = {in->DimX(), in->DimY()};
        int blo[2] = {0,0};
        int nv;
        if (in->pars().getNV()==-1) nv=in->DimZ();
        else nv = in->pars().getNV();
        int ltype = in->pars().getLTYPE();
        int cdens = in->pars().getCDENS();
        for (int ii=0; ii<2; ii++) {
            dring->vrot[ii]=outr->vrot[ir];
            dring->vdisp[ii]=outr->vdisp[ir];
            dring->dens[ii]=outr->dens[ir];
            dring->z0[ii]=outr->z0[ir];
            dring->inc[ii]=outr->inc[ir];
            dring->phi[ii]=outr->phi[ir];
            dring->xpos[ii]=outr->xpos[ir];
            dring->ypos[ii]=outr->ypos[ir];
            dring->vsys[ii]=outr->vsys[ir];
        }

        mod->input(in,bhi,blo,dring,nv,ltype,1,cdens);
        mod->calculate();
        mod->smooth();
        residuals(dring,mod->Out()->Array(),bhi,blo);
        std::string mfile = "maremmabucaiola"+to_string(dring->inc.back())+".fits";
        mod->Out()->Head().setDataMax(0.);
        mod->Out()->Head().setDataMin(0.);
        mod->Out()->fitswrite_3d(mfile.c_str());


        ///-------------- SCRITTURA DELL'ANELLO CON ANGOLO AZIMUTHALE -------------
        std::vector<Pixel<T> > *ppix = getRingRegion(dring,bhi,blo);
        T *ringmap = new T[in->DimX()*in->DimY()];
        for (int k=in->DimX()*in->DimY(); k--;) ringmap[k]=log(-1);
        typename std::vector<Pixel<T> >::iterator pix;
        for(pix=ppix->begin();pix<ppix->end();pix++)
            ringmap[pix->getX()+pix->getY()*in->DimX()]=pix->getF();

        Image2D<T> *mm = new Image2D<T>(in->AxisDim());
        mm->saveHead(in->Head());
        mm->Head().setCrpix(0,in->Head().Crpix(0)-blo[0]);
        mm->Head().setCrpix(1,in->Head().Crpix(1)-blo[1]);
        mm->Head().setMinMax(0.,0.);
        mm->saveParam(in->pars());
        mm->setImage(ringmap, in->AxisDim());
        std::string outname = "Ring_"+to_string(ir)+"_"+to_string(outr->inc[ir])
                                +"_"+to_string(outr->phi[ir])+".fits";
        mm->fitswrite_2d(outname.c_str());
        delete mm;
        /// --------------------------------------------------------------------------

        Cube<T> *cuu = new Cube<T>(axis);
        mfile = "pluto"+to_string(dring->inc[0])+".txt";
        std::ofstream outz(mfile.c_str());
        for (int k=in->NumPix(); k--;) cuu->Array()[k] = 0;

        int numPixels=0, numPix_ring=0;
        double reszsum=0, resz[in->DimZ()];
        for (int z=0; z<in->DimZ(); z++) {
            resz[z]=0;
            for(int y=0; y<in->DimY(); y++) {
                for (int x=0; x<in->DimX();x++) {
                    long npix = in->nPix(x,y,z);
                    cuu->Array()[npix] = 0;
                    if (!isNaN(ringmap[x+y*in->DimX()])) {
                        T model = mod->Out()->Array(npix);
                        T obs = in->Array(npix)>0 ? in->Array(npix) : in->stat().getSpread();
                        if (mask[npix]==0 && model==0) continue;
                        else if (mask[npix]==0 && model!=0) obs = 0;
                        numPixels++;
                        resz[z] += fabs(model-obs);
                        cuu->Array()[cuu->nPix(x,y,z)] = in->Array(npix)*mask[npix];
                    }
                }
            }
            reszsum += resz[z];
        }

        for (int z=in->DimZ();z--;) {
        outz << z+1 << "  "<< setw(10) << setprecision(10) << resz[z]/numPixels << " "
                 << setw(10) << resz[z] << endl;
        }

        cout << endl <<setprecision(10)<< reszsum/numPixels <<endl << numPix_ring << " "<<numPixels <<endl;
        outz.close();


        cuu->saveHead(in->Head());
        cuu->Head().setMinMax(0,0);
        std::string name = "diocan_"+to_string(dring->inc.back())+".fits";
        cuu->fitswrite_3d(name.c_str());
        delete cuu;
        delete mod;
        delete ppix;
        delete [] ringmap;




        if (ir<inr->nr-2) {
            Model::Galmod<T> *modd = new Model::Galmod<T>;
            int xdis = ceil((dring->radii.back()+3*dring->z0.back())/(fabs(in->Head().Cdelt(0))*arcconv));
            int ydis = ceil((dring->radii.back()+3*dring->z0.back())/(fabs(in->Head().Cdelt(1))*arcconv));
            int blo[2] = {dring->xpos.back()-xdis, dring->ypos.back()-ydis};
            int bhi[2] = {dring->xpos.back()+xdis, dring->ypos.back()+ydis};
            if (blo[0]<0) blo[0] = 0;
            if (blo[1]<0) blo[1] = 0;
            if (bhi[0]>in->DimX()) bhi[0] = in->DimX();
            if (bhi[1]>in->DimY()) bhi[1] = in->DimY();
            int nv;
            if (in->pars().getNV()==-1) nv=in->DimZ();
            else nv = in->pars().getNV();
            int ltype = in->pars().getLTYPE();
            int cdens = in->pars().getCDENS();
            modd->input(in,bhi,blo,outr,nv,ltype,1,cdens);
            modd->calculate();
            modd->smooth();
            modd->normalize();
            std::string mfile = in->Head().Obname()+"temp.fits";
            modd->Out()->Head().setDataMax(0.);
            modd->Out()->Head().setDataMin(0.);
            modd->Out()->fitswrite_3d(mfile.c_str());
            delete modd;
        }
        else remove((in->Head().Obname()+"temp.fits").c_str());
        */

            /////////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



            if (verb) {
                int m=8;
                int n=11;
                cout << "  Best parameters for ring #" << ir+1
                     << " (fmin = " << setprecision(6) << minimum << "):\n";

                cout << setprecision(2);

                string s;
                s = "    Vrot";
                if (!mpar[VROT]) s += "(f)";
                cout << setw(n) << left << s << setw(3) << right << "= "
                     << setw(m) << outr->vrot[ir] << left << setw(m) << "  km/s";

                s = "        Disp";
                if (!mpar[VDISP]) s += "(f)";
                cout << setw(n+4) << left << s << setw(3) << right << "= "
                     << setw(m-1) << outr->vdisp[ir]
                        << left << setw(m) << "  km/s" << endl;

                s = "    Inc";
                if (!mpar[INC]) s += "(f)";
                cout << setw(n) << left << s << setw(3) << right << "= "
                     << setw(m) << outr->inc[ir] << left << setw(m) << "  deg";

                s = "        PA";
                if (!mpar[PA]) s += "(f)";
                cout << setw(n+4) << left << s << setw(3) << right << "= "
                     << setw(m-1) << outr->phi[ir] << left << setw(m) << "  deg" << endl;

                s = "    Z0";
                if (!mpar[Z0]) s += "(f)";
                cout << setw(n) << left << s << setw(3) << right << "= "
                     << setw(m) << outr->z0[ir]*toKpc << left << setw(m) << "  Kpc";

                s = "        Vsys";
                if (!mpar[VSYS]) s += "(f)";
                cout << setw(n+4) << left << s << setw(3) << right << "= "
                     << setw(m-1) << outr->vsys[ir] << left << setw(m) << "  km/s" << endl;

                s = "    Xpos";
                if (!mpar[XPOS]) s += "(f)";
                cout << setw(n) << left << s << setw(3) << right << "= "
                     << setw(m) << outr->xpos[ir] << left << setw(m) << "  pix";

                s = "        Ypos";
                if (!mpar[YPOS]) s += "(f)";
                cout << setw(n+4) << left << s << setw(3) << right << "= "
                     << setw(m-1) << outr->ypos[ir] << left << setw(m) << "  pix";

                /*
            s = "        CD";----
            if (!mpar[DENS]) s += "(f)";
            cout << setw(n+4) << left << s << setw(3) << right << "= "
                 << setw(m-1) << scientific << setprecision(1)
                 << outr->dens[ir] << left << setw(m) << "  a/cm2";
            */

                cout << endl;

            }


            fileout << setprecision(3) << fixed << left;
            fileout << setw(m) << radius*toKpc
                    << setw(m) << radius
                    << setw(m+1) << outr->vrot[ir]
                    << setw(m+1) << outr->vdisp[ir]
                    << setw(m) << outr->inc[ir]
                    << setw(m) << outr->phi[ir]
                    << setw(m) << outr->z0[ir]*toKpc*1000
                    << setw(m) << outr->z0[ir]
                    << setw(m) << outr->dens[ir]/1E20
                    << setw(m) << outr->xpos[ir]
                    << setw(m) << outr->ypos[ir]
                    << setw(m+1) << outr->vsys[ir];


            T **errors=allocate_2D<T>(2,nfree);
            if (flagErrors) getErrors(dring,errors,ir,minimum);


            if (flagErrors) {
                for (int kk=0; kk<nfree; kk++) {
                    fileout << setw(m) << errors[0][kk] << setw(m) << errors[1][kk];
                }
            }

            fileout << endl;
            deallocate_2D<T>(errors,2);
            delete dring;

        }
  //  }

    fileout.close();


	if (verb) {				  
		cout << setfill('=') << setw(74) << " " << endl << endl; 
		cout << fixed << setprecision(2) << setfill(' ');
		in->pars().setVerbosity(true);
	}
	
	
}
template void Galfit<float>::galfit();
template void Galfit<double>::galfit();


template <class T> 
void Galfit<T>::SecondStage() {
	
	bool isNeeded = mpar[INC] || mpar[PA]   || mpar[Z0] ||
					mpar[XPOS]|| mpar[YPOS] || mpar[VSYS];
	if (!isNeeded) {in->pars().setTwoStage(false); return;}
	second = true;
    if (!in->pars().getFlagSlitfit()) func_norm = &Model::Galfit<T>::normalize;

	int n = outr->nr;
	
    std::vector<T> xx, yy;

    if (anglepar==-1) {
        // Make a bezier interpolation
        std::vector<T> x_bez(n), y_bez_inc(n), y_bez_pa(n), y_bez_z0(n);
        if (mpar[INC]) {
            for (int i=0; i<n; i++) {
                xx.push_back(outr->radii[i]);
                yy.push_back(outr->inc[i]);
            }
            bezier_interp(xx,yy,x_bez,y_bez_inc);
            xx.clear();
            yy.clear();
        }
        if (mpar[PA]) {
            for (int i=0; i<n; i++) {
                xx.push_back(outr->radii[i]);
                yy.push_back(outr->phi[i]);
            }
            bezier_interp(xx,yy,x_bez,y_bez_pa);
            xx.clear();
            yy.clear();
        }
        if (mpar[Z0]) {
            for (int i=0; i<n; i++) {
                xx.push_back(outr->radii[i]);
                yy.push_back(outr->z0[i]);
            }
            bezier_interp(xx,yy,x_bez,y_bez_z0);
            xx.clear();
            yy.clear();
        }

        T xpos_av = findMedian(outr->xpos, n);
        T ypos_av = findMedian(outr->ypos, n);
        T vsys_av = findMedian(outr->vsys, n);

        *inr = *outr;
        for (int i=0; i<n; i++) {
            if (mpar[INC]) inr->inc[i]=y_bez_inc[i];
            if (mpar[PA])  inr->phi[i]=y_bez_pa[i];
            if (mpar[Z0])  inr->z0[i]=y_bez_z0[i];
            inr->xpos[i]=xpos_av;
            inr->ypos[i]=ypos_av;
            inr->vsys[i]=vsys_av;
        }
    }
    else {
        // Make a polynomial fit
        if (n<=anglepar) {
            std::cout << "3DFIT ERROR - Second stage: too few degree of freedom.\n";
            return;
        }
	
        cout << setprecision(4);
	
        Statistics::Stats<T> stats;
        typename std::vector<T>::iterator where;
        T ww[n], cinc[anglepar], cincout[anglepar];
        T cpa[anglepar], cpaerr[anglepar];
        T cz0[anglepar], cz0err[anglepar];
	
        bool mp[anglepar];
        for (int i=0; i<anglepar;i++) mp[i] = true;
        for (int i=0; i<n; i++) ww[i] = 1;
				
        if (mpar[INC]) {
            for (int i=0; i<n; i++) {
                xx.push_back(outr->radii[i]);
                yy.push_back(outr->inc[i]);
            }
            if (n-2>anglepar) {
                where = std::max_element(yy.begin(), yy.end());
                yy.erase(where);
                xx.erase(xx.begin()+std::distance(yy.begin(), where));
                where = std::min_element(yy.begin(), yy.end());
                yy.erase(where);
                xx.erase(xx.begin()+std::distance(yy.begin(), where));
            }
		
            stats.calculate(&yy[0], yy.size());

            Lsqfit<T> lsq(&xx[0],1,&yy[0],ww,xx.size(),cinc,cincout,mp,anglepar,&polyn,&polynd);
            int nrt = lsq.fit();
            if (nrt<0) {
                std::cout << "3DFIT error: cannot least-square fit the inclination.\n";
                in->pars().setTwoStage(false);
                return;
            }
            if (verb) {
                cout << "  Best parameters for inclination:\n";
                for (int i=0; i<anglepar; i++) {
                    string a = "a"+to_string<int>(i)+" = ";
                    cout << setw(43) << right << a << setw(9) << cinc[i] << setw(4)
                         << " ±" << setw(9) << cincout[i] << endl;
                }
            }
            xx.clear();
            yy.clear();
        }
        if (mpar[PA]) {
            for (int i=0; i<n; i++) {
                xx.push_back(outr->radii[i]);
                yy.push_back(outr->phi[i]);
            }
            if (n-2>anglepar) {
                where = std::max_element(yy.begin(), yy.end());
                yy.erase(where);
                xx.erase(xx.begin()+std::distance(yy.begin(), where));
                where = std::min_element(yy.begin(), yy.end());
                yy.erase(where);
                xx.erase(xx.begin()+std::distance(yy.begin(), where));
            }
            Lsqfit<T> lsq(&xx[0],1,&yy[0],ww,xx.size(),cpa,cpaerr,mp,anglepar,&polyn,&polynd);
            int nrt = lsq.fit();
            if (nrt<0) {
                std::cout << "3DFIT error: cannot least-square fit the position angle.\n";
                in->pars().setTwoStage(false);
                return;
            }
            if (verb) {
                cout << "  Best parameters for position angle:\n";
                for (int i=0; i<anglepar; i++) {
                    std::string a = "a"+to_string<int>(i)+" = ";
                    cout << setw(43) << right << a << setw(9) << cpa[i] << setw(4)
                         << " ±" << setw(9) << cpaerr[i] << endl;
                }
            }
            xx.clear();
            yy.clear();
        }
        if (mpar[Z0]) {
            for (int i=0; i<n; i++) {
                xx.push_back(outr->radii[i]);
                yy.push_back(outr->z0[i]);
            }
            if (n-2>anglepar) {
                where = std::max_element(yy.begin(), yy.end());
                yy.erase(where);
                xx.erase(xx.begin()+std::distance(yy.begin(), where));
                where = std::min_element(yy.begin(), yy.end());
                yy.erase(where);
                xx.erase(xx.begin()+std::distance(yy.begin(), where));
            }
            Lsqfit<T> lsq(&xx[0],1,&yy[0],ww,xx.size(),cz0,cz0err,mp,anglepar,&polyn,&polynd);
            int nrt = lsq.fit();
            if (nrt<0) {
                std::cout << "3DFIT error: cannot least-square fit the thickness.\n";
                in->pars().setTwoStage(false);
                return;
            }
            if (verb) {
                cout << "  Best parameters for thickness:\n";
                for (int i=0; i<anglepar; i++) {
                    std::string a = "a"+to_string<int>(i)+" = ";
                    cout << setw(43) << right << a << setw(9) << cz0[i] << setw(4)
                         << " ±" << setw(9) << cz0err[i] << endl;
                }
            }
            xx.clear();
            yy.clear();
        }

        T xpos_av = findMedian(outr->xpos, n);
        T ypos_av = findMedian(outr->ypos, n);
        T vsys_av = findMedian(outr->vsys, n);
	
        *inr = *outr;
        for (int i=0; i<n; i++) {
            if (mpar[INC]) {
                inr->inc[i]=0;
                for (int j=0; j<anglepar; j++)
                    inr->inc[i] += cinc[j]*std::pow(double(inr->radii[i]),j);
            }
            if (mpar[PA]) {
                inr->phi[i]=0;
                for (int j=0; j<anglepar; j++)
                    inr->phi[i] += cpa[j]*std::pow(double(inr->radii[i]),j);
            }
            if (mpar[Z0]) {
                inr->z0[i]=0;
                for (int j=0; j<anglepar; j++)
                    inr->z0[i] += cz0[j]*std::pow(double(inr->radii[i]),j);
            }
            inr->xpos[i]=xpos_av;
            inr->ypos[i]=ypos_av;
            inr->vsys[i]=vsys_av;
        }
    }

    cout << setprecision(2);
	
	bool oldmpar[MAXPAR];
	for (int i=MAXPAR; i--;) oldmpar[i]=mpar[i];
	int oldnfree =nfree;
	
	mpar[INC] = mpar[PA] = false;
	mpar[XPOS]= mpar[YPOS] = false;
	mpar[VSYS]= mpar[Z0] = false;
	nfree = 2;

	galfit();
	
	for (int i=MAXPAR; i--;) mpar[i]=oldmpar[i];
	nfree = oldnfree;
}
template void Galfit<float>::SecondStage();
template void Galfit<double>::SecondStage();


template <class T> 
void Galfit<T>::createMask() {
		
	if (maskDefined) delete [] mask;
	mask = new short[in->NumPix()];
	
    for (int i=0; i<in->NumPix(); i++) mask[i]=0;

    if (verb) {
        std::cout << " Creating mask for the fit (" << in->pars().getMASK()
                  << ") ..." << std::flush;
        in->pars().setVerbosity(false);
    }

    Statistics::Stats<T> *st = new Statistics::Stats<T>;
    st->setRobust(in->pars().getFlagRobustStats());

    if (in->pars().getMASK()=="SEARCH") {
        if (!in->getIsSearched()) in->Search();
        Detection<T> *larg = in->LargestDetection();
        if (larg==NULL) {
            std::cout << "3DFIT error: No sources detected in the datacube. Cannot fit!!! \n";
            std::terminate();
        }
        std::vector<Voxel<T> > voxlist = larg->getPixelSet();
        typename std::vector<Voxel<T> >::iterator vox;
        for(vox=voxlist.begin();vox<voxlist.end();vox++) {
            mask[in->nPix(vox->getX(),vox->getY(),vox->getZ())]=1;
        }
    }
    else if (in->pars().getMASK()=="THRESHOLD") {
        float thresh = in->pars().getThreshold();
        for (uint i=in->NumPix(); i--;) {
            if (in->Array(i)>thresh) mask[i] = 1;
        }
    }
    else if (in->pars().getMASK()=="SMOOTH") {
        double bmaj = in->Head().Bmaj()*arcconv;
        double bmin = in->Head().Bmin()*arcconv;
        double bpa  = in->Head().Bpa();
        Beam oldbeam = {bmaj,bmin,bpa};      
        Beam newbeam = {in->pars().getFactor()*bmaj,in->pars().getFactor()*bmin,bpa};
        Smooth3D<T> *sm = new Smooth3D<T>;
        sm->smooth(in, oldbeam, newbeam);
        st->calculate(sm->Array(),in->NumPix());
        st->setThresholdSNR(in->pars().getBlankCut());

        for (uint i=in->NumPix(); i--;) {
            if (sm->Array(i)>st->getThreshold()) mask[i] = 1;
        }
        delete sm;
    }
    else if (in->pars().getMASK()=="NONE") {
        for (uint i=in->NumPix(); i--;) {
            if (in->Array(i)>0) mask[i] = 1;
        }
    }

    /*
    else if (in->pars().getNegStats()) {
//        std::ofstream outfile("pippo3.txt");
        for (int z=0; z<in->DimZ(); z++) {
            std::vector<T> onlyneg;
            for (int i=0; i<in->DimX()*in->DimY(); i++)  {
                if (in->Array(i+z*in->DimX()*in->DimY())<0) {
                    onlyneg.push_back(in->Array(i+z*in->DimX()*in->DimY()));
                    onlyneg.push_back(-in->Array(i+z*in->DimX()*in->DimY()));                 }
                }
                st->calculate(&onlyneg[0], onlyneg.size());
                st->setThresholdSNR(in->pars().getBlankCut());

//                outfile << z << "    " << setw(12) << st.getMean() << setw(12) << st.getStddev()
//                        << setw(12) << st.getMedian() << setw(12) << MADFMtosigma << std::endl;

                for (int i=0; i<in->DimX()*in->DimY(); i++)  {
                    if (in->Array(i+z*in->DimX()*in->DimY())>st->getThreshold()) mask[i+z*in->DimX()*in->DimY()]=1;
                    else mask[i+z*in->DimX()*in->DimY()]=0;
                }

                chan_noise[z]=st->getSpread();

            }

    }
    */

    delete st;

	Cube<short> *m = new Cube<short>;
	m->saveHead(in->Head());
 	m->saveParam(in->pars());
 	m->Head().setMinMax(0.,0.);
 	m->setCube(mask, in->AxisDim());
	std::string outname = in->pars().getOutfolder()+"mask.fits";
	m->fitswrite_3d(outname.c_str());
	delete m;
	
    if (verb) {
        std::cout << " Done." << std::endl;
        in->pars().setVerbosity(true);
    }
	maskDefined = true;

}
template void Galfit<float>::createMask();
template void Galfit<double>::createMask();


template <class T> 
bool Galfit<T>::setCfield() {
	
	T pixsizeX = fabs(in->Head().Cdelt(0))*arcconv; 
	T pixsizeY = fabs(in->Head().Cdelt(1))*arcconv; 
	
    //Beam Old = {pixsizeX, pixsizeY, 0};
    Beam Old = {0, 0, 0};

	Beam New = {in->Head().Bmaj()*arcconv,
				in->Head().Bmin()*arcconv,
				in->Head().Bpa()};
/*
	if (Old.bmaj<Old.bmin) {
		std::cout << "Old beam major axis < minor axis. Inverting...";
		std::swap(Old.bmaj, Old.bmin);
	}
*/
	bool agreed = ((New.bmaj>=Old.bmaj) && (New.bmin>=Old.bmin)); 
 
	if (!agreed) {
        std::cout << "3DFIT error: new beam smaller than old beam\n";
        return false;
	}
	if (New.bmaj<New.bmin) {
		std::cout << "New beam major axis < minor axis. Inverting...";
		std::swap(New.bmaj, New.bmin);
	}

	// Now calculate the convolution beam;
	double a2  = Old.bmaj/2;
	double b2  = Old.bmin/2;
	double a0  = New.bmaj/2;
	double b0  = New.bmin/2;
	double D0  = a0*a0-b0*b0;
	double D2  = a2*a2-b2*b2;    
	double th2 = Old.bpa*atan(1.)/45.;
	double th0 = New.bpa*atan(1.)/45.;
	double D1  = sqrt(D0*D0+D2*D2-2*D0*D2*cos(2*(th0-th2)));	
	double a1, b1, th1;
	
	double arg = 0.5*(a0*a0+b0*b0-a2*a2-b2*b2+D1); 
	if (arg<0) {
        std::cout << "3DFIT error: unsuitable new beam parameters!\n";
	return false;
	}
	else a1 = sqrt(arg);
      
	arg = 0.5*(a0*a0+b0*b0-a2*a2-b2*b2-D1); 
	if (arg<0) {
        std::cout << "3DFIT error: unsuitable new beam parameters!\n";
	return false;
	}
	else b1 = sqrt(arg);
	
	double nom   = D0*sin(2*th0)-D2*sin(2*th2);
    double denom = D0*cos(2*th0)-D2*cos(2*th2); 
    if (denom==0 && nom==0) th1=0;
	else {
		T twoth1 = atan2(nom,denom);
		th1 = twoth1/2;          
    }
    
    Beam Con = {2*a1, 2*b1, (th1*45./atan(1.))-90.};


	// Building the convolution field.
	double phi = Con.bpa*atan(1.)/45.;        
	double cs = cos(phi);
	double sn = sin(phi);  
	double beam[2] = {Con.bmaj, Con.bmin};

	double xr = 0.5*beam[0];
	double yr = 0.5*beam[1];
	double extend = sqrt(-1.0*log(1E-04)/log(2.0));
	xr *= extend;
	yr *= extend;
   
	double x1 = fabs(xr*cs-0.0*sn);
	double y1 = fabs(xr*sn+0.0*cs);
	double x2 = fabs(0.0*cs-yr*sn);
	double y2 = fabs(0.0*sn+yr*cs);
	double x  = (x2>x1 ? x2:x1);
	double y  = (y2>y1 ? y2:y1);
   
	int Xmax = lround(x/pixsizeX); 
	int Ymax = lround(y/pixsizeY); 

	NconX = 2*Xmax+1;    
	NconY = 2*Ymax+1; 
	
	cfield = new double[NconX*NconY];
	cfieldAllocated=true;       
      
	double argfac = -4.0 * log(2.0);
	double totalarea = 0;
	for (int j=-Ymax; j<=Ymax; j++) {
		for (int i=-Xmax; i<=Xmax; i++) {
			int pos = (j+Ymax)*NconX+(i+Xmax);    
			x = i*pixsizeX;
			y = j*pixsizeY;
			xr = x*cs + y*sn;
			yr = -1.0*x*sn + y*cs;
			double argX=0;
			double argY=0;
			if (beam[0]!=0) argX = xr/beam[0];
			if (beam[1]!=0) argY = yr/beam[1];
			double arg = argfac*(argX*argX+argY*argY);
			double c = exp(arg);
			if (c>=1E-04) {
				cfield[pos] = c;
				totalarea += c;
			} 
			else cfield[pos] = 0;
		}
	}
   
	for (int i=0;i<NconX*NconY;i++) cfield[i] = cfield[i]/totalarea;

	return true;
}
template bool Galfit<float>::setCfield();
template bool Galfit<double>::setCfield();


template <class T>
Model::Galmod<T>* Galfit<T>::getModel() {

    Model::Galmod<T> *mod = new Model::Galmod<T>;
    int bhi[2] = {in->DimX(), in->DimY()};
    int blo[2] = {0,0};
    int nv = in->pars().getNV();
    if (nv==-1) nv=in->DimZ();
    int ltype = in->pars().getLTYPE();
    int cdens = in->pars().getCDENS();
    mod->input(in,bhi,blo,outr,nv,ltype,1,cdens);
    mod->calculate();
    mod->smooth();
    return mod;
}
template Model::Galmod<float>* Galfit<float>::getModel();
template Model::Galmod<double>* Galfit<double>::getModel();


template <class T>
void Galfit<T>::setFree() {

    std::string FREE = in->pars().getFREE();
    FREE = makelower(FREE);

    int found = FREE.find("vrot");
    if (found<0) mpar[VROT]=false;
    else mpar[VROT]=true;

    found = FREE.find("vdisp");
    if (found<0) mpar[VDISP]=false;
    else mpar[VDISP]=true;

    found = FREE.find("dens");
    if (found<0) mpar[DENS]=false;
    else mpar[DENS]=true;

    found = FREE.find("z0");
    if (found<0) mpar[Z0]=false;
    else mpar[Z0]=true;

    found = FREE.find("inc");
    if (found<0) mpar[INC]=false;
    else mpar[INC]=true;

    found = FREE.find("pa");
    if (found<0) mpar[PA]=false;
    else mpar[PA]=true;

    found = FREE.find("xpos");
    if (found<0) mpar[XPOS]=false;
    else mpar[XPOS]=true;

    found = FREE.find("ypos");
    if (found<0) mpar[YPOS]=false;
    else mpar[YPOS]=true;

    found = FREE.find("vsys");
    if (found<0) mpar[VSYS]=false;
    else mpar[VSYS]=true;

    found = FREE.find("all");
    if (found>=0)
        for (int i=0; i<MAXPAR; i++) mpar[i] = true;

    mpar[DENS] = false;
}
template void Galfit<float>::setFree();
template void Galfit<double>::setFree();


template <class T>
T Galfit<T>::getCenterCoord(std::string pos, std::string type) {

    // Look for the center coordinates in WCS units (deg or sexa) and
    // convert to grid coordinates. If no WCS units are found, it
    // returns the input values. The parameter type is a string that
    // contains either "RA" or "DEC".
    // The input WCS (pos) is expected to be in the following format:
    //      +00:00:00.00     for sexagesimal values
    //      +000.00d         for degrees values

    T pos_grid=0;
    double crval_deg, cdelt_deg, crpix_deg;
    bool isRA = false;
    std::string coord_str;

    type = makelower(type);
    if (type.find("ra")!=-1) {
        crval_deg = in->Head().Crval(0)*degconv(in->Head().Cunit(0));
        cdelt_deg = in->Head().Cdelt(0)*degconv(in->Head().Cunit(0));
        crpix_deg = in->Head().Crpix(0);
        coord_str = in->pars().getXPOS();
        isRA = true;
    }
    else if (type.find("dec")!=-1) {
        crval_deg = in->Head().Crval(1)*degconv(in->Head().Cunit(1));
        cdelt_deg = in->Head().Cdelt(1)*degconv(in->Head().Cunit(1));
        crpix_deg = in->Head().Crpix(1);
        coord_str = in->pars().getYPOS();
    }
    else {
        std::cout << "3DFIT error: unknown coordinate type: " << type
                  << "\n             Cannot convert to grid. \n";
        std::terminate();
    }

    if (coord_str.find('d')!=-1) {                              // Found degrees
        std::string substr = coord_str.erase(coord_str.find('d'),coord_str.size()-1);
        pos_grid = (atof(substr.c_str())-crval_deg)/cdelt_deg+crpix_deg-1;
    }
    else if (coord_str.find(':')!=-1) {                         // Found sexagesimal
        double pos_deg = dmsToDec(coord_str);
        if (isRA) pos_deg*=15;
        pos_grid = (pos_deg-crval_deg)/cdelt_deg+crpix_deg-1;
    }
    else pos_grid  = atof(coord_str.c_str());

    return pos_grid;

}
template float Galfit<float>::getCenterCoord (std::string, std::string);
template double Galfit<double>::getCenterCoord (std::string, std::string);

template <class T>
double* Galfit<T>::getCenterCoordinates(std::string *pos, std::string *type) {

    double *pixels = new double[3];
    double world[3]={0,0,0};
    bool isPOS[3]={false,false,false};
    for (int i=0; i<2; i++) {
        std::string coord_str = pos[i];
        std::string coord_typ = makelower(type[i]);
        if (coord_str.find('d')!=-1) {
            std::string substr = coord_str.erase(coord_str.find('d'),coord_str.size()-1);
            world[i] = atof(substr.c_str());
        }
        else if (coord_str.find(':')!=-1) {                         // Found sexagesimal
            double pos_deg = dmsToDec(coord_str);
            if (coord_typ.find("ra")!=-1) pos_deg*=15;
            world[i] = pos_deg;
        }
        else isPOS[i]=true;
    }

    wcsToPixSingle(in->Head().WCS(),world,pixels);

    if (isPOS[0]) pixels[0]=atof(in->pars().getXPOS().c_str());
    if (isPOS[1]) pixels[1]=atof(in->pars().getYPOS().c_str());
    return pixels;
}
template double* Galfit<float>::getCenterCoordinates (std::string*, std::string*);
template double* Galfit<double>::getCenterCoordinates (std::string*, std::string*);

}



template <class T> T polyn (T *c, T *p, int npar) {
	T value=0;
	for (int i=0; i<npar; i++) value += p[i]*std::pow(double(c[0]),double(i));
	return value;
}
template short polyn(short*,short*,int);
template int polyn(int*,int*,int);
template long polyn(long*,long*,int);
template float polyn(float*,float*,int);
template double polyn(double*,double*,int);

template <class T> void polynd (T *c, T *p, T *d, int npar) {
	for (int i=0; i<npar; i++) d[i]=std::pow(double(c[0]),double(i));
}
template void polynd(short*,short*,short*,int);
template void polynd(int*,int*,int*,int);
template void polynd(long*,long*,long*,int);
template void polynd(float*,float*,float*,int);
template void polynd(double*,double*,double*,int);

template <class T> void fpolyn (T x, T *p, int npar, T &y, T *dydp) {
	
	T value=0;
	for (int i=0; i<npar; i++) {
		value += p[i]*std::pow(double(x),double(i));
		dydp[i]=std::pow(double(x),double(i));
	}
	y = value;
}
template void fpolyn(short,short*,int,short&,short*);
template void fpolyn(int,int*,int,int&,int*);
template void fpolyn(long,long*,int,long&,long*);
template void fpolyn(float,float*,int,float&,float*);
template void fpolyn(double,double*,int,double&,double*);

#undef VROT
#undef VDISP
#undef DENS
#undef Z0
#undef INC
#undef PA
#undef XPOS
#undef YPOS
#undef VSYS
#undef MAXPAR
