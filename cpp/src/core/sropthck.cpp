/************************************************************************//**
 * File: sropthck.cpp
 * Description: Optical element: "Thick" Mirror
 * Project: Synchrotron Radiation Workshop
 * First release: October 2012
 *
 * Copyright (C) Brookhaven National Laboratory, Upton, NY, USA
 * Portions Copyright (C) European XFEL, Hamburg, Germany
 * All Rights Reserved
 *
 * @author O.Chubar
 * @version 1.0
 ***************************************************************************/

#include "sropthck.h"
#include "sroptdrf.h"
#include "gminterp.h"

//*************************************************************************

srTMirror::srTMirror(srTStringVect* pMirInf, srTDataMD* pExtraData) 
{
	if((pMirInf == 0) || (pMirInf->size() < 30)) { ErrorCode = IMPROPER_OPTICAL_COMPONENT_STRUCTURE; return;}
	if(pExtraData != 0) m_reflData = *pExtraData;

	const char* mirID = (*pMirInf)[1];

	m_halfDim1 = 0.5*atof((*pMirInf)[10]); //dimensions
	m_halfDim2 = 0.5*atof((*pMirInf)[11]);

	m_apertShape = 1; //1- rectangular, 2- elliptical 
	int iShape = atoi((*pMirInf)[12]);
	if((iShape > 0) && (iShape < 3)) m_apertShape = (char)iShape; //keep updated!

	m_vCenNorm.x = atof((*pMirInf)[16]); //central normal in the frame of incident beam
	m_vCenNorm.y = atof((*pMirInf)[17]);
	m_vCenNorm.z = atof((*pMirInf)[18]);
	if(m_vCenNorm.z == 0) { ErrorCode = IMPROPER_OPTICAL_COMPONENT_ORIENT; return;}
	m_vCenNorm.Normalize();

	m_vCenTang.x = atof((*pMirInf)[19]);
	m_vCenTang.y = atof((*pMirInf)[20]);
	if((m_vCenTang.x == 0) && (m_vCenTang.y == 0)) { ErrorCode = IMPROPER_OPTICAL_COMPONENT_ORIENT; return;}

	m_vCenTang.z = (-m_vCenNorm.x*m_vCenTang.x - m_vCenNorm.y*m_vCenTang.y)/m_vCenNorm.z;
	m_vCenTang.Normalize();

	TransvCenPoint.x = atof((*pMirInf)[23]);
	TransvCenPoint.y = atof((*pMirInf)[24]);

	m_propMeth = (char)atoi((*pMirInf)[26]);
	if((m_propMeth < 1) || (m_propMeth > 1)) //to keep updated
	{ ErrorCode = IMPROPER_OPTICAL_COMPONENT_SIM_METH; return;}

	//to program:
	int npT = atoi((*pMirInf)[28]); //number of points for representing the element in Tangential direction (for "thin" approx., etc.)
	int npS = atoi((*pMirInf)[29]); //number of points for representing the element in Sagital direction (for "thin" approx., etc.)

	//m_numPartsProp = (char)atoi((*pMirInf)[23]);
	//if(m_numPartsProp < 1) { ErrorCode = SRWL_INCORRECT_PARAM_FOR_WFR_PROP; return;}

	SetupNativeTransFromLocToBeamFrame(m_vCenNorm, m_vCenTang, TransvCenPoint);
	//FindElemExtentsAlongOptAxes(*(TransHndl.rep), m_vCenNorm, m_halfDim1, m_halfDim2, m_extAlongOptAxIn, m_extAlongOptAxOut); //virtual

	m_pRadAux = 0;
}

//*************************************************************************

srTMirror::srTMirror(const SRWLOptMir& srwlMir) 
{
	m_halfDim1 = 0.5*srwlMir.dt; //dimensions: tangential
	m_halfDim2 = 0.5*srwlMir.ds; //dimensions: sagital

	m_apertShape = 1; //1- rectangular, 2- elliptical 
	if(srwlMir.apShape == 'e') m_apertShape = 2;

	m_propMeth = srwlMir.meth;
	if((m_propMeth < 1) || (m_propMeth > 2)) //to keep updated
	{ ErrorCode = IMPROPER_OPTICAL_COMPONENT_SIM_METH; return;}

	m_npt = srwlMir.npt;
	m_nps = srwlMir.nps;

	m_treatInOut = srwlMir.treatInOut;
	//m_treatOut = srwlMir.treatOut;
	m_extAlongOptAxIn = srwlMir.extIn;
	m_extAlongOptAxOut = srwlMir.extOut;

	m_reflData.pData = (char*)srwlMir.arRefl;
	m_reflData.DataType[0] = 'c';
	m_reflData.DataType[1] = 'd'; //?
	m_reflData.AmOfDims = 3;
	m_reflData.DimSizes[0] = srwlMir.reflNumPhEn;
	m_reflData.DimSizes[1] = srwlMir.reflNumAng;
	m_reflData.DimSizes[2] = srwlMir.reflNumComp;
	m_reflData.DimStartValues[0] = 
	m_reflData.DimStartValues[1] = srwlMir.reflAngStart;
	m_reflData.DimStartValues[2] = 1;

	if(strcmp(srwlMir.reflPhEnScaleType, "lin\0") == 0)
	{
		strcpy(m_reflData.DimScales[0], "lin\0");
		m_reflData.DimSteps[0] = srwlMir.reflPhEnFin - srwlMir.reflPhEnStart;
	}
	else if(strcmp(srwlMir.reflPhEnScaleType, "log\0") == 0)
	{
		strcpy(m_reflData.DimScales[0], "log\0");
		m_reflData.DimSteps[0] = log10(srwlMir.reflPhEnFin) - log10(srwlMir.reflPhEnStart);
	}
	if(srwlMir.reflNumPhEn > 1) m_reflData.DimSteps[0] /= (srwlMir.reflNumPhEn - 1);
	if(strcmp(srwlMir.reflAngScaleType, "lin\0") == 0)
	{
		strcpy(m_reflData.DimScales[1], "lin\0");
		m_reflData.DimSteps[1] = srwlMir.reflAngFin - srwlMir.reflAngStart;
	}
	else if(strcmp(srwlMir.reflAngScaleType, "log\0") == 0)
	{
		strcpy(m_reflData.DimScales[1], "log\0");
		m_reflData.DimSteps[1] = log10(srwlMir.reflAngFin) - log10(srwlMir.reflAngStart);
	}
	if(srwlMir.reflNumAng > 1) m_reflData.DimSteps[1] /= (srwlMir.reflNumAng - 1);

	strcpy(m_reflData.DimUnits[0], "eV");
	strcpy(m_reflData.DimUnits[1], "rad");
	m_reflData.DimUnits[2][0] = '\0';
	m_reflData.DataUnits[0] = '\0';
	m_reflData.DataName[0] = '\0';
	m_reflData.hState = 1;

	m_vCenNorm.x = srwlMir.nvx; //central normal in the frame of incident beam
	m_vCenNorm.y = srwlMir.nvy;
	m_vCenNorm.z = srwlMir.nvz;
	if(m_vCenNorm.z == 0) { ErrorCode = IMPROPER_OPTICAL_COMPONENT_ORIENT; return;}
	m_vCenNorm.Normalize();

	m_vCenTang.x = srwlMir.tvx;
	m_vCenTang.y = srwlMir.tvy;
	if((m_vCenTang.x == 0) && (m_vCenTang.y == 0)) { ErrorCode = IMPROPER_OPTICAL_COMPONENT_ORIENT; return;}
	m_vCenTang.z = (-m_vCenNorm.x*m_vCenTang.x - m_vCenNorm.y*m_vCenTang.y)/m_vCenNorm.z;
	m_vCenTang.Normalize();

	TransvCenPoint.x = srwlMir.x;
	TransvCenPoint.y = srwlMir.y;

	//This only calculates the transformation to the local framee
	SetupNativeTransFromLocToBeamFrame(m_vCenNorm, m_vCenTang, TransvCenPoint);
	//Other calculations (transformation of base vectors, finding extents of optical elements along optical axes, etc., will happen just before propagation)
	//FindElemExtentsAlongOptAxes(*(TransHndl.rep), m_vCenNorm, m_halfDim1, m_halfDim2, m_extAlongOptAxIn, m_extAlongOptAxOut); //virtual

	m_pRadAux = 0;
	m_wfrRadWasProp = false;
}

//*************************************************************************

srTMirror* srTMirror::DefineMirror(srTStringVect* pMirInf, srTDataMD* pExtraData)
{
	//if((pMirInf == 0) || (pMirInf->size() < 24)) { ErrorCode = IMPROPER_OPTICAL_COMPONENT_STRUCTURE; return 0;}
	if((pMirInf == 0) || (pMirInf->size() < 3)) return 0;
	
	//const char* mirID = (*pMirInf)[2];
	const char* mirID = (*pMirInf)[1];
	if(strcmp(mirID, "Toroid") == 0) return new srTMirrorToroid(pMirInf, pExtraData);
	//else if(strcmp(mirID, "Paraboloid") == 0) return new srTMirrorToroid(pMirInf, pExtraData);
	else return 0;
}

//*************************************************************************

void srTMirror::SetupNativeTransFromLocToBeamFrame(TVector3d& vCenNorm, TVector3d& vCenTang, TVector2d& vCenP2d)
{//In the Local frame, tangential direction is X, sagital Y
	TVector3d mRow1(vCenTang.x, vCenNorm.y*vCenTang.z - vCenNorm.z*vCenTang.y, vCenNorm.x);
	TVector3d mRow2(vCenTang.y, vCenNorm.z*vCenTang.x - vCenNorm.x*vCenTang.z, vCenNorm.y);
	TVector3d mRow3(vCenTang.z, vCenNorm.x*vCenTang.y - vCenNorm.y*vCenTang.x, vCenNorm.z);
	TMatrix3d M(mRow1, mRow2, mRow3);
	TVector3d vCen(vCenP2d.x, vCenP2d.y, 0);

	gmTrans *pTrans = new gmTrans(M, vCen);
	TransHndl = srTransHndl(pTrans);

/**
	TVector3d vUz(0, 0, 1), vUx(1, 0, 0), vUy(0, 1, 0);
	m_vInLoc = pTrans->TrBiPoint_inv(vUz); //direction of input optical axis in the local frame of opt. elem.

	m_vOutLoc = m_vInLoc - (2.*(m_vInLoc*vUz))*vUz; //direction of output optical axis in the local frame of opt. elem.
	//To modify the above: find m_vOutLocintersection with surface


	//Defining the basis vectors of the output beam frame.
	//The Beam frame should stay "right-handed" even after the reflection;
	//therefore the new basis vectors should be obtained by rotation from the previous basis vectors.
	//The rotation should be around the axis perpendicular to the plane of incidence (i.e. plane of reflection)
	//and the rotation angle is the angle between the incident and the reflected central beams (i.e. optical axes before and aftre the reflection).

	const double relTolZeroVect = 1.e-10;
	const double relTolZeroVectE2 = relTolZeroVect*relTolZeroVect;

	//double absCenNormTrE2 = vCenNorm.x*vCenNorm.x + vCenNorm.y*vCenNorm.y;
	//double absCenNormE2 = absCenNormTrE2 + vCenNorm.z*vCenNorm.z;
	//if(absCenNormTrE2 < absCenNormE2*relTolZeroVectE2)

	TVector3d vDifOutIn = m_vOutLoc - m_vInLoc;
	double absDifE2 = vDifOutIn.AmpE2();
	//Special case: central normal parallel to the input (and output) optical axis
	if(absDifE2 < relTolZeroVectE2)
	{
		m_vHorOutIn.x = -1.; m_vHorOutIn.y = m_vHorOutIn.z = 0.; //= -vUx;
		m_vVerOutIn.y = 1.; m_vHorOutIn.x = m_vHorOutIn.z = 0.; //= vUy;
	}

	//General case: rotation about this axis:

	TVector3d vZero(0,0,0), vRotAxis(-vCenNorm.y, vCenNorm.x, 0.); //= vCenNorm^vUz
	double rotAng = acos(m_vInLoc*m_vOutLoc);

	gmTrans auxRot;
	auxRot.SetupRotation(vZero, vRotAxis, rotAng);
	m_vHorOutIn = auxRot.TrBiPoint(vUx); //output horizontal vector in the frame of input beam
	m_vVerOutIn = auxRot.TrBiPoint(vUy); //output vertical vector in the frame of input beam

	//m_vHorOutIn = vUx - (2.*(vUx*vCenNorm))*vCenNorm; //output horizontal vector in the frame of input beam
	//m_vVerOutIn = vUy - (2.*(vUy*vCenNorm))*vCenNorm; 
**/
}

//*************************************************************************

int srTMirror::FindBasisVectorTransAndExents()
{//Setting up auxiliary vectors before the propagation: m_vInLoc, m_vOutLoc, m_vHorOutIn, m_vVerOutIn;
 //To be called after the native transformation to the local frame has been already set up!

	gmTrans *pTrans = TransHndl.rep;

	TVector3d vUz(0, 0, 1), vUx(1, 0, 0), vUy(0, 1, 0), vZero(0, 0, 0);
	//It is assumed that the optical axis in the frame of input beam is defined by point {0,0,0} and vector {0,0,1}
	m_vInLoc = pTrans->TrBiPoint_inv(vUz); //direction of input optical axis in the Local frame of opt. elem.
	TVector3d vCenPtLoc = pTrans->TrPoint_inv(vZero); //point through which the iput optical axis passes in the local frame
	TVector3d vIntesPtLoc, vNormAtIntersPtLoc;
	if(!FindRayIntersectWithSurfInLocFrame(vCenPtLoc, m_vInLoc, vIntesPtLoc, &vNormAtIntersPtLoc)) return FAILED_DETERMINE_OPTICAL_AXIS;

	m_vInLoc.Normalize();
	m_vOutLoc = m_vInLoc - ((2.*(m_vInLoc*vNormAtIntersPtLoc))*vNormAtIntersPtLoc); //direction of output optical axis in the local frame of opt. elem.
	m_vOutLoc.Normalize();

	TVector3d vNormAtIntersPtTestIn = pTrans->TrBiPoint(vNormAtIntersPtLoc);

	//Defining coordinates of basis vectors of the output beam frame in the input beam frame (m_vHorOutIn, m_vVerOutIn).
	//The Beam frame should stay "right-handed" even after the reflection;
	//therefore the new basis vectors should be obtained by rotation from the previous basis vectors.
	//The rotation should be around the axis perpendicular to the plane of incidence (i.e. plane of reflection)
	//and the rotation angle is the angle between the incident and the reflected central beams (i.e. optical axes before and aftre the reflection).

	//Checking for Special case: central normal parallel to the input (and output) optical axis
	const double relTolZeroVect = 1.e-10;
	double absRotAng = acos(m_vInLoc*m_vOutLoc);
	if(absRotAng < relTolZeroVect)
	{//Special case: central normal parallel to the input (and output) optical axis
		m_vHorOutIn.x = -1.; m_vHorOutIn.y = m_vHorOutIn.z = 0.; //= -vUx;
		m_vVerOutIn.y = 1.; m_vHorOutIn.x = m_vHorOutIn.z = 0.; //= vUy;
	}

	//General case: rotation about vRotAxis:
	TVector3d vRotAxis = m_vInLoc^m_vOutLoc; //rotation axis in the Local opt. elem. frame
	vRotAxis = pTrans->TrBiPoint(vRotAxis); //now in the frame of input beam

	gmTrans auxRot;
	auxRot.SetupRotation(vZero, vRotAxis, absRotAng);
	m_vHorOutIn = auxRot.TrBiPoint(vUx); //output horizontal vector in the frame of input beam
	m_vVerOutIn = auxRot.TrBiPoint(vUy); //output vertical vector in the frame of input beam

	if((m_extAlongOptAxIn == 0.) && (m_extAlongOptAxOut == 0.))
	{
		//Calculate "extents": m_extAlongOptAxIn, m_extAlongOptAxOut, using other member variables (which are assumed to be already defined)
		//Mirror cormers in local frame:
		TVector3d r1(-m_halfDim1, -m_halfDim2, 0), r2(m_halfDim1, -m_halfDim2, 0), r3(-m_halfDim1, m_halfDim2, 0), r4(m_halfDim1, m_halfDim2, 0); 
		//r1 = pTrans->TrBiPoint(r1);
		//r2 = pTrans->TrBiPoint(r2);
		//r3 = pTrans->TrBiPoint(r3);
		//r4 = pTrans->TrBiPoint(r4);
		//Mirror cormers in Input beam frame:
		r1 = pTrans->TrPoint(r1);
		r2 = pTrans->TrPoint(r2);
		r3 = pTrans->TrPoint(r3);
		r4 = pTrans->TrPoint(r4);
		//Longitudinal positions of Mirror cormers in the Input beam frame:
		double arLongCoordIn[] = {r1.z, r2.z, r3.z, r4.z};

		TVector3d m_vLongOutIn = m_vHorOutIn^m_vVerOutIn;
		double arLongCoordOut[] = {r1*m_vLongOutIn, r2*m_vLongOutIn, r3*m_vLongOutIn, r4*m_vLongOutIn};

		double *t_arLongCoordIn = arLongCoordIn, *t_arLongCoordOut = arLongCoordOut;
		double extMin = *(t_arLongCoordIn++), extMax = *(t_arLongCoordOut++);
		for(int i=0; i<3; i++)
		{
			if(extMin > *t_arLongCoordIn) extMin = *t_arLongCoordIn;
			if(extMax < *t_arLongCoordOut) extMax = *t_arLongCoordOut;
			t_arLongCoordIn++; t_arLongCoordOut++;
		}

		m_extAlongOptAxIn = ::fabs(extMin);
		m_extAlongOptAxOut = extMax;
	}

	return 0;
}

//*************************************************************************

void srTMirror::FindElemExtentsAlongOptAxes(gmTrans& trfMir, TVector3d& vCenNorm, double halfDim1, double halfDim2, double& extIn, double& extOut)
{
	TVector3d r1(-halfDim1, -halfDim2, 0), r2(halfDim1, -halfDim2, 0), r3(-halfDim1, halfDim2, 0), r4(halfDim1, halfDim2, 0);
	//TVector3d r1(-halfDim1 + TransvCenPoint.x, -halfDim2 + TransvCenPoint.y, 0);
	//TVector3d r2(halfDim1 + TransvCenPoint.x, -halfDim2 + TransvCenPoint.y, 0);
	//TVector3d r3(-halfDim1 + TransvCenPoint.x, halfDim2 + TransvCenPoint.y, 0);
	//TVEctor3d r4(halfDim1 + TransvCenPoint.x, halfDim2 + TransvCenPoint.y, 0);

	gmTrans *pTrans = TransHndl.rep;
	//r1 = pTrans->TrBiPoint(r1);
	//r2 = pTrans->TrBiPoint(r2);
	//r3 = pTrans->TrBiPoint(r3);
	//r4 = pTrans->TrBiPoint(r4);
	r1 = pTrans->TrPoint(r1);
	r2 = pTrans->TrPoint(r2);
	r3 = pTrans->TrPoint(r3);
	r4 = pTrans->TrPoint(r4);
	double arLongCoordIn[] = {r1.z, r2.z, r3.z, r4.z};

	TVector3d vOptAxis(0, 0, 1);
	vOptAxis -= (2.*(vOptAxis*vCenNorm))*vCenNorm;
	double arLongCoordOut[] = {r1*vOptAxis, r2*vOptAxis, r3*vOptAxis, r4*vOptAxis};

	double *t_arLongCoordIn = arLongCoordIn, *t_arLongCoordOut = arLongCoordOut;
	double extMin = *(t_arLongCoordIn++), extMax = *(t_arLongCoordOut++);
	for(int i=0; i<3; i++)
	{
		if(extMin > *t_arLongCoordIn) extMin = *t_arLongCoordIn;
		if(extMax < *t_arLongCoordOut) extMax = *t_arLongCoordOut;
		t_arLongCoordIn++; t_arLongCoordOut++;
	}

	extIn = ::fabs(extMin);
	extOut = extMax;
}

//*************************************************************************

int srTMirror::WfrInterpolOnOrigGrid(srTSRWRadStructAccessData* pWfr, float* arRayTrCoord, float* arEX, float* arEZ, float xMin, float xMax, float zMin, float zMax)
{
	if((pWfr == 0) || (arRayTrCoord == 0) || ((arEX == 0) && (arEZ == 0))) return FAILED_INTERPOL_ELEC_FLD;
	//if((pWfr == 0) || (arRayTrCoord == 0) || (arOptPathDif == 0) || ((arEX == 0) && (arEZ == 0))) return FAILED_INTERPOL_ELEC_FLD;

	bool isCoordRepres = (pWfr->Pres == 0);
	bool isFreqRepres = (pWfr->PresT == 0);
	bool waveFrontTermWasTreated = false;
	//test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//if(false)
	if(isFreqRepres && isCoordRepres && WaveFrontTermCanBeTreated(*pWfr))
	{
		float *pPrevBaseRadX = pWfr->pBaseRadX; 
		float *pPrevBaseRadZ = pWfr->pBaseRadZ;
		pWfr->pBaseRadX = arEX;
		pWfr->pBaseRadZ = arEZ;
		
		//test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//TreatStronglyOscillatingTerm(*pWfr, 'r');
		//end test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		TreatStronglyOscillatingTermIrregMesh(*pWfr, arRayTrCoord, xMin, xMax, zMin, zMax, 'r');
		
		pWfr->pBaseRadX = pPrevBaseRadX;
		pWfr->pBaseRadZ = pPrevBaseRadZ;
		waveFrontTermWasTreated = true;
	}

	float *t_ExRes = pWfr->pBaseRadX;
	float *t_EzRes = pWfr->pBaseRadZ;

	long PerX = (pWfr->ne) << 1;
	long PerZ = PerX*(pWfr->nx);
	long nTot = PerZ*(pWfr->nz);
	long nTot_mi_1 = nTot - 1;
	long nx_mi_1 = pWfr->nx - 1;
	long nz_mi_1 = pWfr->nz - 1;
	double f0m1, fm10, f00, f10, f01, f11, a10, a01, a11, a20, a02;

	long ix0=-1, iz0=-1;
	double phEn, x, z = pWfr->zStart;

	const double dMax = 1.E+20;
	double dx, dz, dTest, dTest0, dTestPrev;

	for(long iz=0; iz<pWfr->nz; iz++)
	{
		x = pWfr->xStart;
		for(long ix=0; ix<pWfr->nx; ix++)
		{
			bool pointIsInsideNonZeroSquare = (x >= xMin) && (x <= xMax) && (z >= zMin) && (z <= zMax);

			phEn = pWfr->eStart;
			for(long ie=0; ie<pWfr->ne; ie++)
			{
				long two_ie = 2*ie;

				if(pointIsInsideNonZeroSquare)
				{//perform interpolation on irregular mesh (bilinear or based on 12 points)
					//find indexes of the relevant point for the interpolation

					if(ix0 < 0) ix0 = ix;
					if(iz0 < 0) iz0 = iz;

					bool pointFound = false, candPointFound = false;
					long ix0pr = -1, iz0pr = -1;

					while((ix0 != ix0pr) && (iz0 != iz0pr))
					{
						ix0pr = ix0; iz0pr = iz0;

						long iz0_PerZ_p_2_ie = iz0*PerZ + two_ie;

						dTestPrev = 1.E+23;
						long ofst = ix0*PerX + iz0_PerZ_p_2_ie;
						if(ofst < nTot_mi_1)
						{
							dx = x - arRayTrCoord[ofst];
							dz = z - arRayTrCoord[ofst + 1];
							dTestPrev = sqrt(dx*dx + dz*dz);
						}
						dTest0 = dTestPrev;

						pointFound = false;
						candPointFound = false;
						for(int iix=ix0-1; iix>=0; iix--)
						{
							ofst = iix*PerX + iz0_PerZ_p_2_ie;
							if(ofst < nTot_mi_1)
							{
								dx = x - arRayTrCoord[ofst];
								dz = z - arRayTrCoord[ofst + 1];
								dTest = sqrt(dx*dx + dz*dz);
								if((dTest > dMax) && (!candPointFound)) continue;
								if(dTest < dTestPrev)  
								{
									ix0 = iix; dTestPrev = dTest; candPointFound = true;
								}
								else
								{
									if(candPointFound) pointFound = true;
									break;
								}
							}
						}
						if(!pointFound)
						{
							//dTestPrev = 1.E+23;
							dTestPrev = dTest0;
							candPointFound = false;
							for(int iix=ix0+1; iix<pWfr->nx; iix++)
							{
								ofst = iix*PerX + iz0_PerZ_p_2_ie;
								if(ofst < nTot_mi_1)
								{
									dx = x - arRayTrCoord[ofst];
									dz = z - arRayTrCoord[ofst + 1];
									dTest = sqrt(dx*dx + dz*dz);
									if((dTest > dMax) && (!candPointFound)) continue;
									if(dTest < dTestPrev)  
									{
										ix0 = iix; dTestPrev = dTest; candPointFound = true;
									}
									else
									{
										//if(candPointFound) pointFound = true;
										break;
									}
								}
							}
						}

						long ix0_PerX_p_2_ie = ix0*PerX + two_ie;
						
						dTestPrev = 1.E+23;
						ofst = iz0*PerZ + ix0_PerX_p_2_ie;
						if(ofst < nTot_mi_1)
						{
							dx = x - arRayTrCoord[ofst];
							dz = z - arRayTrCoord[ofst + 1];
							dTestPrev = sqrt(dx*dx + dz*dz);
						}
						dTest0 = dTestPrev;

						pointFound = false;
						candPointFound = false;
						for(int iiz=iz0-1; iiz>=0; iiz--)
						{
							ofst = iiz*PerZ + ix0_PerX_p_2_ie;
							if(ofst < nTot_mi_1)
							{
								dx = x - arRayTrCoord[ofst];
								dz = z - arRayTrCoord[ofst + 1];
								dTest = sqrt(dx*dx + dz*dz);
								if((dTest > dMax) && (!candPointFound)) continue;
								if(dTest < dTestPrev) 
								{
									iz0 = iiz; dTestPrev = dTest; candPointFound = true;
								}
								else 
								{
									if(candPointFound) pointFound = true;
									break;
								}
							}
						}
						if(!pointFound)
						{
							//dTestPrev = 1.E+23;
							dTestPrev = dTest0;
							candPointFound = false;
							for(int iiz=iz0+1; iiz<pWfr->nz; iiz++)
							{
								ofst = iiz*PerZ + ix0_PerX_p_2_ie;
								if(ofst < nTot_mi_1)
								{
									dx = x - arRayTrCoord[ofst];
									dz = z - arRayTrCoord[ofst + 1];
									dTest = sqrt(dx*dx + dz*dz);
									if((dTest > dMax) && (!candPointFound)) continue;
									if(dTest < dTestPrev) 
									{
										iz0 = iiz; dTestPrev = dTest; candPointFound = true;
									}
									else 
									{
										//if(candPointFound) pointFound = true;
										break;
									}
								}
							}
						}
					}
					//calculate indexes of other points and interpolate 
					//2 cases are considered: "bi-linear" 2D interpolation based on 4 points and "bi-quadratic" 2D interpolation based on 5 points (mesh can be irregular)
					const double relTolEqualStep = 1.e-04; //to tune
					
					if(ix0 < 0) ix0 = 0;
					else if(ix0 >= nx_mi_1) ix0 = nx_mi_1 - 1;
					if(iz0 < 0) iz0 = 0;
					else if(iz0 >= nz_mi_1) iz0 = nz_mi_1 - 1;

					long ofst_00 = ix0*PerX + iz0*PerZ + two_ie;
					long ofst_m10 = ofst_00 - PerX;
					long ofst_10 = ofst_00 + PerX;
					long ofst_0m1 = ofst_00 - PerZ;
					long ofst_01 = ofst_00 + PerZ;

					if(ix0 == 0) ofst_m10 = ofst_00;
					if(ix0 == nx_mi_1) ofst_10 = ofst_00;
					if(iz0 == 0) ofst_0m1 = ofst_00;
					if(iz0 == nz_mi_1) ofst_10 = ofst_00;

					long ofst_00_p_1 = ofst_00 + 1;
					long ofst_m10_p_1 = ofst_m10 + 1;
					long ofst_10_p_1 = ofst_10 + 1;
					long ofst_0m1_p_1 = ofst_0m1 + 1;
					long ofst_01_p_1 = ofst_01 + 1;

					double x_00 = arRayTrCoord[ofst_00], z_00 = arRayTrCoord[ofst_00_p_1];
					double x_m10 = arRayTrCoord[ofst_m10], z_m10 = arRayTrCoord[ofst_m10_p_1];
					double x_10 = arRayTrCoord[ofst_10], z_10 = arRayTrCoord[ofst_10_p_1];
					double x_0m1 = arRayTrCoord[ofst_0m1], z_0m1 = arRayTrCoord[ofst_0m1_p_1];
					double x_01 = arRayTrCoord[ofst_01], z_01 = arRayTrCoord[ofst_01_p_1];

					double rx_m10 = x_m10 - x_00, rz_m10 = z_m10 - z_00;
					double rx_10 = x_10 - x_00, rz_10 = z_10 - z_00;
					double rx_0m1 = x_0m1 - x_00, rz_0m1 = z_0m1 - z_00;
					double rx_01 = x_01 - x_00, rz_01 = z_01 - z_00;
					double dx_00 = x - x_00, dz_00 = z - z_00;

					bool rx_m10_isNotOK = ((fabs(rx_m10) > dMax) || (rx_m10 == 0));
					bool rx_10_isNotOK = ((fabs(rx_10) > dMax) || (rx_10 == 0));
					if(rx_m10_isNotOK && rx_10_isNotOK) goto SetFieldToZero;
					
					bool rz_0m1_isNotOK = ((fabs(rz_0m1) > dMax) || (rz_0m1 == 0));
					bool rz_01_isNotOK = ((fabs(rz_01) > dMax) || (rz_01 == 0));
					if(rz_0m1_isNotOK && rz_01_isNotOK) goto SetFieldToZero;

					if(rx_m10_isNotOK) rx_m10 = -rx_10;
					else if(rx_10_isNotOK) rx_10 = -rx_m10;

					if(fabs(rx_0m1) > dMax) rx_0m1 = 0.;
					if(fabs(rx_01) > dMax) rx_01 = 0.;

					if(rz_0m1_isNotOK) rz_0m1 = -rz_01;
					else if(rz_01_isNotOK) rz_01 = -rz_0m1;

					if(fabs(rz_m10) > dMax) rz_m10 = 0.;
					if(fabs(rz_10) > dMax) rz_10 = 0.;

					if(m_wfrInterpMode == 1)
					{//bi-linear, based on 4 points
						double sp_m10 = rx_m10*dx_00 + rz_m10*dz_00;
						double sp_10 = rx_10*dx_00 + rz_10*dz_00;
						double sp_0m1 = rx_0m1*dx_00 + rz_0m1*dz_00;
						double sp_01 = rx_01*dx_00 + rz_01*dz_00;

						bool initPointMoved = false;
						if((sp_m10 > 0) && (sp_10 < 0))
						{
							if(ix0 > 0) { ix0--; initPointMoved = true;}
						}
						if((sp_0m1 > 0) && (sp_01 < 0))
						{
							if(iz0 > 0) { iz0--; initPointMoved = true;}
						}

						long ofst0 = initPointMoved? (ix0*PerX + iz0*PerZ + two_ie) : ofst_00;

						long ofst0_p_PerX = ofst0 + PerX;
						long ofst0_p_PerZ = ofst0 + PerZ;
						long ofst0_p_PerX_p_PerZ = ofst0_p_PerZ + PerX;
						double x00 = arRayTrCoord[ofst0], x10 = arRayTrCoord[ofst0_p_PerX], x01 = arRayTrCoord[ofst0_p_PerZ], x11 = arRayTrCoord[ofst0_p_PerX_p_PerZ];
						long ofst0_p_1 = ofst0 + 1;
						long ofst0_p_PerX_p_1 = ofst0_p_PerX + 1;
						long ofst0_p_PerZ_p_1 = ofst0_p_PerZ + 1;
						long ofst0_p_PerX_p_PerZ_p_1 = ofst0_p_PerX_p_PerZ + 1;
						double z00 = arRayTrCoord[ofst0_p_1], z10 = arRayTrCoord[ofst0_p_PerX_p_1], z01 = arRayTrCoord[ofst0_p_PerZ_p_1], z11 = arRayTrCoord[ofst0_p_PerX_p_PerZ_p_1];

						double rX = x - x00, rZ = z - z00;
						double rX10 = x10 - x00, rZ10 = z10 - z00;
						double rX01 = x01 - x00, rZ01 = z01 - z00;
						double rX11 = x11 - x00, rZ11 = z11 - z00;

						double absTolX = relTolEqualStep*fabs(rX10);
						double absTolZ = relTolEqualStep*fabs(rZ01);
						bool isRecX = (fabs(rX01) < absTolX) && (fabs(x11 - x10) < absTolX);
						bool isRecZ = (fabs(rZ10) < absTolZ) && (fabs(z11 - z01) < absTolZ);
						if(isRecX && isRecZ)
						{//regular rectangular mesh
							double xt = rX/rX10, zt = rZ/rZ01;
							if(arEX != 0)
							{
								f00 = arEX[ofst0]; f10 = arEX[ofst0_p_PerX]; f01 = arEX[ofst0_p_PerZ]; f11 = arEX[ofst0_p_PerX_p_PerZ];
								a10 = f10 - f00; a01 = f01 - f00; a11 = f00 - f01 - f10 + f11;
								*t_ExRes = (float)(xt*(a10 + a11*zt) + a01*zt + f00);

								f00 = arEX[ofst0_p_1]; f10 = arEX[ofst0_p_PerX_p_1]; f01 = arEX[ofst0_p_PerZ_p_1]; f11 = arEX[ofst0_p_PerX_p_PerZ_p_1];
								a10 = f10 - f00; a01 = f01 - f00; a11 = f00 - f01 - f10 + f11;
								*(t_ExRes + 1) = (float)(xt*(a10 + a11*zt) + a01*zt + f00);
							}
							if(arEZ != 0)
							{
								f00 = arEZ[ofst0]; f10 = arEZ[ofst0_p_PerX]; f01 = arEZ[ofst0_p_PerZ]; f11 = arEZ[ofst0_p_PerX_p_PerZ];
								a10 = f10 - f00; a01 = f01 - f00; a11 = f00 - f01 - f10 + f11;
								*t_EzRes = (float)(xt*(a10 + a11*zt) + a01*zt + f00);

								f00 = arEZ[ofst0_p_1]; f10 = arEZ[ofst0_p_PerX_p_1]; f01 = arEZ[ofst0_p_PerZ_p_1]; f11 = arEZ[ofst0_p_PerX_p_PerZ_p_1];
								a10 = f10 - f00; a01 = f01 - f00; a11 = f00 - f01 - f10 + f11;
								*(t_EzRes + 1) = (float)(xt*(a10 + a11*zt) + a01*zt + f00);
							}
						}
						else
						{//irregular mesh (general case)
							double arXZ[] = {rX10, rZ10, rX01, rZ01, rX11, rZ11};
							if(arEX != 0)
							{
								double arER[] = {arEX[ofst0], arEX[ofst0_p_PerX], arEX[ofst0_p_PerZ], arEX[ofst0_p_PerX_p_PerZ]};
								*t_ExRes = (float)CGenMathInterp::Interp2dBiLinVar(rX, rZ, arXZ, arER);
								double arEI[] = {arEX[ofst0_p_1], arEX[ofst0_p_PerX_p_1], arEX[ofst0_p_PerZ_p_1], arEX[ofst0_p_PerX_p_PerZ_p_1]};
								*(t_ExRes + 1) = (float)CGenMathInterp::Interp2dBiLinVar(rX, rZ, arXZ, arEI);
							}
							if(arEZ != 0)
							{
								double arER[] = {arEZ[ofst0], arEZ[ofst0_p_PerX], arEZ[ofst0_p_PerZ], arEZ[ofst0_p_PerX_p_PerZ]};
								*t_EzRes = (float)CGenMathInterp::Interp2dBiLinVar(rX, rZ, arXZ, arER);
								double arEI[] = {arEZ[ofst0_p_1], arEZ[ofst0_p_PerX_p_1], arEZ[ofst0_p_PerZ_p_1], arEZ[ofst0_p_PerX_p_PerZ_p_1]};
								*(t_EzRes + 1) = (float)CGenMathInterp::Interp2dBiLinVar(rX, rZ, arXZ, arEI);
							}
						}
					}
					else if(m_wfrInterpMode == 2)
					{//bi-quadratic, based on 5 points
						double absTolX = relTolEqualStep*fabs(rx_10);
						double absTolZ = relTolEqualStep*fabs(rz_01);
						bool isRecX = (fabs(rx_0m1) < absTolX) && (fabs(rx_01) < absTolX);
						bool isRecZ = (fabs(rz_m10) < absTolZ) && (fabs(rz_10) < absTolX);
						if(isRecX && isRecZ)
						{
							bool isEquidistX = (fabs(rx_m10 + rx_10) < absTolX);
							bool isEquidistZ = (fabs(rz_0m1 + rz_01) < absTolZ);
							if(isEquidistX && isEquidistZ)
							{//regular rectangular mesh
								double xt = dx_00/rx_10, zt = dz_00/rz_01;
								if(arEX != 0)
								{
									f0m1 = arEX[ofst_0m1]; fm10 = arEX[ofst_m10]; f00 = arEX[ofst_00]; f10 = arEX[ofst_10]; f01 = arEX[ofst_01];
									a10 = 0.5*(f10 - fm10); a01 = 0.5*(f01 - f0m1); a20 = 0.5*(f10 + fm10) - f00; a02 = 0.5*(f01 + f0m1) - f00;
									*t_ExRes = (float)(xt*(xt*a20 + a10) + zt*(zt*a02 + a01) + f00);

									f0m1 = arEX[ofst_0m1_p_1]; fm10 = arEX[ofst_m10_p_1]; f00 = arEX[ofst_00_p_1]; f10 = arEX[ofst_10_p_1]; f01 = arEX[ofst_01_p_1];
									a10 = 0.5*(f10 - fm10); a01 = 0.5*(f01 - f0m1); a20 = 0.5*(f10 + fm10) - f00; a02 = 0.5*(f01 + f0m1) - f00;
									*(t_ExRes + 1) = (float)(xt*(xt*a20 + a10) + zt*(zt*a02 + a01) + f00);
								}
								if(arEZ != 0)
								{
									f0m1 = arEZ[ofst_0m1]; fm10 = arEZ[ofst_m10]; f00 = arEZ[ofst_00]; f10 = arEZ[ofst_10]; f01 = arEZ[ofst_01];
									a10 = 0.5*(f10 - fm10); a01 = 0.5*(f01 - f0m1); a20 = 0.5*(f10 + fm10) - f00; a02 = 0.5*(f01 + f0m1) - f00;
									*t_EzRes = (float)(xt*(xt*a20 + a10) + zt*(zt*a02 + a01) + f00);

									f0m1 = arEZ[ofst_0m1_p_1]; fm10 = arEZ[ofst_m10_p_1]; f00 = arEZ[ofst_00_p_1]; f10 = arEZ[ofst_10_p_1]; f01 = arEZ[ofst_01_p_1];
									a10 = 0.5*(f10 - fm10); a01 = 0.5*(f01 - f0m1); a20 = 0.5*(f10 + fm10) - f00; a02 = 0.5*(f01 + f0m1) - f00;
									*(t_EzRes + 1) = (float)(xt*(xt*a20 + a10) + zt*(zt*a02 + a01) + f00);
								}
							}
							else
							{//variable-step rectangular mesh
								double DX = 1./((rx_10 - rx_m10)*rx_10*rx_m10), DZ = 1./((rz_01 - rz_0m1)*rz_01*rz_0m1);
								if(arEX != 0)
								{
									f0m1 = arEX[ofst_0m1]; fm10 = arEX[ofst_m10]; f00 = arEX[ofst_00]; f10 = arEX[ofst_10]; f01 = arEX[ofst_01];
									double f00_mi_fm10_x1 = (f00 - fm10)*rx_10, f00_mi_f0m1_z1 = (f00 - f0m1)*rz_01;
									double f00_mi_f10_xm1 = (f00 - f10)*rx_m10, f00_mi_f01_zm1 = (f00 - f01)*rz_0m1;
									a10 = DX*(f00_mi_f10_xm1*rx_m10 - f00_mi_fm10_x1*rx_10); a01 = DZ*(f00_mi_f01_zm1*rz_0m1 - f00_mi_f0m1_z1*rz_01);
									a20 = DX*(f00_mi_fm10_x1 - f00_mi_f10_xm1); a02 = DZ*(f00_mi_f0m1_z1 - f00_mi_f01_zm1);
									*t_ExRes = (float)(dx_00*(dx_00*a20 + a10) + dz_00*(dz_00*a02 + a01) + f00);

									f0m1 = arEX[ofst_0m1_p_1]; fm10 = arEX[ofst_m10_p_1]; f00 = arEX[ofst_00_p_1]; f10 = arEX[ofst_10_p_1]; f01 = arEX[ofst_01_p_1];
									f00_mi_fm10_x1 = (f00 - fm10)*rx_10; f00_mi_f0m1_z1 = (f00 - f0m1)*rz_01;
									f00_mi_f10_xm1 = (f00 - f10)*rx_m10; f00_mi_f01_zm1 = (f00 - f01)*rz_0m1;
									a10 = DX*(f00_mi_f10_xm1*rx_m10 - f00_mi_fm10_x1*rx_10); a01 = DZ*(f00_mi_f01_zm1*rz_0m1 - f00_mi_f0m1_z1*rz_01);
									a20 = DX*(f00_mi_fm10_x1 - f00_mi_f10_xm1); a02 = DZ*(f00_mi_f0m1_z1 - f00_mi_f01_zm1);
									*(t_ExRes + 1) = (float)(dx_00*(dx_00*a20 + a10) + dz_00*(dz_00*a02 + a01) + f00);
								}
								if(arEZ != 0)
								{
									f0m1 = arEZ[ofst_0m1]; fm10 = arEZ[ofst_m10]; f00 = arEZ[ofst_00]; f10 = arEZ[ofst_10]; f01 = arEZ[ofst_01];
									double f00_mi_fm10_x1 = (f00 - fm10)*rx_10, f00_mi_f0m1_z1 = (f00 - f0m1)*rz_01;
									double f00_mi_f10_xm1 = (f00 - f10)*rx_m10, f00_mi_f01_zm1 = (f00 - f01)*rz_0m1;
									a10 = DX*(f00_mi_f10_xm1*rx_m10 - f00_mi_fm10_x1*rx_10); a01 = DZ*(f00_mi_f01_zm1*rz_0m1 - f00_mi_f0m1_z1*rz_01);
									a20 = DX*(f00_mi_fm10_x1 - f00_mi_f10_xm1); a02 = DZ*(f00_mi_f0m1_z1 - f00_mi_f01_zm1);
									*t_EzRes = (float)(dx_00*(dx_00*a20 + a10) + dz_00*(dz_00*a02 + a01) + f00);

									f0m1 = arEZ[ofst_0m1_p_1]; fm10 = arEZ[ofst_m10_p_1]; f00 = arEZ[ofst_00_p_1]; f10 = arEZ[ofst_10_p_1]; f01 = arEZ[ofst_01_p_1];
									f00_mi_fm10_x1 = (f00 - fm10)*rx_10; f00_mi_f0m1_z1 = (f00 - f0m1)*rz_01;
									f00_mi_f10_xm1 = (f00 - f10)*rx_m10; f00_mi_f01_zm1 = (f00 - f01)*rz_0m1;
									a10 = DX*(f00_mi_f10_xm1*rx_m10 - f00_mi_fm10_x1*rx_10); a01 = DZ*(f00_mi_f01_zm1*rz_0m1 - f00_mi_f0m1_z1*rz_01);
									a20 = DX*(f00_mi_fm10_x1 - f00_mi_f10_xm1); a02 = DZ*(f00_mi_f0m1_z1 - f00_mi_f01_zm1);
									*(t_EzRes + 1) = (float)(dx_00*(dx_00*a20 + a10) + dz_00*(dz_00*a02 + a01) + f00);
								}
							}
						}
						else
						{//irregular mesh (general case)
							double arXZ[] = {rx_0m1, rz_0m1, rx_m10, rz_m10, rx_10, rz_10, rx_01, rz_01};
							if(arEX != 0)
							{
								double arER[] = {arEX[ofst_0m1], arEX[ofst_m10], arEX[ofst_00], arEX[ofst_10], arEX[ofst_01]};
								*t_ExRes = (float)CGenMathInterp::Interp2dBiQuad5Var(dx_00, dz_00, arXZ, arER);
								double arEI[] = {arEX[ofst_0m1_p_1], arEX[ofst_m10_p_1], arEX[ofst_00_p_1], arEX[ofst_10_p_1], arEX[ofst_01_p_1]};
								*(t_ExRes + 1) = (float)CGenMathInterp::Interp2dBiQuad5Var(dx_00, dz_00, arXZ, arEI);
							}
							if(arEZ != 0)
							{
								double arER[] = {arEZ[ofst_0m1], arEZ[ofst_m10], arEZ[ofst_00], arEZ[ofst_10], arEZ[ofst_01]};
								*t_EzRes = (float)CGenMathInterp::Interp2dBiQuad5Var(dx_00, dz_00, arXZ, arER);
								double arEI[] = {arEZ[ofst_0m1_p_1], arEZ[ofst_m10_p_1], arEZ[ofst_00_p_1], arEZ[ofst_10_p_1], arEZ[ofst_01_p_1]};
								*(t_EzRes + 1) = (float)CGenMathInterp::Interp2dBiQuad5Var(dx_00, dz_00, arXZ, arEI);
							}
						}
					}
				}
				else
				{
					SetFieldToZero:
					*t_ExRes = 0.; *(t_ExRes+1) = 0.;
					*t_EzRes = 0.; *(t_EzRes+1) = 0.;
				}

				//test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				//long ofstAux = iz*PerZ + ix*PerX + ie*2;
				//*t_ExRes = arEX[ofstAux]; *(t_ExRes+1) = arEX[ofstAux+1];
				//*t_EzRes = arEZ[ofstAux]; *(t_EzRes+1) = arEZ[ofstAux+1];
				//end test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

				t_ExRes += 2;
				t_EzRes += 2;

				phEn += pWfr->eStep;
			}
			x += pWfr->xStep;
		}
		z += pWfr->zStep;
	}

	if(waveFrontTermWasTreated) TreatStronglyOscillatingTerm(*pWfr, 'a');
	return 0;
}

//*************************************************************************

void srTMirror::RadPointModifier_ThinElem(srTEXZ& EXZ, srTEFieldPtrs& EPtrs)
{
	TVector3d inP(EXZ.x, EXZ.z, -m_extAlongOptAxIn), intersP;
	inP = TransHndl.rep->TrPoint_inv(inP); //Point in input transverse plane in local frame

	if(!FindRayIntersectWithSurfInLocFrame(inP, m_vInLoc, intersP)) 
	{
		*(EPtrs.pExIm) = 0.; *(EPtrs.pExRe) = 0.; *(EPtrs.pEzIm) = 0.; *(EPtrs.pEzRe) = 0.;
		return;
	}
	if(!CheckIfPointIsWithinOptElem(intersP.x, intersP.y)) 
	{
		*(EPtrs.pExIm) = 0.; *(EPtrs.pExRe) = 0.; *(EPtrs.pEzIm) = 0.; *(EPtrs.pEzRe) = 0.;
		return;
	}

	//Distance from intersection point with surface to intersection point with output transverse plane of this step
	double distBwIntersPtAndOut = m_vOutLoc*(m_vPtOutLoc - intersP);
	double distBwInAndIntersP = m_vInLoc*(intersP - inP);

	double optPathDif = distBwInAndIntersP + distBwIntersPtAndOut - (m_extAlongOptAxIn + m_extAlongOptAxOut);
	double phShift = 5.067730652e+06*EXZ.e*optPathDif; //to check sign!
	float cosPh, sinPh;
	CosAndSin(phShift, cosPh, sinPh);

	if(m_reflData.pData == 0) //no reflectivity defined
	{
		float NewExRe = (*(EPtrs.pExRe))*cosPh - (*(EPtrs.pExIm))*sinPh;
		float NewExIm = (*(EPtrs.pExRe))*sinPh + (*(EPtrs.pExIm))*cosPh;
		*(EPtrs.pExRe) = NewExRe; *(EPtrs.pExIm) = NewExIm; 
		float NewEzRe = (*(EPtrs.pEzRe))*cosPh - (*(EPtrs.pEzIm))*sinPh;
		float NewEzIm = (*(EPtrs.pEzRe))*sinPh + (*(EPtrs.pEzIm))*cosPh;
		*(EPtrs.pEzRe) = NewEzRe; *(EPtrs.pEzIm) = NewEzIm; 
		return;
	}

	//Calculate change of the electric field due to reflectivity...
	TVector3d vNormAtP;
	FindSurfNormalInLocFrame(intersP.x, intersP.y, vNormAtP);
	vNormAtP = TransHndl.rep->TrBiPoint(vNormAtP);  //to the frame of incident beam

	//double xp = (EXZ.x - m_inWfrCh)/m_inWfrRh, yp = (EXZ.z - m_inWfrCv)/m_inWfrRv;
	//TVector3d vRay(0, 0, 1.); //in the frame of incident beam
	//TVector3d vSig = vNormAtP^vRay; vSig.Normalize(); //in the frame of incident beam

	double vSigX=1., vSigY=0., vPiX=0., vPiY=1.;
	if((vNormAtP.x != 0.) || (vNormAtP.y != 0.))
	{
		//double multNorm = sqrt(vNormAtP.x*vNormAtP.x + vNormAtP.y*vNormAtP.y);
		double multNorm = 1./sqrt(vNormAtP.x*vNormAtP.x + vNormAtP.y*vNormAtP.y); //?
		vSigX = vNormAtP.y*multNorm; vSigY = -vNormAtP.x*multNorm;
		vPiX = -vSigY; vPiY = vSigX;
	}

	//TVector3d vEr(*(EPtrs.pExRe), *(EPtrs.pEzRe), 0), vEi(*(EPtrs.pExIm), *(EPtrs.pEzIm), 0);
	//Maybe rather this? //TVector3d vEr(-*(EPtrs.pExRe), *(EPtrs.pEzRe), 0), vEi(-*(EPtrs.pExIm), *(EPtrs.pEzIm), 0);
	//double EsigRe = vEr*vSig, EsigIm = vEi*vSig; //in the frame of incident beam
	//double EpiRe = vEr*vPi, EpiIm = vEi*vPi;
	//Sigma and Pi components of input electric field in the frame of incident beam
	double EsigRe = (*(EPtrs.pExRe))*vSigX + (*(EPtrs.pEzRe))*vSigY;
	double EsigIm = (*(EPtrs.pExIm))*vSigX + (*(EPtrs.pEzIm))*vSigY;
	double EpiRe = (*(EPtrs.pExRe))*vPiX + (*(EPtrs.pEzRe))*vPiY;
	double EpiIm = (*(EPtrs.pExIm))*vPiX + (*(EPtrs.pEzIm))*vPiY;

	//double sinAngInc = ::fabs(vRay*vNormAtP);
	double sinAngInc = ::fabs(vNormAtP.z);
	double angInc = asin(sinAngInc);
	double RsigRe=1, RsigIm=0, RpiRe=1, RpiIm=0;
	GetComplexReflectCoefFromTable(EXZ.e, angInc, RsigRe, RsigIm, RpiRe, RpiIm);

	double newEsigRe = -cosPh*(EsigIm*RsigIm - EsigRe*RsigRe) - sinPh*(EsigRe*RsigIm + EsigIm*RsigRe);
	double newEsigIm = cosPh*(EsigRe*RsigIm + EsigIm*RsigRe) - sinPh*(EsigIm*RsigIm - EsigRe*RsigRe);
	double newEpiRe = -cosPh*(EpiIm*RpiIm - EpiRe*RpiRe) - sinPh*(EpiRe*RpiIm + EpiIm*RpiRe);
	double newEpiIm = cosPh*(EpiRe*RpiIm + EpiIm*RpiRe) - sinPh*(EpiIm*RpiIm - EpiRe*RpiRe);

	//vEr = newEsigRe*vSig + newEpiRe*vPi; //in the frame of incident beam
	//vEi = newEsigIm*vSig + newEpiIm*vPi;
	double vErX = newEsigRe*vSigX + newEpiRe*vPiX; //in the frame of incident beam
	double vErY = newEsigRe*vSigY + newEpiRe*vPiY;
	double vEiX = newEsigIm*vSigX + newEpiIm*vPiX;
	double vEiY = newEsigIm*vSigY + newEpiIm*vPiY;

	//electric field components in the frame of output beam 
	//test
	//*(EPtrs.pExRe) = (float)(vEr*m_vHorOutIn);
	//*(EPtrs.pExIm) = (float)(vEi*m_vHorOutIn);
	//*(EPtrs.pEzRe) = (float)(vEr*m_vVerOutIn);
	//*(EPtrs.pEzIm) = (float)(vEi*m_vVerOutIn);
	*(EPtrs.pExRe) = (float)(vErX*m_vHorOutIn.x + vErY*m_vHorOutIn.y);
	*(EPtrs.pExIm) = (float)(vEiX*m_vHorOutIn.x + vEiY*m_vHorOutIn.y);
	*(EPtrs.pEzRe) = (float)(vErX*m_vVerOutIn.x + vErY*m_vVerOutIn.y);
	*(EPtrs.pEzIm) = (float)(vEiX*m_vVerOutIn.x + vEiY*m_vVerOutIn.y);
}

//*************************************************************************

int srTMirror::PropagateRadiationSimple_LocRayTracing(srTSRWRadStructAccessData* pRadAccessData)
{
	int res = 0;

	char LocWaveFrontTermCanBeTreated = WaveFrontTermCanBeTreated(*pRadAccessData); //checks if quad. term can be treated and set local variables

	if((m_treatInOut == 2) && (m_extAlongOptAxIn != 0.))
	{//Propagate wavefront back (by -m_extAlongOptAxIn) to the beginning of the optical element using Wavefront Propagation through a Drift
		srTRadResizeVect dummyResizeVect; //consider removing this completely
		srTDriftSpace driftIn(-m_extAlongOptAxIn);
		driftIn.PropBufVars.UseExactRxRzForAnalytTreatQuadPhaseTerm = true;
		if(res = driftIn.PropagateRadiation(pRadAccessData, m_ParPrecWfrPropag, dummyResizeVect)) return res;
	}

	double RxInWfr = pRadAccessData->RobsX;
	double xcInWfr = pRadAccessData->xc;
	double RzInWfr = pRadAccessData->RobsZ;
	double zcInWfr = pRadAccessData->zc;

		//test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//TreatStronglyOscillatingTerm(*pRadAccessData, 'r');
		//return 0;
		//end test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	if(((m_treatInOut == 0) || (m_treatInOut == 2)) && (m_extAlongOptAxIn != 0))
	{
		srTDriftSpace driftAux(m_extAlongOptAxIn);
		driftAux.PropagateWaveFrontRadius(pRadAccessData);
	}
	if(res = PropagateWaveFrontRadius(pRadAccessData)) return res;
	if(((m_treatInOut == 0) || (m_treatInOut == 2)) && (m_extAlongOptAxOut != 0))
	{
		srTDriftSpace driftAux(m_extAlongOptAxOut);
		driftAux.PropagateWaveFrontRadius(pRadAccessData);
	}

	double RxOutWfr = pRadAccessData->RobsX;
	double RzOutWfr = pRadAccessData->RobsZ;

	//double ampFact = 1.;
	//double RxInCor = RxInWfr + m_extAlongOptAxIn;
	//double RzInCor = RzInWfr + m_extAlongOptAxIn;
	//double RxOutCor = RxOutWfr - m_extAlongOptAxOut;
	//double RzOutCor = RzOutWfr - m_extAlongOptAxOut;
	//if((RxInCor != 0.) && (RzInCor != 0.) && (RxOutWfr != 0.) && (RzOutCor != 0.))
	//{
	//	double ampFactE2 = RxInWfr*RxOutCor/(RxInCor*RxOutWfr);
	//	ampFactE2 *= RzInWfr*RzOutCor/(RzInCor*RzOutWfr);
	//	ampFact = sqrt(fabs(ampFactE2));
	//}
	double ampFact, ampFactE2, RxInCor, RzInCor, RxOutCor, RzOutCor;

	gmTrans *pTrans = TransHndl.rep;

	TVector3d rayLocFr[2]; //ray[2], , RayOut[2], arIntersectP[3];
	TVector3d &rayLocFrP = rayLocFr[0], &rayLocFrV = rayLocFr[1];
	TVector3d vIntersPtLocFr, vSurfNormLocFr;
	TVector3d vAuxIntersectP, vAuxOptPath, vRayIn, vSig, vPi, vTrAux;

	TVector3d planeBeforeLocFr[2]; // vAuxIntersectP, vAuxDif;
	TVector3d &planeBeforeLocFrP = planeBeforeLocFr[0], &planeBeforeLocFrV = planeBeforeLocFr[1];
	
	planeBeforeLocFrP.x = TransvCenPoint.x;
	planeBeforeLocFrP.y = TransvCenPoint.y;
	//planeBeforeLocFrP.z = 0.;
	//if(m_treatInOut == 1) planeBeforeLocFrP.z = -m_extAlongOptAxIn;
	planeBeforeLocFrP.z = -m_extAlongOptAxIn;

	planeBeforeLocFrV.x = planeBeforeLocFrV.y = 0.; 
	planeBeforeLocFrV.z = 1.;
	if(pTrans != 0)
	{
		planeBeforeLocFrP = pTrans->TrPoint_inv(planeBeforeLocFrP);
		planeBeforeLocFrV = pTrans->TrBiPoint_inv(planeBeforeLocFrV);
	}

	TVector3d planeAfterLocFr[2]; //point and normal vector (in the direction of beam propagation) to the plane at the exit of the optical element
	TVector3d &planeAfterLocFrP = planeAfterLocFr[0], &planeAfterLocFrV = planeAfterLocFr[1];

	TVector3d planeCenOutLocFr[2]; //point and normal vector (in the direction of beam propagation) to the plane at the center of the optical element
	TVector3d &planeCenOutLocFrP = planeCenOutLocFr[0], &planeCenOutLocFrV = planeCenOutLocFr[1];

	//Determine Exit (output) plane coordinates in the Local frame
	if(!FindRayIntersectWithSurfInLocFrame(planeBeforeLocFrP, m_vInLoc, planeAfterLocFrP)) return FAILED_DETERMINE_OPTICAL_AXIS;
	planeAfterLocFrV = m_vOutLoc;

	planeCenOutLocFrV = m_vOutLoc;
	planeCenOutLocFrP = planeAfterLocFrP;
	planeAfterLocFrP += m_extAlongOptAxOut*m_vOutLoc;

	float *pEX0 = pRadAccessData->pBaseRadX;
	float *pEZ0 = pRadAccessData->pBaseRadZ;
	double ePh = pRadAccessData->eStart, x, y;

	long PerX = pRadAccessData->ne << 1;
	long PerY = PerX*pRadAccessData->nx;

	long nTot = PerY*pRadAccessData->nz;
	float *arAuxRayTrCoord = new float[nTot];
	if(arAuxRayTrCoord == 0) return NOT_ENOUGH_MEMORY_FOR_SR_COMP;

	float *arAuxEX=0, *arAuxEY=0;
	if(pEX0 != 0) 
	{
		arAuxEX = new float[nTot];
		if(arAuxEX == 0) return NOT_ENOUGH_MEMORY_FOR_SR_COMP;
	}
	if(pEZ0 != 0)
	{
		arAuxEY = new float[nTot];
		if(arAuxEY == 0) return NOT_ENOUGH_MEMORY_FOR_SR_COMP;
	}

	float xRelOutMin = (float)1.E+23, xRelOutMax = (float)(-1.E+23);
	float yRelOutMin = (float)1.E+23, yRelOutMax = (float)(-1.E+23);
	long ixMin = pRadAccessData->nx - 1, ixMax = 0;
	long iyMin = pRadAccessData->nz - 1, iyMax = 0;
	float cosPh, sinPh;
	double EsigRe, EsigIm, EpiRe, EpiIm;

	for(long ie=0; ie<pRadAccessData->ne; ie++)
	{
		double TwoPi_d_LambdaM = ePh*5.067730652e+06;
		long Two_ie = ie << 1;
	
		y = pRadAccessData->zStart;

		for(long iy=0; iy<pRadAccessData->nz; iy++)
		{
			long iyPerY = iy*PerY;
			float *pEX_StartForX = pEX0 + iyPerY;
			float *pEZ_StartForX = pEZ0 + iyPerY;

			float *pEX_StartForXres = arAuxEX + iyPerY;
			float *pEY_StartForXres = arAuxEY + iyPerY;

			float *pAuxRayTrCoord = arAuxRayTrCoord + iyPerY;

			x = pRadAccessData->xStart;
			for(long ix=0; ix<pRadAccessData->nx; ix++)
			{
				long ixPerX_p_Two_ie = ix*PerX + Two_ie;
				float *pExRe = pEX_StartForX + ixPerX_p_Two_ie;
				float *pExIm = pExRe + 1;
				float *pEzRe = pEZ_StartForX + ixPerX_p_Two_ie;
				float *pEzIm = pEzRe + 1;

				float *pExReRes = pEX_StartForXres + ixPerX_p_Two_ie;
				float *pExImRes = pExReRes + 1;
				float *pEyReRes = pEY_StartForXres + ixPerX_p_Two_ie;
				float *pEyImRes = pEyReRes + 1;
				float *pAuxRayTrCoordX = pAuxRayTrCoord + ixPerX_p_Two_ie;
				float *pAuxRayTrCoordY = pAuxRayTrCoordX + 1;

				*pAuxRayTrCoordX = (float)(-1.E+23); *pAuxRayTrCoordY = (float)(-1.E+23);

				bool ExIsNotZero = false, EzIsNotZero = false;
				if(pEX0 != 0)
				{
					*pExReRes = 0.; *pExImRes = 0.;
					if((*pExRe != 0) || (*pExIm != 0)) ExIsNotZero = true;
				}
				if(pEZ0 != 0)
				{
					*pEyReRes = 0.; *pEyImRes = 0.;
					if((*pEzRe != 0) || (*pEzIm != 0)) EzIsNotZero = true;
				}
					//test!!!!!!!!!!!!!!!!!!!!!
					//x = 0.; y = 0.;
				if(ExIsNotZero || EzIsNotZero)
				{
					//double tgAngX=0, tgAngY=0;
					//pRadAccessData->GetWaveFrontNormal(x, y, tgAngX, tgAngY);

					double tgAngX = (x - xcInWfr)/RxInWfr; //check sign
					double tgAngY = (y - zcInWfr)/RzInWfr; //check sign

						//test!!!!!!!!!!!!!!!!!!!!!
						//tgAngX = 0.; tgAngY = 0.;

					rayLocFrV.x = tgAngX;
					rayLocFrV.y = tgAngY;
					rayLocFrV.z = sqrt(1. - rayLocFrV.x*rayLocFrV.x - rayLocFrV.y*rayLocFrV.y);
					rayLocFrP.x = x; rayLocFrP.y = y; rayLocFrP.z = 0.;

					if((m_treatInOut == 0) || (m_treatInOut == 2))
					{
						rayLocFrP.z = -m_extAlongOptAxIn; //?
					}

					if(pTrans != 0)
					{//from input beam frame to local frame
						rayLocFrP = pTrans->TrPoint_inv(rayLocFrP);
						rayLocFrV = pTrans->TrBiPoint_inv(rayLocFrV);
					}
					vRayIn = rayLocFrV;

					//if((m_treatIn == 1) && (m_extAlongOptAxIn != 0.)) //check sign?
					//{//propagate back to a plane before optical element, using geometrical ray-tracing
					//	FindLineIntersectWithPlane(planeBeforeLocFr, rayLocFr, vAuxIntersectP);
					//	rayLocFrP = vAuxIntersectP;
					//}

					//bool intersectHappened = false;
					if(FindRayIntersectWithSurfInLocFrame(rayLocFrP, rayLocFrV, vIntersPtLocFr, &vSurfNormLocFr))
					{
						if(CheckIfPointIsWithinOptElem(vIntersPtLocFr.x, vIntersPtLocFr.y)) 
						{//continue calculating reflected and propagated electric field
							//test
							//FindRayIntersectWithSurfInLocFrame(rayLocFrP, rayLocFrV, vIntersPtLocFr, &vSurfNormLocFr);
							//intersectHappened = true;

							vAuxOptPath = vIntersPtLocFr - rayLocFrP;
							//double optPath = vAuxOptPath.Abs();

							double optPath = vAuxOptPath*rayLocFrV;
							double optPathBefore = optPath;

							//ray after reflection (in local frame):
							rayLocFrP = vIntersPtLocFr;
							rayLocFrV -= (2.*(rayLocFrV*vSurfNormLocFr))*vSurfNormLocFr;
							rayLocFrV.Normalize();

							if((m_treatInOut == 0) || (m_treatInOut == 2))
							{
								FindLineIntersectWithPlane(planeAfterLocFr, rayLocFr, vAuxIntersectP);
							}
							else if(m_treatInOut == 1)
							{
								FindLineIntersectWithPlane(planeCenOutLocFr, rayLocFr, vAuxIntersectP);
							}

							vAuxOptPath = vAuxIntersectP - rayLocFrP;
							//optPath += vAuxOptPath.Abs();
							double optPathAfter = vAuxOptPath*rayLocFrV;
							optPath += optPathAfter;

							//double RxInCor = (RxInWfr > 0)? (RxInWfr + optPathBefore) : (RxInWfr - optPathBefore);
							//double RzInCor = (RzInWfr > 0)? (RzInWfr + optPathBefore) : (RzInWfr - optPathBefore);
							//double RxOutCor = (RxOutWfr > 0)? (RxOutWfr + optPathAfter) : (RxOutWfr - optPathAfter);
							//double RzOutCor = (RzOutWfr > 0)? (RzOutWfr + optPathAfter) : (RzOutWfr - optPathAfter);

							ampFact = 1.;
							RxInCor = RxInWfr + optPathBefore; //to check signs
							RzInCor = RzInWfr + optPathBefore;
							RxOutCor = RxOutWfr - optPathAfter;
							RzOutCor = RzOutWfr - optPathAfter;
							if((RxInCor != 0.) && (RzInCor != 0.) && (RxOutWfr != 0.) && (RzOutCor != 0.))
							{
								ampFactE2 = RxInWfr*RxOutCor/(RxInCor*RxOutWfr);
								ampFactE2 *= RzInWfr*RzOutCor/(RzInCor*RzOutWfr);
								ampFact = sqrt(fabs(ampFactE2));
							}

							//Calculating transverse coordinates of intersection point of the ray with the output plane (or central plane) in the frame of the output beam
							vTrAux = vAuxIntersectP - planeCenOutLocFrP;
							if(pTrans != 0)
							{//from local frame to input beam frame
								vTrAux = pTrans->TrBiPoint(vTrAux);
							}
							float xRelOut = (float)(vTrAux*m_vHorOutIn);
							float yRelOut = (float)(vTrAux*m_vVerOutIn);
							//test!!!!!!!!!!!!!!!!!!!!!
							//float yRelOut = -(float)(vTrAux*m_vVerOutIn);
							//end test!!!!!!!!!!!!!!!!!!!!!

							*pAuxRayTrCoordX = xRelOut;
							*pAuxRayTrCoordY = yRelOut;

							if(xRelOutMin > xRelOut) 
							{
								xRelOutMin = xRelOut; ixMin = ix;
							}
							if(xRelOutMax < xRelOut) 
							{
								xRelOutMax = xRelOut; ixMax = ix;
							}
							if(yRelOutMin > yRelOut) 
							{
								yRelOutMin = yRelOut; iyMin = iy;
							}
							if(yRelOutMax < yRelOut) 
							{
								yRelOutMax = yRelOut; iyMax = iy;
							}

							//double angE2 = tgAngX*tgAngX + tgAngY*tgAngY;
							//double angFact = 1. + angE2*(0.5 + angE2*((5./24.) + (61./720.)*angE2));
							//////double angFact = 1. + angE2*0.5;
							////double optPathDif = optPath - (m_extAlongOptAxIn + m_extAlongOptAxOut)*angFact; //L/cos(alpha)
							//////double optPathDif = optPath - (m_extAlongOptAxIn + m_extAlongOptAxOut); //L/cos(alpha)
							//double optPathDif = optPath;

							//last commented:
							//double phShift = TwoPi_d_LambdaM*optPathDif; //to check sign!
							double phShift = TwoPi_d_LambdaM*optPath; //to check sign!
							//double phShift = -TwoPi_d_LambdaM*optPathDif; //to check sign!

							//test
							//phShift += 0.04005;

							CosAndSin(phShift, cosPh, sinPh);

							if(m_reflData.pData == 0) //no reflectivity defined
							//if(true) //no reflectivity defined
							{
								if(pEX0 != 0)
								{
									float NewExRe = (float)(ampFact*((*pExRe)*cosPh - (*pExIm)*sinPh));
									float NewExIm = (float)(ampFact*((*pExRe)*sinPh + (*pExIm)*cosPh));
									//*pExRe = NewExRe; *pExIm = NewExIm; 
									*pExReRes = NewExRe; *pExImRes = NewExIm;
								}
								if(pEZ0 != 0)
								{
									float NewEzRe = (float)(ampFact*((*pEzRe)*cosPh - (*pEzIm)*sinPh));
									float NewEzIm = (float)(ampFact*((*pEzRe)*sinPh + (*pEzIm)*cosPh));
									//*pEzRe = NewEzRe; *pEzIm = NewEzIm; 
									*pEyReRes = NewEzRe; *pEyImRes = NewEzIm;
								}

									//test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
									//double Pi_d_Lambda_m = ePh*2.533840802E+06;
									//double xRel = x - TransvCenPoint.x, zRel = y - TransvCenPoint.y;

									//phShift = -Pi_d_Lambda_m*(xRel*xRel/FocDistX + zRel*zRel/FocDistZ);
									//CosAndSin(phShift, cosPh, sinPh);
									//float NewExRe = (*pExRe)*cosPh - (*pExIm)*sinPh;
									//float NewExIm = (*pExRe)*sinPh + (*pExIm)*cosPh;
									//*pExReRes = NewExRe; *pExImRes = NewExIm; 
									//float NewEzRe = (*pEzRe)*cosPh - (*pEzIm)*sinPh;
									//float NewEzIm = (*pEzRe)*sinPh + (*pEzIm)*cosPh;
									//*pEyReRes = NewEzRe; *pEyImRes = NewEzIm; 
									//end test!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

									//*pExRe = phShift; *pExIm = 0; 
									//*pEzRe = phShift; *pEzIm = 0; 
							}
							else
							//if(m_reflData.pData != 0)
							{//Calculate change of the electric field due to reflectivity...
								vRayIn.Normalize();
								vSig = (-1)*(vRayIn^vSurfNormLocFr); //sigma unit vector in Local frame; check sign
								double grazAng = 1.5707963268;
								if(vSig.isZero())
								{//In the frame of incident beam
									vSig.x = 1.; vSig.y = 0.; vSig.z = 0.;
									vPi.x = 0.; vPi.y = 1.; vPi.z = 0.;
								}
								else
								{
									//grazAng = asin(-(vRayIn*vSurfNormLocFr));
									grazAng = acos(vRayIn*vSurfNormLocFr) - 1.5707963267948966;

									vSig.Normalize();
									vPi = vRayIn^vSig;
									if(pTrans != 0)
									{//to the frame of incident beam
										vSig = pTrans->TrBiPoint(vSig); 
										vPi = pTrans->TrBiPoint(vPi); 
									}
								}

								EsigRe = EsigIm = EpiRe = EpiIm = 0.;
								if(pEX0 != 0)
								{
									EsigRe = (*pExRe)*vSig.x;
									EsigIm = (*pExIm)*vSig.x;
									EpiRe = (*pExRe)*vPi.x;
									EpiIm = (*pExIm)*vPi.x;
								}
								if(pEZ0 != 0)
								{
									EsigRe += (*pEzRe)*vSig.y;
									EsigIm += (*pEzIm)*vSig.y;
									EpiRe += (*pEzRe)*vPi.y;
									EpiIm += (*pEzIm)*vPi.y;
								}
								//double EsigRe = (*pExRe)*vSig.x + (*pEzRe)*vSig.y;
								//double EsigIm = (*pExIm)*vSig.x + (*pEzIm)*vSig.y;
								//double EpiRe = (*pExRe)*vPi.x + (*pEzRe)*vPi.y;
								//double EpiIm = (*pExIm)*vPi.x + (*pEzIm)*vPi.y;

								double RsigRe = 1, RsigIm = 0, RpiRe = 1, RpiIm = 0;
								GetComplexReflectCoefFromTable(ePh, grazAng, RsigRe, RsigIm, RpiRe, RpiIm);

								double newEsigRe = -cosPh*(EsigIm*RsigIm - EsigRe*RsigRe) - sinPh*(EsigRe*RsigIm + EsigIm*RsigRe);
								double newEsigIm = cosPh*(EsigRe*RsigIm + EsigIm*RsigRe) - sinPh*(EsigIm*RsigIm - EsigRe*RsigRe);
								double newEpiRe = -cosPh*(EpiIm*RpiIm - EpiRe*RpiRe) - sinPh*(EpiRe*RpiIm + EpiIm*RpiRe);
								double newEpiIm = cosPh*(EpiRe*RpiIm + EpiIm*RpiRe) - sinPh*(EpiIm*RpiIm - EpiRe*RpiRe);
								//double newEsigRe = -(EsigIm*RsigIm - EsigRe*RsigRe);
								//double newEsigIm = EsigRe*RsigIm + EsigIm*RsigRe;
								//double newEpiRe = -(EpiIm*RpiIm - EpiRe*RpiRe);
								//double newEpiIm = EpiRe*RpiIm + EpiIm*RpiRe;

								//In the frame of incident beam:
								double vErX = newEsigRe*vSig.x + newEpiRe*vPi.x;
								double vErY = newEsigRe*vSig.y + newEpiRe*vPi.y;
								double vEiX = newEsigIm*vSig.x + newEpiIm*vPi.x;
								double vEiY = newEsigIm*vSig.y + newEpiIm*vPi.y;

								//In the frame of output beam:
								if(pEX0 != 0)
								{
									//*pExRe = (float)(vErX*m_vHorOutIn.x + vErY*m_vHorOutIn.y);
									//*pExIm = (float)(vEiX*m_vHorOutIn.x + vEiY*m_vHorOutIn.y);
									*pExReRes = (float)(ampFact*(vErX*m_vHorOutIn.x + vErY*m_vHorOutIn.y)); 
									*pExImRes = (float)(ampFact*(vEiX*m_vHorOutIn.x + vEiY*m_vHorOutIn.y));
								}
								if(pEZ0 != 0)
								{
									//*pEzRe = (float)(vErX*m_vVerOutIn.x + vErY*m_vVerOutIn.y);
									//*pEzIm = (float)(vEiX*m_vVerOutIn.x + vEiY*m_vVerOutIn.y);
									*pEyReRes = (float)(ampFact*(vErX*m_vVerOutIn.x + vErY*m_vVerOutIn.y)); 
									*pEyImRes = (float)(ampFact*(vEiX*m_vVerOutIn.x + vEiY*m_vVerOutIn.y));
								}
									//test!!!!!!!!!!!!!!!!!!!!!
									//*pExReRes = optPath; *pExImRes = 0.;
									//*pEyReRes = optPath; *pEyImRes = 0.;
									//end test!!!!!!!!!!!!!!!!!!!!!
							}
						}
					}
				}
				x += pRadAccessData->xStep;
			}
			y += pRadAccessData->zStep;
		}
		ePh += pRadAccessData->eStep;
	}

	//Re-interpolate the output wavefront (at fixed photon energy) on the initial equidistant grid:
	if(res = WfrInterpolOnOrigGrid(pRadAccessData, arAuxRayTrCoord, arAuxEX, arAuxEY, xRelOutMin, xRelOutMax, yRelOutMin, yRelOutMax)) return res;

	if((m_treatInOut == 2) && (m_extAlongOptAxOut != 0.))
	{//Propagate wavefront back (by -m_extAlongOptAxOut) to the center of the optical element using Wavefront Propagation through a Drift
		srTRadResizeVect dummyResizeVect; //consider removing this completely
		srTDriftSpace driftOut(-m_extAlongOptAxOut);
		driftOut.PropBufVars.UseExactRxRzForAnalytTreatQuadPhaseTerm = true;
		if(res = driftOut.PropagateRadiation(pRadAccessData, m_ParPrecWfrPropag, dummyResizeVect)) return res;
	}

	if(arAuxEX != 0) delete[] arAuxEX;
	if(arAuxEY != 0) delete[] arAuxEY;
	if(arAuxRayTrCoord != 0) delete[] arAuxRayTrCoord;
	//if(arOptPathDif != 0) delete[] arOptPathDif;
	return 0;
}

//*************************************************************************
//Test of propagation by Fourier method in steps (failed?)
int srTMirror::PropagateRadiationSimple_FourierByParts(srTSRWRadStructAccessData* pRadAccessData)
{
	int res = 0;
	//propagate wavefront back (by -m_extAlongOptAxIn) to the beginning of the optical element
	//to make optional, assuming that the wavefront can be supplied already before the optical element, and not in its middle 
	srTRadResizeVect dummyResizeVect; //consider removing this completely
	srTDriftSpace driftIn(-m_extAlongOptAxIn);
	driftIn.PropBufVars.UseExactRxRzForAnalytTreatQuadPhaseTerm = true;
	if(res = driftIn.PropagateRadiation(pRadAccessData, m_ParPrecWfrPropag, dummyResizeVect)) return res;

	//m_pRadAux = new srTSRWRadStructAccessData(pRadAccessData); //to propagate "old" wavefront part

	//if(res = PropagateWaveFrontRadius(pRadAccessData)) return res;
	//pRadAccessData->AssignElFieldToConst((float)0., (float)0.); //to place and propagate "new" wavefront parts
	//test
	//FocDistZ = 0.153; //test

	//propagate through the optical element by steps
	double stepProp = (m_extAlongOptAxIn + m_extAlongOptAxOut)/m_numPartsProp;
	srTDriftSpace driftStep(stepProp);
	driftStep.PropBufVars.UseExactRxRzForAnalytTreatQuadPhaseTerm = true;

	m_longPosStartPropPart = 0.; m_longPosEndPropPart = stepProp;

	double posOnOutOptAx = m_extAlongOptAxOut - stepProp*(m_numPartsProp - 1);
	m_vPtOutLoc = posOnOutOptAx*m_vOutLoc;
	TVector3d vStepProp = stepProp*m_vOutLoc;

	for(int i=0; i<m_numPartsProp; i++)
	{
		m_inWfrRh = pRadAccessData->RobsX; m_inWfrRv = pRadAccessData->RobsZ;
		m_inWfrCh = pRadAccessData->xc; m_inWfrCv = pRadAccessData->zc;
		//m_inWfrRh = m_pRadAux->RobsX; m_inWfrRv = m_pRadAux->RobsZ;
		//m_inWfrCh = m_pRadAux->xc; m_inWfrCv = m_pRadAux->zc;
		if(res = TraverseRadZXE(pRadAccessData)) return res;

		if(res = driftStep.PropagateRadiation(pRadAccessData, m_ParPrecWfrPropag, dummyResizeVect)) return res;
		//if(res = driftStep.PropagateRadiation(m_pRadAux, m_ParPrecWfrPropag, dummyResizeVect)) return res;
			//if(i == 2) break;
			//break;

		m_longPosStartPropPart = m_longPosEndPropPart; m_longPosEndPropPart += stepProp;
		m_vPtOutLoc += vStepProp;
	}

	//srTDriftSpace driftOutCen(-m_extAlongOptAxOut);
	//driftOutCen.PropBufVars.UseExactRxRzForAnalytTreatQuadPhaseTerm = true;
	//if(res = driftOutCen.PropagateRadiation(pRadAccessData, m_ParPrecWfrPropag, dummyResizeVect)) return res;
	
	//delete m_pRadAux; m_pRadAux = 0; //memory leak is possible!
	return 0;
}

//*************************************************************************
//Test of propagation by Fourier method in steps (failed?)
void srTMirror::RadPointModifier_FourierByParts(srTEXZ& EXZ, srTEFieldPtrs& EPtrs)
{//adds optical path difference to simulate propagation through a part of optical element

	TVector3d inP(EXZ.x, EXZ.z, -m_extAlongOptAxIn + m_longPosStartPropPart);
	inP = TransHndl.rep->TrPoint_inv(inP); //point in input transverse plane in local frame
	
	TVector3d intersP;
	FindRayIntersectWithSurfInLocFrame(inP, m_vInLoc, intersP);

	if(!CheckIfPointIsWithinOptElem(intersP.x, intersP.y))
	{
		*(EPtrs.pExIm) = 0.; *(EPtrs.pExRe) = 0.; *(EPtrs.pEzIm) = 0.; *(EPtrs.pEzRe) = 0.;
		//if(m_pRadAux != 0)
		//{
		//	if(m_pRadAux->pBaseRadX != 0)
		//	{
		//		float *pEx = m_pRadAux->pBaseRadX + EXZ.aux_offset;
		//		*(pEx++) = 0; *(pEx++) = 0;
		//	}
		//	if(m_pRadAux->pBaseRadZ != 0)
		//	{
		//		float *pEz = m_pRadAux->pBaseRadZ + EXZ.aux_offset;
		//		*(pEz++) = 0; *(pEz++) = 0;
		//	}
		//}
		return;
	}

	//distance from intersection point with surface to intersection point with output transverse plane of this step
	double distBwIntersPtAndOut = m_vOutLoc*(m_vPtOutLoc - intersP);

	if(distBwIntersPtAndOut < 0)
	{
		//*(EPtrs.pExIm) = 0.; *(EPtrs.pExRe) = 0.; *(EPtrs.pEzIm) = 0.; *(EPtrs.pEzRe) = 0.;
		return;
	}

	//double dx = intersP.x - inP.x, dy = intersP.y - inP.y, dz = intersP.z - inP.z;
	//double distBwInAndIntersP = sqrt(dx*dx + dy*dy + dz*dz);

	double stepProp = m_longPosEndPropPart - m_longPosStartPropPart;

	double distBwInAndIntersP = m_vInLoc*(intersP - inP);
	if(distBwInAndIntersP < 0.) return; //no need to apply opt. path correction at this point, because it has been already applied

	if(distBwInAndIntersP > stepProp) 
	{
		//*(EPtrs.pExIm) = 0.; *(EPtrs.pExRe) = 0.; *(EPtrs.pEzIm) = 0.; *(EPtrs.pEzRe) = 0.;
		return; //no need to apply opt. path correction at this point: it will be applied at next step(s)
	}

	//add optical path difference, taking into account electric field transformation at mirror surface
	double optPathDif = distBwInAndIntersP + distBwIntersPtAndOut - stepProp;
	double phShift = 5.067730652e+06*EXZ.e*optPathDif; //to check sign!
	float cosPh, sinPh;
	CosAndSin(phShift, cosPh, sinPh);
	//test
	//cosPh = 1.; sinPh = 0.;

	//if(m_pRadAux != 0)
	//{
	//	if(m_pRadAux->pBaseRadX != 0)
	//	{
	//		float *pEx = m_pRadAux->pBaseRadX + EXZ.aux_offset;
	//		*(EPtrs.pExRe) = *pEx; *(pEx++) = 0;
	//		*(EPtrs.pExIm) = *pEx; *pEx = 0;
	//	}
	//	if(m_pRadAux->pBaseRadZ != 0)
	//	{
	//		float *pEy = m_pRadAux->pBaseRadZ + EXZ.aux_offset;
	//		*(EPtrs.pEzRe) = *pEy; *(pEy++) = 0;
	//		*(EPtrs.pEzIm) = *pEy; *pEy = 0;
	//	}
	//}

	TVector3d vEr(*(EPtrs.pExRe), *(EPtrs.pEzRe), 0), vEi(*(EPtrs.pExIm), *(EPtrs.pEzIm), 0);
	//Maybe rather this?
	//TVector3d vEr(-*(EPtrs.pExRe), *(EPtrs.pEzRe), 0), vEi(-*(EPtrs.pExIm), *(EPtrs.pEzIm), 0);

	double xp = (EXZ.x - m_inWfrCh)/m_inWfrRh, yp = (EXZ.z - m_inWfrCv)/m_inWfrRv;
	TVector3d vRay(xp, yp, sqrt(1. - xp*xp - yp*yp)); //in the frame of incident beam

	TVector3d vNormAtP;
	FindSurfNormalInLocFrame(intersP.x, intersP.y, vNormAtP);
	vNormAtP = TransHndl.rep->TrBiPoint(vNormAtP);  //to the frame of incident beam

	TVector3d vSig = vNormAtP^vRay; vSig.Normalize(); //in the frame of incident beam
	TVector3d vPi = vRay^vSig;
	double EsigRe = vEr*vSig, EsigIm = vEi*vSig; //in the frame of incident beam
	double EpiRe = vEr*vPi, EpiIm = vEi*vPi;

	//getting complex reflecivity coefficients for Sigma and Pi components of the electric field
	int ne = m_reflData.DimSizes[1];
	double eStart = m_reflData.DimStartValues[1];
	double eStep = m_reflData.DimSteps[1];
	int nAng = m_reflData.DimSizes[2];
	double angStart = m_reflData.DimStartValues[2];
	double angStep = m_reflData.DimSteps[2];

	const long perSigPi = 2;
	const long perPhotEn = perSigPi << 1;
	long perAng = perPhotEn*ne;

	int ie = (int)((EXZ.e - eStart)/eStep + 0.00001);
	if((EXZ.e - (eStart + ie*eStep)) > 0.5*eStep) ie++;
	if(ie < 0) ie = 0;
	if(ie >= ne) ie = ne - 1;

	double sinAngInc = ::fabs(vRay*vNormAtP);
	double angInc = asin(sinAngInc);

	int iAng = (int)((angInc - angStart)/angStep + 0.00001);
	if((angInc - (angStart + iAng*angStep)) > 0.5*angStep) iAng++;
	if(iAng < 0) iAng = 0;
	if(iAng >= nAng) iAng = nAng - 1;

	long ofstSig = perPhotEn*ie + perAng*iAng;
	//long ofstPi = ofstSig + perSigPi;
	double RsigRe=1, RsigIm=0, RpiRe=1, RpiIm=0;

	//setting appropriate pointer type 
	if(m_reflData.pData != 0)
	{
		if(m_reflData.DataType[1] == 'f')
		{
			float *pRsig = ((float*)(m_reflData.pData)) + ofstSig;
			float *pRpi = pRsig + perSigPi;
			RsigRe = *(pRsig++); RsigIm = *pRsig;
			RpiRe = *(pRpi++); RpiIm = *pRpi;
		}
		else
		{
			double *pRsig = ((double*)(m_reflData.pData)) + ofstSig;
			double *pRpi = pRsig + perSigPi;
			RsigRe = *(pRsig++); RsigIm = *pRsig;
			RpiRe = *(pRpi++); RpiIm = *pRpi;
		}
	}

	double newEsigRe = -cosPh*(EsigIm*RsigIm - EsigRe*RsigRe) - sinPh*(EsigRe*RsigIm + EsigIm*RsigRe);
	double newEsigIm = cosPh*(EsigRe*RsigIm + EsigIm*RsigRe) - sinPh*(EsigIm*RsigIm - EsigRe*RsigRe);
	double newEpiRe = -cosPh*(EpiIm*RpiIm - EpiRe*RpiRe) - sinPh*(EpiRe*RpiIm + EpiIm*RpiRe);
	double newEpiIm = cosPh*(EpiRe*RpiIm + EpiIm*RpiRe) - sinPh*(EpiIm*RpiIm - EpiRe*RpiRe);

	vEr = newEsigRe*vSig + newEpiRe*vPi; //in the frame of incident beam
	vEi = newEsigIm*vSig + newEpiIm*vPi;

	//electric field components in the frame of output beam 
	//test
	*(EPtrs.pExRe) = (float)(vEr*m_vHorOutIn);
	*(EPtrs.pExIm) = (float)(vEi*m_vHorOutIn);
	*(EPtrs.pEzRe) = (float)(vEr*m_vVerOutIn);
	*(EPtrs.pEzIm) = (float)(vEi*m_vVerOutIn);
}

//*************************************************************************

void srTMirror::EstimateFocalLengths(double radTan, double radSag) //to make it virtual in srTFocusingElem?
{//Assumes that m_vCenNorm, m_vCenTang are set !
 //Estimates focal lengths (approximately!):
	double cosAng = ::fabs(m_vCenNorm.z);
	if(::fabs(m_vCenTang.x) < ::fabs(m_vCenTang.y))
	{//tangential plane is close to be vertical
		if(::fabs(m_vCenNorm.x) < ::fabs(m_vCenNorm.y))
		{//normal is turned in vertical direction
			//if(FocDistX == 0.) FocDistX = 0.5*radSag/cosAng; //focal length in horizontal plane
			//if(FocDistZ == 0.) FocDistZ = 0.5*radTan*cosAng; //focal length in vertical plane
			FocDistX = 0.5*radSag/cosAng; //focal length in horizontal plane
			FocDistZ = 0.5*radTan*cosAng; //focal length in vertical plane
		}
		else
		{//normal is turned in horizontal direction
			//if(FocDistX == 0.) FocDistX = 0.5*radSag*cosAng; //focal length in horizontal plane
			//if(FocDistZ == 0.) FocDistZ = 0.5*radTan/cosAng; //focal length in vertical plane
			FocDistX = 0.5*radSag*cosAng; //focal length in horizontal plane
			FocDistZ = 0.5*radTan/cosAng; //focal length in vertical plane
		}
	}
	else
	{//tangential plane is close to be horizontal
		if(::fabs(m_vCenNorm.x) < ::fabs(m_vCenNorm.y))
		{//normal is turned in vertical direction
			//if(FocDistX == 0.) FocDistX = 0.5*radTan/cosAng; //focal length in vertical plane
			//if(FocDistZ == 0.) FocDistZ = 0.5*radSag*cosAng; //focal length in vertical plane
			FocDistX = 0.5*radTan/cosAng; //focal length in vertical plane
			FocDistZ = 0.5*radSag*cosAng; //focal length in vertical plane
		}
		else
		{//normal is turned in horizontal direction
			//if(FocDistX == 0.) FocDistX = 0.5*radTan*cosAng; //focal length in vertical plane
			//if(FocDistZ == 0.) FocDistZ = 0.5*radSag/cosAng; //focal length in vertical plane
			FocDistX = 0.5*radTan*cosAng; //focal length in vertical plane
			FocDistZ = 0.5*radSag/cosAng; //focal length in vertical plane
		}
	}
}

//*************************************************************************

srTMirrorEllipsoid::srTMirrorEllipsoid(const SRWLOptMirEl& srwlMirEl) : srTMirror(srwlMirEl.baseMir)
{
	m_p = srwlMirEl.p;
	m_q = srwlMirEl.q;
	m_angGraz = srwlMirEl.angGraz;
	m_radSag = srwlMirEl.radSag;

	//Validate parameters: make sure all are positive
	if((m_p <= 0) || (m_q <= 0) || (m_angGraz <= 0) || (m_radSag <= 0))
	{ ErrorCode = IMPROPER_OPTICAL_COMPONENT_ELLIPSOID; return;} //throw here?

	//Determine ellipsoid parameters in Local frame
	DetermineEllipsoidParamsInLocFrame(); 

	//Estimate focal lengths:
	double pq = m_p*m_q;
	double radTan = sqrt(pq*pq*pq)/(m_ax*m_az);
	EstimateFocalLengths(radTan, m_radSag);
}

//*************************************************************************

srTMirrorToroid::srTMirrorToroid(srTStringVect* pMirInf, srTDataMD* pExtraData) : srTMirror(pMirInf, pExtraData)
{
	if((pMirInf == 0) || (pMirInf->size() < 5)) { ErrorCode = IMPROPER_OPTICAL_COMPONENT_STRUCTURE; return;}

	m_Rt = atof((*pMirInf)[2]);
	m_Rs = atof((*pMirInf)[3]);

	FocDistX = atof((*pMirInf)[8]);
	FocDistZ = atof((*pMirInf)[9]);
	if((FocDistX != 0.) && (FocDistZ != 0.)) return;

	//Estimating focal lengths (approximately!):
	EstimateFocalLengths(m_Rt, m_Rs);
}

//*************************************************************************

srTMirrorToroid::srTMirrorToroid(const SRWLOptMirTor& mirTor) : srTMirror(mirTor.baseMir)
{
	m_Rt = mirTor.radTan;
	m_Rs = mirTor.radSag;

	//Estimating focal lengths (approximately!):
	EstimateFocalLengths(m_Rt, m_Rs);
}

//*************************************************************************
//OBSOLETE?
srTThickMirrorGen::srTThickMirrorGen(srTStringVect* pElemInfo, srTDataMD* pExtraData) 
{
	if(pExtraData != 0) m_surfData = *pExtraData;

	char BufStr[256];

	TransvCenPoint.x = 0;
	strcpy(BufStr, (*pElemInfo)[4]); //$name[4]=num2str(xc)
	double aux_xc = atof(BufStr);
	if(::fabs(aux_xc) < 1.e+10) TransvCenPoint.x = aux_xc;

	TransvCenPoint.y = 0;
	strcpy(BufStr, (*pElemInfo)[5]); //$name[5]=num2str(yc)
	double aux_yc = atof(BufStr);
	if(::fabs(aux_yc) < 1.e+10) TransvCenPoint.y = aux_yc;

	m_apertShape = 1; //1- rectangular, 2- elliptical 
	strcpy(BufStr, (*pElemInfo)[6]); //$name[6]=num2str(apertShape)
	int iShape = atoi(BufStr);
	if((iShape > 0) && (iShape < 3)) m_apertShape = (char)iShape; //keep updated!

	m_ampReflectPerp = 1.;
	strcpy(BufStr, (*pElemInfo)[10]); //$name[10]=num2str(ampRefPerp)
	double aux_ampReflectPerp = atof(BufStr);
	if((aux_ampReflectPerp > 0) && (aux_ampReflectPerp < 1.)) m_ampReflectPerp = aux_ampReflectPerp;

	m_phaseShiftPerp = 0.;
	strcpy(BufStr, (*pElemInfo)[11]); //$name[11]=num2str(phShiftPerp)
	double aux_phaseShiftPerp = atof(BufStr);
	if(::fabs(aux_phaseShiftPerp) < 2.*3.141593) m_phaseShiftPerp = aux_phaseShiftPerp;

	m_ampReflectPar = 1.;
	strcpy(BufStr, (*pElemInfo)[12]); //$name[12]=num2str(ampRefPar)
	double aux_ampReflectPar = atof(BufStr);
	if((aux_ampReflectPar > 0) && (aux_ampReflectPar < 1.)) m_ampReflectPar = aux_ampReflectPar;

	m_phaseShiftPar = 0.;
	strcpy(BufStr, (*pElemInfo)[13]); //$name[13]=num2str(phShiftPar)
	double aux_phaseShiftPar = atof(BufStr);
	if(::fabs(aux_phaseShiftPar) < 2.*3.141593) m_phaseShiftPar = aux_phaseShiftPar;

	char m_axRot1 = 0;
	strcpy(BufStr, (*pElemInfo)[14]); //$name[14]=num2str(axRot1 - 1) //"0" means no rotation, "1" means vs "horizontal" axis, ...
	int aux_Rot = atoi(BufStr);
	if((aux_Rot > 0) && (aux_Rot < 4)) m_axRot1 = (char)aux_Rot; 

	strcpy(BufStr, (*pElemInfo)[15]); //$name[15]=num2str(angRot1)
	m_angRot1 = atof(BufStr);

	char m_axRot2 = 0;
	strcpy(BufStr, (*pElemInfo)[16]); //$name[16]=num2str(axRot2 - 1) //"0" means no rotation, "1" means vs "horizontal" axis, ...
	aux_Rot = atoi(BufStr);
	if((aux_Rot > 0) && (aux_Rot < 4)) m_axRot2 = (char)aux_Rot; 

	strcpy(BufStr, (*pElemInfo)[17]); //$name[17]=num2str(angRot2)
	m_angRot2 = atof(BufStr);

	char m_axRot3 = 0;
	strcpy(BufStr, (*pElemInfo)[18]); //$name[18]=num2str(axRot3 - 1) //"0" means no rotation, "1" means vs "horizontal" axis, ...
	aux_Rot = atoi(BufStr);
	if((aux_Rot > 0) && (aux_Rot < 4)) m_axRot3 = (char)aux_Rot; 

	strcpy(BufStr, (*pElemInfo)[19]); //$name[19]=num2str(angRot3)
	m_angRot3 = atof(BufStr);

	SetupNativeTransformation();


	//$name[7]="0" // Setup was finished or not
	//8 - foc. dist. x
	//9 - foc. dist. z
	strcpy(BufStr, (*pElemInfo)[7]); // Setup was completed or not
	int SetupIsCompleted = atoi(BufStr);
	if(SetupIsCompleted) 
	{
		strcpy(BufStr, (*pElemInfo)[8]);
		FocDistX = atof(BufStr);
		if(FocDistX == 0.) { ErrorCode = IMPROPER_OPTICAL_COMPONENT_STRUCTURE; return;}

		strcpy(BufStr, (*pElemInfo)[9]);
		FocDistZ = atof(BufStr);
		if(FocDistZ == 0.) { ErrorCode = IMPROPER_OPTICAL_COMPONENT_STRUCTURE; return;}
	}
	else
	{//Complete setup

/**
		if(ErrorCode = EstimateFocalDistancesAndCheckSampling()) return;

		//"erase" part of existing strings
		char *aStr=0;
		int AuxInfStartInd = 7;
		for(int k=AuxInfStartInd; k<(int)(pElemInfo->size()); k++)
		{
			aStr = (*pElemInfo)[k];
			//if(aStr != 0) delete[] aStr;
			if(aStr != 0) *aStr = '\0';
		}
		pElemInfo->erase(pElemInfo->begin() + AuxInfStartInd, pElemInfo->end());

		aStr = (*pElemInfo)[AuxInfStartInd];
		sprintf(aStr, "1");

		aStr = (*pElemInfo)[AuxInfStartInd + 1];
		sprintf(aStr, "%g", FocDistX);

		aStr = (*pElemInfo)[AuxInfStartInd + 2];
		sprintf(aStr, "%g", FocDistZ);

		aStr = (*pElemInfo)[AuxInfStartInd + 3];
		sprintf(aStr, "%d", OptPathOrPhase);
**/
	}

}

//*************************************************************************