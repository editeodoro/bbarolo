// -----------------------------------------------------------------------
// detection.cpp : Member functions for the Detection class.
// -----------------------------------------------------------------------

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
#include <vector>
#include <string>
#include <math.h>
#include "voxel.hh"
#include "object3D.hh"
#include "detection.hh"
#include "../Utilities/utils.hh"
#include "../Arrays/header.hh"


using namespace PixelInfo;

template <class T>
void getIntSpec(Detection<T> &object, float *fluxArray, long *dimArray, std::vector<bool> mask, 
				float beamCorrection, float *spec);

template <class T>
void Detection<T>::defaultDetection() {
	
	xSubOffset = 0;
	ySubOffset = 0;
    zSubOffset = 0;
    haveParams = false;
    haveMass  = false;
    mass    = 0;
    SFR		= 0;
    totalFlux = 0.;
    peakFlux = 0.;
    intFlux = 0.;
    xpeak = 0;
    ypeak = 0;
    zpeak = 0;
    peakSNR = 0.;
    xCentroid = 0.;
    yCentroid = 0.;
    zCentroid = 0.;
    centreType="centroid";
    negSource = false; 
    flagText="";
    id = -1;
    flagWCS=false; 
    raS = "";
    decS = "";
    ra = 0.;
    dec = 0.;
    raWidth = 0.;
    decWidth = 0.;
    majorAxis = 0.;
    minorAxis = 0.;
    posang = 0.;
    specUnits = "";
    fluxUnits = "";
    intFluxUnits = "";
    lngtype = "RA";
    lattype = "DEC";
    vel = 0.;
    velWidth = 0.;
    velMin = 0.;
    velMax = 0.;
    w20 = 0.;
    v20min = 0.;
    v20max = 0.;
    w50 = 0.;
    v50min = 0.;
    v50max = 0.;
    
}


template <class T>
Detection<T>::Detection(): Object3D<T>() {
	
    defaultDetection();
}


template <class T>  
Detection<T>::Detection(const Object3D<T>& o): Object3D<T>(o) {
	
    defaultDetection();
}


template <class T>  
Detection<T>::Detection(const Detection<T>& d): Object3D<T>(d) {
	
    operator=(d);
}


template <class T>  
Detection<T>& Detection<T>::operator= (const Detection<T>& d) {
    
    ((Object3D<T> &) *this) = d;
    this->xSubOffset   = d.xSubOffset;
    this->ySubOffset   = d.ySubOffset;
    this->zSubOffset   = d.zSubOffset;
    this->haveParams   = d.haveParams;
    this->totalFlux    = d.totalFlux;
    this->intFlux      = d.intFlux;
    this->mass		   = d.mass;
    this->SFR		   = d.SFR;
    this->haveMass	   = d.haveMass;
    this->peakFlux     = d.peakFlux;
    this->xpeak        = d.xpeak;
    this->ypeak        = d.ypeak;
    this->zpeak        = d.zpeak;
    this->peakSNR      = d.peakSNR;
    this->xCentroid    = d.xCentroid;
    this->yCentroid    = d.yCentroid;
    this->zCentroid    = d.zCentroid;
    this->centreType   = d.centreType;
    this->negSource    = d.negSource;
    this->flagText     = d.flagText;
    this->id           = d.id;
    this->flagWCS      = d.flagWCS;
    this->raS          = d.raS;
    this->decS         = d.decS;
    this->ra           = d.ra;
    this->dec	       = d.dec;
    this->raWidth      = d.raWidth;
    this->decWidth     = d.decWidth;
    this->majorAxis    = d.majorAxis;
    this->minorAxis    = d.minorAxis;
    this->posang       = d.posang;
    this->specUnits    = d.specUnits;
    this->fluxUnits    = d.fluxUnits;
    this->intFluxUnits = d.intFluxUnits;
    this->lngtype      = d.lngtype;
    this->lattype      = d.lattype;
    this->vel          = d.vel;
    this->velWidth     = d.velWidth;
    this->velMin       = d.velMin;
    this->velMax       = d.velMax;
    this->w20          = d.w20;
    this->v20min       = d.v20min;
    this->v20max       = d.v20max;
    this->w50          = d.w50;
    this->v50min       = d.v50min;
    this->v50max       = d.v50max;
    this->posPrec      = d.posPrec;
    this->xyzPrec      = d.xyzPrec;
    this->fintPrec     = d.fintPrec;
    this->fpeakPrec    = d.fpeakPrec;
    this->velPrec      = d.velPrec;
    this->snrPrec      = d.snrPrec;
    return *this;
}


template <class T>
Detection<T> operator+ (Detection<T> &lhs, Detection<T> &rhs) {
	
    Detection<T> output = lhs;
    for(typename std::map<long, Object2D<T> >::iterator it = rhs.chanlist.begin(); it!=rhs.chanlist.end();it++)
		output.addChannel(it->first, it->second);
    output.haveParams = false; 
    return output;
}


//========================================================


template <class T>  
float Detection<T>::getXcentre() {
	
    if(centreType=="peak") return xpeak;
    else if(centreType=="average") return this->getXaverage();
    else return xCentroid;
}


template <class T>  
float Detection<T>::getYcentre() {
	
    if(centreType=="peak") return ypeak;
    else if(centreType=="average") return this->getYaverage();
    else return yCentroid;
}


template <class T>
float Detection<T>::getZcentre() {
	
    if(centreType=="peak") return zpeak;
    else if(centreType=="average") return this->getZaverage();
    else return zCentroid;
}


template <class T>
void Detection<T>::setOffsets(long Xoffset, long Yoffset, long Zoffset) {

    /// This function stores the values of the offsets for each cube axis.
    /// The offsets are the starting values of the cube axes that may differ from
    /// the default value of 0 (for instance, if a subsection is being used).
    /// The values will be used when the detection is outputted.

    xSubOffset = Xoffset;
    ySubOffset = Yoffset;
    zSubOffset = Zoffset;
  }


template <class T>
void Detection<T>::addOffsets() {
      
    Object3D<T>::addOffsets(xSubOffset,ySubOffset,zSubOffset);
    xpeak+=xSubOffset; ypeak+=ySubOffset; zpeak+=zSubOffset;
    xCentroid+=xSubOffset; yCentroid+=ySubOffset; zCentroid+=zSubOffset;
}


template <class T>
void Detection<T>::addPixel(PixelInfo::Voxel<T> point) {
		
	Object3D<T>::addPixel(point.getX(),point.getY(),point.getZ());
	totalFlux += point.getF();
	if(point.getF()>peakFlux){
		peakFlux = point.getF();
		xpeak = point.getX(); ypeak = point.getY(); zpeak = point.getZ();
	}
}


//=================================================================


template <class T>
void Detection<T>::addDetection(Detection<T> &other) {
    
    for(typename std::map<long, Object2D<T> >::iterator it = other.chanlist.begin(); it!=other.chanlist.end();it++)
		this->addChannel(it->first, it->second);
    haveParams = false;
}


template <class T>
bool Detection<T>::hasEnoughChannels(int minNumber) {
	
  /// A function to determine if the Detection has enough contiguous channels 
  /// to meet the minimum requirement given as the argument.
  ///
  /// \param minNumber 		How many channels is the minimum acceptable number?
  /// \return 				True if there is at least one occurence of minNumber 
  ///						consecutive channels present to return true.

    // Preferred method -- need a set of minNumber consecutive channels present.

    int numChan = this->getMaxAdjacentChannels();
    bool result = (numChan >= minNumber);

    return result;
  
}


template <class T>
bool Detection<T>::canMerge(Detection<T> &other, Param &par) {
	
    bool near = isNear(other, par);
    if(near) return isClose(other, par);
    else return near;
}


template <class T>
bool Detection<T>::isNear(Detection<T> &other, Param &par) {

    bool flagAdj = par.getFlagAdjacent();
	float threshS = par.getThreshS();
    float threshV = par.getThreshV();
    
    long gap;
    if(flagAdj) gap = 1;
    else 
    gap = long(ceil(threshS));

    bool areNear;
    // Test X ranges
    if((this->xmin-gap)<other.xmin) areNear=((this->xmax+gap)>=other.xmin);
    else areNear=(other.xmax>=(this->xmin-gap));
    // Test Y ranges
    if(areNear){
		if((this->ymin-gap)<other.ymin) areNear=areNear&&((this->ymax+gap)>=other.ymin);
		else areNear=areNear&&(other.ymax>=(this->ymin-gap));
    }
    // Test Z ranges
    if(areNear){
		gap = long(ceil(threshV));
		if((this->zmin-gap)<other.zmin) areNear=areNear&&((this->zmax+gap)>=other.zmin);
		else areNear=areNear&&(other.zmax>=(this->zmin-gap));
    }
    
    return areNear;

}


template <class T>
bool Detection<T>::isClose(Detection<T> &other, Param &par)  {
   
    bool close = false;   
    
    bool flagAdj = par.getFlagAdjacent();
	float threshS = par.getThreshS();
    float threshV = par.getThreshV();
    // 
    // If we get to here, the pixel ranges overlap -- so we do a
    // pixel-by-pixel comparison to make sure they are actually
    // "close" according to the thresholds.  Otherwise, close=false,
    // and so don't need to do anything else before returning.
    // 

    std::vector<long> zlist1 = this->getChannelList();
    std::vector<long> zlist2 = other.getChannelList();
    Scan<T> test1,test2;

    for(size_t ct1=0; (!close && (ct1<zlist1.size())); ct1++){
		for(size_t ct2=0; (!close && (ct2<zlist2.size())); ct2++){
			if(abs(zlist1[ct1]-zlist2[ct2])<=threshV){
	      
				Object2D<T> temp1 = this->getChanMap(zlist1[ct1]);
				Object2D<T> temp2 = other.getChanMap(zlist2[ct2]);
				close = temp1.canMerge(temp2,threshS,flagAdj);

			}
		}
	}
       
    return close;
    
}


template <class T>
bool Detection<T>::voxelListsMatch(std::vector<Voxel<T> > voxelList) {
    
  ///  A test to see whether there is a 1-1 correspondence between
  ///  the given list of Voxels and the voxel positions contained in
  ///  this Detection's pixel list. No testing of the fluxes of the
  ///  Voxels is done.
  /// 
  ///  \param voxelList 	The std::vector list of Voxels to be tested.

    bool listsMatch = true;
    listsMatch = listsMatch && (voxelList.size() == this->getSize());
    if(!listsMatch) return listsMatch;
    listsMatch = listsMatch && voxelListCovered(voxelList);
    typename std::vector<Voxel<T> >::iterator vox;
    for(vox=voxelList.begin();vox<voxelList.end();vox++)
		listsMatch = listsMatch && isInObject(*vox);
    return listsMatch;

}


template <class T>
bool Detection<T>::voxelListCovered(std::vector<Voxel<T> > voxelList) {

  ///  A test to see whether the given list of Voxels contains each
  ///  position in this Detection's pixel list. It does not look for
  ///  a 1-1 correspondence: the given list can be a super-set of the
  ///  Detection. No testing of the fluxes of the Voxels is done.
  /// 
  ///  \param voxelList 	The std::vector list of Voxels to be tested.

    bool listsMatch = true;
    size_t v1=0;
    std::vector<Voxel<T> > detpixlist = this->getPixelSet();
    while(listsMatch && v1<detpixlist.size()){
		bool inList = false;
		size_t v2=0;
		while(!inList && v2<voxelList.size()){
			inList = inList || detpixlist[v1].match(voxelList[v2]);
			v2++;
		}
		listsMatch = listsMatch && inList;
		v1++;
    }

    return listsMatch;

}
  

template <class T>
void Detection<T>::calcFluxes(std::vector<Voxel<T> > voxelList) {
    
  ///  A function that calculates total & peak fluxes (and 
  ///  the location  of the peak flux) for a Detection.
  /// 
  ///  \param voxelList 	The list of Voxel to calculate
  ///   					the flux parameters from.

    totalFlux = peakFlux = 0;
    xCentroid = yCentroid = zCentroid = 0.;
/*
    if(!voxelListCovered(voxelList)){
      std::cout<< "Detection::calcFluxes: Voxel list provided does not match."<<std::endl;
      return;
    }
*/
	
    typename std::vector<Voxel<T> >::iterator vox;
    
    for(vox=voxelList.begin();vox<voxelList.end();vox++) {
		if(this->isInObject(*vox)){
			long x = vox->getX();
			long y = vox->getY();
			long z = vox->getZ();
			float f = vox->getF();
			totalFlux += f;
			xCentroid += x*f;
			yCentroid += y*f;
			zCentroid += z*f;
			if( (vox==voxelList.begin()) ||
				(negSource&&(f<peakFlux)) || 
				(!negSource&&(f>peakFlux)) ) {
				
				peakFlux = f;
				xpeak =    x;
				ypeak =    y;
				zpeak =    z;
			}
		}
    }
	
    xCentroid /= totalFlux;
    yCentroid /= totalFlux;
    zCentroid /= totalFlux;
    
	haveParams = true;

}


template <class T>
void Detection<T>::calcFluxes(T *fluxArray, long *dim) {
    
  /// A function that calculates total & peak fluxes (and the
  /// location of the peak flux) for a Detection.
  /// 
  /// \param fluxArray 	The array of flux values to 
  /// 					calculate the flux parameters from.
  /// \param dim 		The dimensions of the flux array.

    totalFlux = peakFlux = 0;
    xCentroid = yCentroid = zCentroid = 0.;

    std::vector<Voxel<T> > voxList = this->getPixelSet();
    typename std::vector<Voxel<T> >::iterator vox=voxList.begin();
    for(;vox<voxList.end();vox++) {
		long x=vox->getX();
		long y=vox->getY();
		long z=vox->getZ();
		long ind = vox->arrayIndex(dim);
		float f = fluxArray[ind];
		totalFlux += f;
		xCentroid += x*f;
		yCentroid += y*f;
		zCentroid += z*f;
		if( (vox==voxList.begin()) ||
			(negSource&&(f<peakFlux)) || 
			(!negSource&&(f>peakFlux)) ) {
	  
			peakFlux = f;
			xpeak = x;
			ypeak = y;
			zpeak = z;
		}
 
    }

    xCentroid /= totalFlux;
    yCentroid /= totalFlux;
    zCentroid /= totalFlux;
    
    haveParams = true;

}


template <class T>
void Detection<T>::calcWCSparams(Header &head) {
    ///  @details
    ///  Use the input wcs to calculate the position and velocity 
    ///    information for the Detection.
    ///  Quantities calculated:
    ///  <ul><li> RA: ra [deg], ra (string), ra width.
    ///      <li> Dec: dec [deg], dec (string), dec width.
    ///      <li> Vel: vel [km/s], min & max vel, vel width.
    ///      <li> coord type for all three axes, nuRest, 
    ///      <li> name (IAU-style, in equatorial or Galactic) 
    ///  </ul>
    /// 
    ///  Note that the regular parameters are NOT recalculated!
    /// 
    ///  \param head FitsHeader object that contains the WCS information.

 
	haveParams = true;

	std::string cunit = head.Cunit(0);	
	double arcsconv, degconv;
	if (cunit=="DEGREE" || cunit=="DEGREES" || cunit=="DEG" ||
		cunit=="degree" || cunit=="degrees" || cunit=="deg") {
		degconv  = 1;
		arcsconv = 3600;
	}
	else if (cunit=="ARCSEC" || cunit=="ARCS" ||
			 cunit=="arcsec" || cunit=="arcs") {
		degconv  = 1/3600.;
		arcsconv = 1;
	}
	else if (cunit=="ARCMIN" || cunit=="ARCM" ||
			 cunit=="arcmin" || cunit=="arcm") {
		degconv  = 1/60.;
		arcsconv = 60;
	}
	else {
		std::cout << "Error (unknown CUNIT for RA-DEC): ";
		std::cout << "cannot convert to ARCSEC.\n";
		std::cout << cunit;
        std::terminate();
	}
	
    lngtype = head.Ctype(0);
	lattype = head.Ctype(1);
	specUnits = head.Ctype(2);
	fluxUnits = head.Bunit();
	intFluxUnits = "JY KM/S";
	ra 	 = (getXcentre()+1-head.Crpix(0))*head.Cdelt(0)+head.Crval(0);
	ra 	*= degconv;
	dec  = (getYcentre()+1-head.Crpix(1))*head.Cdelt(1)+head.Crval(1);
	dec *= degconv;
	raS  = decToDMS(ra, lngtype);
	decS = decToDMS(dec,lattype);
	float raMin = (this->getXmin()+1-head.Crpix(0))*head.Cdelt(0)+head.Crval(0);
	float raMax = (this->getXmax()+1-head.Crpix(0))*head.Cdelt(0)+head.Crval(0);
	float deMin = (this->getYmin()+1-head.Crpix(1))*head.Cdelt(1)+head.Crval(1);
	float deMax = (this->getYmax()+1-head.Crpix(1))*head.Cdelt(1)+head.Crval(1);
	raMin *= degconv;
	raMax *= degconv;
	deMin *= degconv;
	deMax *= degconv;
	raWidth   = angularSeparation(raMin,dec,raMax,dec)*arcsconv;
	decWidth  = angularSeparation(ra,deMin,ra,deMax)*arcsconv;

	Object2D<T> spatMap = this->getSpatialMap();
	std::pair<double,double> axes = spatMap.getPrincipleAxes();
	float PixScale = ((fabs(head.Cdelt(0))+fabs(head.Cdelt(1)))/2.)*arcsconv;
	majorAxis = std::max(axes.first,axes.second)*PixScale;
	minorAxis = std::min(axes.first,axes.second)*PixScale;
	posang = spatMap.getPositionAngle()*180./M_PI;

	vel    = (this->getZcentre()+1-head.Crpix(2))*head.Cdelt(2)+head.Crval(2);
    vel	   = AlltoVel(vel, head);
	velMin = (this->getZmin()+1-head.Crpix(2))*head.Cdelt(2)+head.Crval(2);
    velMin = AlltoVel(velMin, head);
	velMax = (this->getZmax()+1-head.Crpix(2))*head.Cdelt(2)+head.Crval(2);
    velMax = AlltoVel(velMax, head);
	velWidth = fabs(velMax-velMin);

	flagWCS = true;
    

}
  //--------------------------------------------------------------------

template <class T>
void Detection<T>::calcIntegFlux(long zdim, std::vector<Voxel<T> > voxelList, Header &head) {
   
 	///  @details
    ///  Uses the input WCS to calculate the velocity-integrated flux, 
    ///   putting velocity in units of km/s.
    ///  The fluxes used are taken from the Voxels, rather than an
    ///   array of flux values.
    ///  Integrates over full spatial and velocity range as given 
    ///   by the extrema calculated by calcWCSparams.
    /// 
    ///  If the flux units end in "/beam" (eg. Jy/beam), then the flux is
    ///  corrected by the beam size (in pixels). This is done by
    ///  multiplying the integrated flux by the number of spatial pixels,
    ///  and dividing by the beam size in pixels (e.g. Jy/beam * pix /
    ///  pix/beam --> Jy)
    /// 
    ///  \param zdim The size of the spectral axis (needed to find the velocity widths)
    ///  \param voxelList The list of Voxels with flux information
    ///  \param head FitsHeader object that contains the WCS information.

    const int border = 1;
	/*
    if(!voxelListCovered(voxelList)){
     	std::cout << "Voxel list provided does not match";
      	return;
    }
	*/
   
	haveParams = true;

    // include one pixel either side in each direction
    long xsize = (this->getXmax()-this->getXmin()+border*2+1);
    long ysize = (this->getYmax()-this->getYmin()+border*2+1);
    long zsize = (this->getZmax()-this->getZmin()+border*2+1); 
    long size = xsize*ysize*zsize;
    std::vector<bool> isObj(size,false);
    double *localFlux = new double[size];
    for(int i=0;i<size;i++) localFlux[i]=0.;

    typename std::vector<Voxel<T> >::iterator vox;
    for(vox=voxelList.begin();vox<voxelList.end();vox++){
		//if(isInObject(*vox)){
	  		long x = vox->getX();
	  		long y = vox->getY();
	  		long z = vox->getZ();
	  		long pos = (x-this->getXmin()+border)+(y-this->getYmin()+border)*xsize+(z-this->getZmin()+border)*xsize*ysize;
	  		//localFlux[pos] = vox->getF();
			localFlux[pos] = Pbcor(*vox,head);
	  		isObj[pos] = true;
		//}
     }
  
    // work out the WCS coords for each pixel
    double *world  = new double[zsize];
    for(int i=0;i<zsize;i++){
		int zpt = lround(this->getZmin()-border+i);
		world[i] = (zpt+1-head.Crpix(2))*head.Cdelt(2)+head.Crval(2);
        world[i] = AlltoVel(world[i], head);
    }

    double integrated = 0.;
    for(int pix=0; pix<xsize*ysize; pix++){ 
		for(int z=0; z<zsize; z++){
			int pos = z*xsize*ysize+pix;
			if(isObj[pos]){ 				
	   			double deltaVel;
	   			if(z==0) deltaVel = (world[z+1]-world[z]);
	   			else if(z==(zsize-1)) deltaVel = (world[z]-world[z-1]);
	   			else deltaVel = (world[z+1]-world[z-1])/2.;
	   			integrated += localFlux[pos]*fabs(deltaVel);
			}
		}
    }
      	
	intFlux = integrated;

    delete [] world;
    delete [] localFlux;

    calcVelWidths(zdim,voxelList,head);

    // correct for the beam size and convert to Jy
   	intFlux = FluxtoJy(intFlux, head);
    

}
  //--------------------------------------------------------------------

template <class T>
void Detection<T>::calcIntegFlux(T *fluxArray, long *dim, Header &head) {
    
	///  @details
    ///  Uses the input WCS to calculate the velocity-integrated flux, 
    ///   putting velocity in units of km/s.
    ///  Integrates over full spatial and velocity range as given 
    ///   by the extrema calculated by calcWCSparams.
    /// 
    ///  If the flux units end in "/beam" (eg. Jy/beam), then the flux is
    ///  corrected by the beam size (in pixels). This is done by
    ///  multiplying the integrated flux by the number of spatial pixels,
    ///  and dividing by the beam size in pixels (e.g. Jy/beam * pix /
    ///  pix/beam --> Jy)
    /// 
    ///  \param fluxArray The array of flux values.
    ///  \param dim The dimensions of the flux array.
    ///  \param head FitsHeader object that contains the WCS information.

	haveParams = true;

    // include one pixel either side in each direction
    long xsize = (this->xmax-this->xmin+3);
    long ysize = (this->ymax-this->ymin+3);
    long zsize = (this->zmax-this->zmin+3); 
    long size = xsize*ysize*zsize;
    std::vector <bool> isObj(size,false);
    double *localFlux = new double[size];
    for(int i=0;i<size;i++) localFlux[i]=0.;
      	
	// work out which pixels are object pixels
    std::vector<Voxel<T> > voxlist = this->getPixelSet();
    for(typename std::vector<Voxel<T> >::iterator v=voxlist.begin();v<voxlist.end();v++){
		long pos=(v->getX()-this->xmin+1)+(v->getY()-this->ymin+1)*xsize+(v->getZ()-this->zmin+1)*xsize*ysize;
		Voxel<T> vox(v->getX(), v->getY(), v->getZ(), fluxArray[v->arrayIndex(dim)]);
		localFlux[pos] = Pbcor(vox, head);
		//localFlux[pos] = fluxArray[v->arrayIndex(dim)];
		isObj[pos] = true;
    }

    // work out the WCS coords for each pixel
    double *world  = new double[zsize];
    for(int z=0;z<zsize;z++){
	    int zpt = lround(this->zmin-1+z);
		world[z] = (zpt+1-head.Crpix(2))*head.Cdelt(2)+head.Crval(2);
        world[z] = AlltoVel(world[z], head);
	}
	
	double integrated = 0.;
    for(int pix=0; pix<xsize*ysize; pix++){ // loop over each spatial pixel.
		for(int z=0; z<zsize; z++){
	  		int pos = z*xsize*ysize+pix;
	  		if(isObj[pos]){ // if it's an object pixel...
	    		double deltaVel;
	    		if(z==0) deltaVel = (world[z+1]-world[z]);
	   			else if(z==(zsize-1)) deltaVel = (world[z]-world[z-1]);
	   			else deltaVel = (world[z+1]-world[z-1])/2.;
	    		integrated += localFlux[pos]*fabs(deltaVel);
	  		}
		}
	}
    intFlux = integrated;

    delete [] world;
    delete [] localFlux;
	
    calcVelWidths(fluxArray, dim, head);

    // correct for the beam size and convert to Jy
   	intFlux = FluxtoJy(intFlux, head);
	
}
  //--------------------------------------------------------------------

template <class T>
void Detection<T>::calcVelWidths(long zdim, std::vector<Voxel<T> > voxelList, Header &head) {
    ///  @details
    /// Calculates the widths of the detection at 20% and 50% of the
    /// peak integrated flux. The procedure is as follows: first
    /// generate an integrated flux spectrum (using all given voxels
    /// that lie in the object's spatial map); find the peak; starting
    /// at the spectral edges of the detection, move in or out until
    /// you reach the 20% or 50% peak flux level. Linear interpolation
    /// between points is done.
    /// 
    ///  \param zdim The size of the spectral axis in the cube
    ///  \param voxelList The list of Voxels with flux information
    ///  \param head FitsHeader object that contains the WCS information.

    T *intSpec = new T[zdim];
    for(int i=0;i<zdim;i++) intSpec[i]=0;
     
    //Object2D spatMap = getSpatialMap();
    //for(int s=0;s<spatMap.getNumScan();s++){
    	typename std::vector<Voxel<T> >::iterator vox;
      	for(vox=voxelList.begin();vox<voxelList.end();vox++){
			//if(spatMap.isInObject(*vox)){
				double flux = Pbcor(*vox,head);
	  			intSpec[vox->getZ()] += flux;
			//}
      	}
    //}
    
    calcVelWidths(zdim, intSpec, head);

    delete [] intSpec;

}

  //--------------------------------------------------------------------

template <class T>
void Detection<T>::calcVelWidths(long zdim, T *intSpec, Header &head) {

      // finding the 20% & 50% points.  Start at the velmin & velmax
      //  points. Then, if the int flux there is above the 20%/50%
      //  limit, go out, otherwise go in. This is to deal with the
      //  problems from double- (or multi-) peaked sources.

    haveParams = true;

    int z=this->getZmin();
    double zpt;
    bool goLeft;
    
    float peak=0.;
    int peakLoc=0;
    for(int z=this->getZmin();z<=this->getZmax();z++) {
      	if(z==0 || peak<intSpec[z]){
			peak = intSpec[z];
			peakLoc = z;
      	}
    }
    
    goLeft = intSpec[z]>peak*0.5;
    if(goLeft) while(z>0 && intSpec[z]>peak*0.5) z--;
    else       while(z<peakLoc && intSpec[z]<peak*0.5) z++;
    if(z==0) v50min = velMin;
    else {
      	if (goLeft) zpt = z+(peak*0.5-intSpec[z])/(intSpec[z+1]-intSpec[z]);
      	else        zpt = z-(peak*0.5-intSpec[z])/(intSpec[z-1]-intSpec[z]);
      	v50min = (zpt+1-head.Crpix(2))*head.Cdelt(2)+head.Crval(2);
        v50min = AlltoVel(v50min, head);
    }

    z=this->getZmax();
    goLeft = intSpec[z]<peak*0.5;
    if(goLeft) while(z>peakLoc && intSpec[z]<peak*0.5) z--;
    else       while(z<zdim    && intSpec[z]>peak*0.5) z++;
    if(z==zdim) v50max = velMax;
    else{
      	if(goLeft) zpt = z+(peak*0.5-intSpec[z])/(intSpec[z+1]-intSpec[z]);
      	else       zpt = z-(peak*0.5-intSpec[z])/(intSpec[z-1]-intSpec[z]);
      	v50max = (zpt+1-head.Crpix(2))*head.Cdelt(2)+head.Crval(2);
        v50max = AlltoVel(v50max, head);
    }

    z=this->getZmin();
    goLeft = intSpec[z]>peak*0.2;
    if(goLeft) while(z>0 && intSpec[z]>peak*0.2) z--;
    else       while(z<peakLoc && intSpec[z]<peak*0.2) z++;
    if(z==0) v20min = velMin;
    else{
      	if(goLeft) zpt = z+(peak*0.2-intSpec[z])/(intSpec[z+1]-intSpec[z]);
      	else       zpt = z-(peak*0.2-intSpec[z])/(intSpec[z-1]-intSpec[z]);
      	v20min = (zpt+1-head.Crpix(2))*head.Cdelt(2)+head.Crval(2);
        v20min = AlltoVel(v20min, head);
    }

    z=this->getZmax();
    goLeft = intSpec[z]<peak*0.2;
    if(goLeft) while(z>peakLoc && intSpec[z]<peak*0.2) z--;
    else       while(z<zdim    && intSpec[z]>peak*0.2) z++;
    if(z==zdim) v20max = velMax;
    else{
      	if(goLeft) zpt = z+(peak*0.2-intSpec[z])/(intSpec[z+1]-intSpec[z]);
      	else       zpt = z-(peak*0.2-intSpec[z])/(intSpec[z-1]-intSpec[z]);
      	v20max = (zpt+1-head.Crpix(2))*head.Cdelt(2)+head.Crval(2);
        v20max = AlltoVel(v20max, head);
    }

    w20 = fabs(v20min-v20max);
    w50 = fabs(v50min-v50max);
    float Vsys_20 = (v20max+v20min)/2.;
	float Vsys_50 = (v50max+v50min)/2.;
	vsys = (Vsys_20+Vsys_50)/2.;


}
  //--------------------------------------------------------------------

template <class T>
void Detection<T>::calcVelWidths(T *fluxArray, long *dim, Header &head) {
    ///  @details
    /// Calculates the widths of the detection at 20% and 50% of the
    /// peak integrated flux. The procedure is as follows: first
    /// generate an integrated flux spectrum (summing each spatial
    /// pixel's spectrum); find the peak; starting at the spectral
    /// edges of the detection, move in or out until you reach the 20%
    /// or 50% peak flux level. Linear interpolation between points is
    /// done. 
    /// 
    ///  \param fluxArray The array of flux values.
    ///  \param dim The dimensions of the flux array.
    ///  \param head FitsHeader object that contains the WCS information.

    if(dim[2]>2){

		float *intSpec = new float[dim[2]];
		long size=dim[0]*dim[1]*dim[2];
		std::vector<bool> mask(size,true); 
		getIntSpec(*this,fluxArray,dim,mask,1.,intSpec);
	
		calcVelWidths(dim[2],intSpec,head);

		delete [] intSpec;

    }
    else{
		v50min = v20min = velMin;
		v50max = v20max = velMax;
		w20 = fabs(v20min-v20max);
		w50 = fabs(v50min-v50max);
		float Vsys_20 = (v20max+v20min)/2.;
		float Vsys_50 = (v50max+v50min)/2.;
		vsys = (Vsys_20+Vsys_50)/2.;
    }



}
  //--------------------------------------------------------------------


  //--------------------------------------------------------------------

template <class T>
std::vector<int> Detection<T>::getVertexSet() {

  /// Gets a list of points being the end-points of 1-pixel long
  /// segments drawing a border around the spatial extend of a
  /// detection. The vector is a series of 4 integers, being: x_0,
  /// y_0, x_1, y_1.
  ///
  /// \return The vector of vertex positions.

    std::vector<int> vertexSet;

    int xmin = this->getXmin() - 1;
    int xmax = this->getXmax() + 1;
    int ymin = this->getYmin() - 1;
    int ymax = this->getYmax() + 1;
    int xsize = xmax - xmin + 1;
    int ysize = ymax - ymin + 1;

    std::vector<Voxel<T> > voxlist = this->getPixelSet();
    std::vector<bool> isObj(xsize*ysize,false);
    typename std::vector<Voxel<T> >::iterator vox;
    for(vox=voxlist.begin();vox<voxlist.end();vox++){
		int pos = (vox->getX()-xmin)+(vox->getY()-ymin)*xsize;
		isObj[pos] = true;
    }
    voxlist.clear();
    
    for(int x=xmin; x<=xmax; x++){
   		for(int y=ymin+1;y<=ymax;y++){
			int current  = (y-ymin)*xsize + x-xmin;
			int previous = (y-ymin-1)*xsize + x-xmin;
			if((isObj[current]&&!isObj[previous])||(!isObj[current]&&isObj[previous])){
				vertexSet.push_back(x);
				vertexSet.push_back(y);
				vertexSet.push_back(x+1);
				vertexSet.push_back(y);
			}
		}
    }
    
    for(int y=ymin; y<=ymax; y++){
		for(int x=xmin+1;x<=xmax;x++){
			int current  = (y-ymin)*xsize + x-xmin;
			int previous = (y-ymin)*xsize + x-xmin - 1;
			if((isObj[current]&&!isObj[previous])||(!isObj[current]&&isObj[previous])){
				vertexSet.push_back(x);
				vertexSet.push_back(y);
				vertexSet.push_back(x);
				vertexSet.push_back(y+1);
			}
		}
    }

    return vertexSet;
  
}


template <class T> 
void getIntSpec(Detection<T> &object, float *fluxArray, long *dimArray, std::vector<bool> mask, 
				float beamCorrection, float *spec) {
					
    /// @details
    ///  The base function that extracts an integrated spectrum for a
    ///  given object from a pixel array. The spectrum is returned as
    ///  the integrated flux, corrected for the beam using the given
    ///  correction factor.
    ///   \param object The Detection in question
    ///   \param fluxArray The full array of pixel values.
    ///   \param dimArray The axis dimensions for the fluxArray
    ///   \param mask A mask array indicating whether given pixels are valid
    ///   \param beamCorrection How much to divide the summed spectrum
    ///   by to return the integrated flux.
    ///   \param spec The integrated spectrum for the object -- must be allocated first.

    for(int i=0;i<dimArray[2];i++) spec[i] = 0.;
    long xySize = dimArray[0]*dimArray[1];
    bool *done = new bool[xySize]; 
    for(int i=0;i<xySize;i++) done[i]=false;
    std::vector<Voxel<float> > voxlist = object.getPixelSet();
    std::vector<Voxel<float> >::iterator vox;
    for(vox=voxlist.begin();vox<voxlist.end();vox++){
		long pos = vox->getX()+dimArray[0]*vox->getY();
		if(!done[pos]){
			done[pos] = true;
			for(int z=0;z<dimArray[2];z++){
				if(mask[pos+z*xySize]){
					spec[z] += fluxArray[pos + z*xySize] / beamCorrection;
				}	    
			}
		}
    }
    delete [] done;

}




