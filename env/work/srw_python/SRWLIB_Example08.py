# -*- coding: utf-8 -*-
#############################################################################
# SRWLIB Example#8: Simulating partially-coherent UR focusing with a CRL
# v 0.04
#############################################################################

from __future__ import print_function #Python 2.7 compatibility
from srwlib import *
import os
#import sys

print('SRWLIB Python Example # 8:')
print('Simulating emission and propagation of undulator radiation (UR) wavefront through a simple optical scheme including CRL')
print('')
print('First, single-electron UR (on-axis spectrum and a wavefront at a fixed photon energy) is calculated and propagated through the optical scheme. ', end='')
print('After this, calculation of partially-coherent UR from entire electron beam is started as a loop over "macro-electrons", using "srwl_wfr_emit_prop_multi_e" function. ', end='')
print('This function can run either in "normal" sequential mode, or in parallel mode under "mpi4py".', end='')
print('For this, an MPI2 package and the "mpi4py" Python package have to be installed and configured, and this example has to be started e.g. as:')
print('    mpiexec -n 5 python SRWLIB_Example08.py')
print('For more information on parallel calculations under "mpi4py" please see documentation to the "mpi4py" and MPI.')
print('Note that the long-lasting partially-coherent UR calculation saves from time to time instant average intensity to an ASCII file, ', end='')
print('so the execution of the long loop over "macro-electrons" can be aborted after some time without the danger that all results will be lost.')
print('')

#**********************Auxiliary function to write tabulated resulting Intensity data to ASCII file:
def AuxSaveIntData(arI, mesh, filePath):
    f = open(filePath, 'w')
    f.write('#C-aligned Intensity (inner loop is vs photon energy, outer loop vs vertical position)\n')
    f.write('#' + repr(mesh.eStart) + ' #Initial Photon Energy [eV]\n')
    f.write('#' + repr(mesh.eFin) + ' #Final Photon Energy [eV]\n')
    f.write('#' + repr(mesh.ne) + ' #Number of points vs Photon Energy\n')
    f.write('#' + repr(mesh.xStart) + ' #Initial Horizontal Position [m]\n')
    f.write('#' + repr(mesh.xFin) + ' #Final Horizontal Position [m]\n')
    f.write('#' + repr(mesh.nx) + ' #Number of points vs Horizontal Position\n')
    f.write('#' + repr(mesh.yStart) + ' #Initial Vertical Position [m]\n')
    f.write('#' + repr(mesh.yFin) + ' #Final Vertical Position [m]\n')
    f.write('#' + repr(mesh.ny) + ' #Number of points vs Vertical Position\n')
    for i in range(mesh.ne*mesh.nx*mesh.ny): #write all data into one column using "C-alignment" as a "flat" 1D array
        f.write(' ' + repr(arI[i]) + '\n')
    f.close()

#**********************Auxiliary function to write Optical Transmission characteristic data to ASCII file:
def AuxSaveOpTransmData(optTr, t, filePath):
    f = open(filePath, 'w')
    f.write('#C-aligned optical Transmission characteristic (inner loop is vs horizontal position, outer loop vs vertical position)\n')
    f.write('#' + repr(optTr.mesh.eStart) + ' #Initial Photon Energy [eV]\n')
    f.write('#' + repr(optTr.mesh.eFin) + ' #Final Photon Energy [eV]\n')
    f.write('#' + repr(optTr.mesh.ne) + ' #Number of points vs Photon Energy\n')
    f.write('#' + repr(optTr.mesh.xStart) + ' #Initial Horizontal Position [m]\n')
    f.write('#' + repr(optTr.mesh.xFin) + ' #Final Horizontal Position [m]\n')
    f.write('#' + repr(optTr.mesh.nx) + ' #Number of points vs Horizontal Position\n')
    f.write('#' + repr(optTr.mesh.yStart) + ' #Initial Vertical Position [m]\n')
    f.write('#' + repr(optTr.mesh.yFin) + ' #Final Vertical Position [m]\n')
    f.write('#' + repr(optTr.mesh.ny) + ' #Number of points vs Vertical Position\n')
    neLoc = 1
    if(optTr.mesh.ne > 1):
        neLoc = optTr.mesh.ne
    for i in range(neLoc*optTr.mesh.nx*optTr.mesh.ny): #write all data into one column using "C-alignment" as a "flat" 1D array
        tr = 0
        if((t == 1) or (t == 2)): #amplitude or intensity transmission
            tr = optTr.arTr[i*2]
            if(t == 2): #intensity transmission
                tr *= tr
        else: #optical path difference
            tr = optTr.arTr[i*2 + 1]
        f.write(' ' + repr(tr) + '\n')
    f.close()

#def AuxSaveOpTransmData(optTr, t, filePath):
#    f = open(filePath, 'w')
#    f.write('#C-aligned optical Transmission characteristic (inner loop is vs horizontal position, outer loop vs vertical position)\n')
#    f.write('#' + repr(1) + ' #Reserved for Initial Photon Energy [eV]\n')
#    f.write('#' + repr(1) + ' #Reserved for Final Photon Energy [eV]\n')
#    f.write('#' + repr(1) + ' #Reserved for Number of points vs Photon Energy\n')
#    f.write('#' + repr(optTr.x - 0.5*optTr.rx) + ' #Initial Horizontal Position [m]\n')
#    f.write('#' + repr(optTr.x + 0.5*optTr.rx) + ' #Final Horizontal Position [m]\n')
#    f.write('#' + repr(optTr.nx) + ' #Number of points vs Horizontal Position\n')
#    f.write('#' + repr(optTr.y - 0.5*optTr.ry) + ' #Initial Vertical Position [m]\n')
#    f.write('#' + repr(optTr.y + 0.5*optTr.ry) + ' #Final Vertical Position [m]\n')
#    f.write('#' + repr(optTr.ny) + ' #Number of points vs Vertical Position\n')
#    for i in range(optTr.nx*optTr.ny): #write all data into one column using "C-alignment" as a "flat" 1D array
#        tr = 0
#        if((t == 1) or (t == 2)): #amplitude or intensity transmission
#            tr = optTr.arTr[i*2]
#            if(t == 2): #intensity transmission
#                tr *= tr
#        else: #optical path difference
#            tr = optTr.arTr[i*2 + 1]
#        f.write(' ' + repr(tr) + '\n')
#    f.close()


#**********************Input Parameters:
strExDataFolderName = 'data_example_08' #example data sub-folder name
strTrajOutFileName = 'wfr_res_traj.dat' #file name for output trajectory data
strIntOutFileName1 = 'wfr_res_crl_int1.dat' #file name for output SR intensity data
strIntOutFileName2 = 'wfr_res_crl_int2.dat' #file name for output SR intensity data
strIntOutFileName3 = 'wfr_res_crl_int3.dat' #file name for output SR intensity data
strIntOutFileNamePartCoh = 'wfr_res_crl_int_part_coh1.dat' #file name for output SR intensity data

#***********Undulator
numPer = 72.5 #Number of ID Periods (without counting for terminations
undPer = 0.033 #Period Length [m]
Bx = 0 #Peak Horizontal field [T]
By = 0.3545 #Peak Vertical field [T]
phBx = 0 #Initial Phase of the Horizontal field component
phBy = 0 #Initial Phase of the Vertical field component
sBx = 1 #Symmetry of the Horizontal field component vs Longitudinal position
sBy = -1 #Symmetry of the Vertical field component vs Longitudinal position
xcID = 0 #Transverse Coordinates of Undulator Center [m]
ycID = 0
zcID = 1.25 #0 #Longitudinal Coordinate of Undulator Center wit hrespect to Straight Section Center [m]

und = SRWLMagFldU([SRWLMagFldH(1, 'v', By, phBy, sBy, 1), SRWLMagFldH(1, 'h', Bx, phBx, sBx, 1)], undPer, numPer) #Ellipsoidal Undulator
magFldCnt = SRWLMagFldC([und], array('d', [xcID]), array('d', [ycID]), array('d', [zcID])) #Container of all Field Elements

#***********Electron Beam
elecBeam = SRWLPartBeam()
elecBeam.Iavg = 0.1 #Average Current [A]
elecBeam.partStatMom1.x = 0. #Initial Transverse Coordinates (initial Longitudinal Coordinate will be defined later on) [m]
elecBeam.partStatMom1.y = 0.
elecBeam.partStatMom1.z = 0. #-0.5*undPer*(numPer + 4) #Initial Longitudinal Coordinate (set before the ID)
elecBeam.partStatMom1.xp = 0 #Initial Relative Transverse Velocities
elecBeam.partStatMom1.yp = 0
elecBeam.partStatMom1.gamma = 7./0.51099890221e-03 #Relative Energy
#2nd order statistical moments
elecBeam.arStatMom2[0] = (118.027e-06)**2 #<(x-x0)^2>
elecBeam.arStatMom2[1] = 0
elecBeam.arStatMom2[2] = (27.3666e-06)**2 #<(x'-x'0)^2>
elecBeam.arStatMom2[3] = (15.4091e-06)**2 #<(y-y0)^2>
elecBeam.arStatMom2[4] = 0
elecBeam.arStatMom2[5] = (2.90738e-06)**2 #<(y'-y'0)^2>
elecBeam.arStatMom2[10] = (1e-03)**2 #<(E-E0)^2>/E0^2

#***********Precision Parameters for SR calculation
meth = 1 #SR calculation method: 0- "manual", 1- "auto-undulator", 2- "auto-wiggler"
relPrec = 0.01 #relative precision
zStartInteg = 0 #longitudinal position to start integration (effective if < zEndInteg)
zEndInteg = 0 #longitudinal position to finish integration (effective if > zStartInteg)
npTraj = 20000 #Number of points for trajectory calculation 
useTermin = 1 #Use "terminating terms" (i.e. asymptotic expansions at zStartInteg and zEndInteg) or not (1 or 0 respectively)
sampFactNxNyForProp = 0.25 #sampling factor for adjusting nx, ny (effective if > 0)
arPrecPar = [meth, relPrec, zStartInteg, zEndInteg, npTraj, useTermin, 0]

#*********** Spectrum
wfr1 = SRWLWfr() #For spectrum vs photon energy
wfr1.allocate(10000, 1, 1) #Numbers of points vs Photon Energy, Horizontal and Vertical Positions
wfr1.mesh.zStart = 36.25 + 1.25 #Longitudinal Position [m] from Center of Straight Section at which SR has to be calculated
wfr1.mesh.eStart = 1000. #Initial Photon Energy [eV]
wfr1.mesh.eFin = 10000. #Final Photon Energy [eV]
wfr1.mesh.xStart = 0. #Initial Horizontal Position [m]
wfr1.mesh.xFin = 0 #Final Horizontal Position [m]
wfr1.mesh.yStart = 0 #Initial Vertical Position [m]
wfr1.mesh.yFin = 0 #Final Vertical Position [m]
wfr1.partBeam = elecBeam

#****************** Initial Wavefront
wfr2 = SRWLWfr() #For intensity distribution at fixed photon energy
wfr2.allocate(1, 101, 101) #Numbers of points vs Photon Energy, Horizontal and Vertical Positions
wfr2.mesh.zStart = 36.25 + 1.25 #Longitudinal Position [m] from Center of Straight Section at which SR has to be calculated
wfr2.mesh.eStart = 8830 #Initial Photon Energy [eV]
wfr2.mesh.eFin = 8830 #Final Photon Energy [eV]
wfr2.mesh.xStart = -0.0015 #Initial Horizontal Position [m]
wfr2.mesh.xFin = 0.0015 #Final Horizontal Position [m]
wfr2.mesh.yStart = -0.0006 #Initial Vertical Position [m]
wfr2.mesh.yFin = 0.0006 #Final Vertical Position [m]
meshInitPartCoh = deepcopy(wfr2.mesh)

wfr2.partBeam = elecBeam

#***************** Optical Elements and Propagation Parameters
fx = 1e+23 #Focal length in Horizontal plane
fy = 19.0939 #Focal length in Horizontal plane
optLens = SRWLOptL(fx, fy) #Ideal Lens

delta = 4.3712962E-06 #Refractive index decrement of Be at 8830 eV
attenLen = 6946.13E-06 #[m] Attenuation length of Be at 8830 eV
geomApertF = 1E-03 #[m] Geometrical aparture of 1D CRL in the Focusing plane
geomApertNF = 3E-03 #[m] Geometrical aparture of 1D CRL in the plane where there is no focusing
rMin = 0.5E-03 #[m] radius at tip of parabola of CRL
nCRL = 3
wallThick = 50E-06 #[m] wall thickness of CRL

optCRL = srwl_opt_setup_CRL(2, delta, attenLen, 1, geomApertNF, geomApertF, rMin, nCRL, wallThick, 0, 0) #1D CRL
print('   Saving CRL transmission data to files (for viewing/debugging)...', end='')
AuxSaveOpTransmData(optCRL, 2, os.path.join(os.getcwd(), strExDataFolderName, 'res_op_transm_CRL.dat'))
AuxSaveOpTransmData(optCRL, 3, os.path.join(os.getcwd(), strExDataFolderName, 'res_op_path_dif_CRL.dat'))
print('done')

optApert = SRWLOptA('r', 'a', geomApertNF, geomApertF) #Aperture

optDrift = SRWLOptD(38.73) #Drift space

propagParApert = [0, 0, 1., 0, 0, 1.5, 1., 1.1, 8., 0, 0, 0]
propagParLens = [0, 0, 1., 0, 0, 1., 1., 1., 1., 0, 0, 0]
propagParDrift = [0, 0, 1., 1, 0, 1., 1.2, 1., 1., 0, 0, 0]

#Wavefront Propagation Parameters:
#[0]: Auto-Resize (1) or not (0) Before propagation
#[1]: Auto-Resize (1) or not (0) After propagation
#[2]: Relative Precision for propagation with Auto-Resizing (1. is nominal)
#[3]: Allow (1) or not (0) for semi-analytical treatment of the quadratic (leading) phase terms at the propagation
#[4]: Do any Resizing on Fourier side, using FFT, (1) or not (0)
#[5]: Horizontal Range modification factor at Resizing (1. means no modification)
#[6]: Horizontal Resolution modification factor at Resizing
#[7]: Vertical Range modification factor at Resizing
#[8]: Vertical Resolution modification factor at Resizing
#[9]: Type of wavefront Shift before Resizing (not yet implemented)
#[10]: New Horizontal wavefront Center position after Shift (not yet implemented)
#[11]: New Vertical wavefront Center position after Shift (not yet implemented)
optBL = SRWLOptC([optApert, optCRL, optDrift], [propagParApert, propagParLens, propagParDrift]) #"Beamline" - Container of Optical Elements (together with the corresponding wavefront propagation instructions)
#optBL = SRWLOptC([optApert, optLens, optDrift], [propagParApert, propagParLens, propagParDrift]) #"Beamline" - Container of Optical Elements (together with the corresponding wavefront propagation instructions)

#**********************Calculation (SRWLIB function calls)
if(srwl_uti_proc_is_master()):
    print('   Performing Electric Field Wavefront calculation ... ', end='')
    srwl.CalcElecFieldSR(wfr1, 0, magFldCnt, arPrecPar)
    print('done')
    print('   Extracting Intensity from the Calculated Electric Field ... ', end='')
    arI1 = array('f', [0]*wfr1.mesh.ne)
    srwl.CalcIntFromElecField(arI1, wfr1, 6, 0, 0, wfr1.mesh.eStart, wfr1.mesh.xStart, wfr1.mesh.yStart)
    print('done')
    print('   Saving the radiation spectrum data to a file ... ', end='')
    AuxSaveIntData(arI1, wfr1.mesh, os.path.join(os.getcwd(), strExDataFolderName, strIntOutFileName1))
    print('done')

    print('   Performing Initial Electric Field calculation ... ', end='')
    arPrecPar[6] = sampFactNxNyForProp #sampling factor for adjusting nx, ny (effective if > 0)
    srwl.CalcElecFieldSR(wfr2, 0, magFldCnt, arPrecPar)
    print('done')
    print('   Extracting Intensity from the Calculated Initial Electric Field ... ', end='')
    arI2 = array('f', [0]*wfr2.mesh.nx*wfr2.mesh.ny) #"flat" array to take 2D intensity data
    srwl.CalcIntFromElecField(arI2, wfr2, 6, 0, 3, wfr2.mesh.eStart, 0, 0)
    print('done')
    print('   Saving the Initial Electric Field into a file ... ', end='')
    AuxSaveIntData(arI2, wfr2.mesh, os.path.join(os.getcwd(), strExDataFolderName, strIntOutFileName2))
    print('done')

    print('   Simulating Electric Field Wavefront Propagation ... ', end='')
    srwl.PropagElecField(wfr2, optBL)
    print('done')
    print('   Extracting Intensity from the Propagated Electric Field  ... ', end='')
    arI3 = array('f', [0]*wfr2.mesh.nx*wfr2.mesh.ny) #"flat" 2D array to take intensity data
    srwl.CalcIntFromElecField(arI3, wfr2, 6, 0, 3, wfr2.mesh.eStart, 0, 0)
    print('done')
    print('   Saving the Propagated Wavefront Intensity data to a file ... ', end='')
    AuxSaveIntData(arI3, wfr2.mesh, os.path.join(os.getcwd(), strExDataFolderName, strIntOutFileName3))
    print('done')

#sys.exit(0)

print('   Simulating Partially-Coherent Wavefront Propagation by summing-up contributions of SR from individual electrons (takes time)... ')
nMacroElec = 50000
radStokesProp = srwl_wfr_emit_prop_multi_e(elecBeam, magFldCnt, meshInitPartCoh, 1, 0.01, nMacroElec, 5, 10, os.path.join(os.getcwd(), strExDataFolderName, strIntOutFileNamePartCoh), sampFactNxNyForProp, optBL)
print('done')

