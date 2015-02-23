#ifndef _GLGPUDATASET_H
#define _GLGPUDATASET_H

#include "io/GLDataset.h"

class GLGPUDataset : public GLDataset
{
public:
  GLGPUDataset();
  ~GLGPUDataset();
  
public:
  bool OpenDataFile(const std::string& filename); // file list
  bool OpenDataFileByPattern(const std::string& pattern); 
  void LoadTimeStep(int timestep, int slot=0);
  void RotateTimeSteps();
  void CloseDataFile();

  int NTimeSteps() const {return _filenames.size();}

  void PrintInfo() const;

private:
  bool OpenBDATDataFile(const std::string& filename, int slot=0);
  bool OpenLegacyDataFile(const std::string& filename, int slot=0);
  
protected:
  void Nid2Idx(NodeIdType id, int *idx) const; 
  NodeIdType Idx2Nid(int *idx) const;
 
  void Idx2Pos(const int idx[3], double X[3]) const;
  void Pos2Idx(const double X[3], int idx[3]) const;
  void Pos2Grid(const double pos[3], double gpos[3]) const; //!< to grid coordinates

public: // rectilinear grid
  const int* dims() const {return _dims;}
  const bool* pbc() const {return _pbc;}
  const double* CellLengths() const {return _cell_lengths;}

  double dx() const {return _cell_lengths[0];}
  double dy() const {return _cell_lengths[1];}
  double dz() const {return _cell_lengths[2];}
  
  // Magnetic potential
  bool A(const double X[3], double A[3], int slot=0) const;
  bool A(NodeIdType n, double A[3], int slot=0) const;
  // double Ax(const double X[3]) const {if (By()>0) return Kex(); else return -X[1]*Bz()+Kex();}
  double Ax(const double X[3], int slot=0) const {if (By()>0) return -Kex(slot); else return -X[1]*Bz()-Kex(slot);}
  // double Ax(const double X[3]) const {if (By()>0) return 0; else return -X[1]*Bz();}
  double Ay(const double X[3], int slot=0) const {if (By()>0) return X[0]*Bz(); else return 0;}
  double Az(const double X[3], int slot=0) const {if (By()>0) return -X[0]*By(); else return X[1]*Bx();}
  
  // Magnetic field
  void SetMagneticField(const double B[3]) {_B[0]=B[0]; _B[1]=B[1]; _B[2]=B[2];}
  const double* B() const {return _B;}
  double Bx() const {return _B[0];} 
  double By() const {return _B[1];} 
  double Bz() const {return _B[2];}

public:
  double QP(const double X0[], const double X1[]) const;

protected:
  int _dims[3]; 
  bool _pbc[3]; 
  double _cell_lengths[3]; 

  double _B[3]; // magnetic field

  double *_re, *_im, 
         *_re1, *_im1;
  double *_Jx, *_Jy, *_Jz; // only for timestep 0

  std::vector<std::string> _filenames; // filenames for different timesteps
};

#endif
