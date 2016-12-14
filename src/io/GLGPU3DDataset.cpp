#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cmath>
#include <cstring>
#include <climits>
#include <iostream>
#include "common/Utils.hpp"
#include "common/Lerp.hpp"
#include "common/MeshGraphRegular3D.h"
#include "common/MeshGraphRegular3DTets.h"
#include "GLGPU3DDataset.h"

GLGPU3DDataset::GLGPU3DDataset() :
  _mesh_type(GLGPU3D_MESH_HEX)
{
}

GLGPU3DDataset::~GLGPU3DDataset()
{
}

void GLGPU3DDataset::SetMeshType(int t)
{
  _mesh_type = t;
}

void GLGPU3DDataset::BuildMeshGraph()
{
  if (_mesh_type == GLGPU3D_MESH_TET)
    _mg = new class MeshGraphRegular3DTets(_h[0].dims, _h[0].pbc);
  else if (_mesh_type == GLGPU3D_MESH_HEX)
    _mg = new class MeshGraphRegular3D(_h[0].dims, _h[0].pbc);
  else assert(false);
}

std::vector<FaceIdType> GLGPU3DDataset::GetBoundaryFaceIds(int type) const
{
  if (_mesh_type == GLGPU3D_MESH_HEX) {
    const struct MeshGraphRegular3D *mg = (const MeshGraphRegular3D*)_mg;
    return mg->GetBoundaryFaceIds(type);
  }
  else 
    return std::vector<FaceIdType>();
}

#if 0
float GLGPU3DDataset::Flux(int face) const
{
  // TODO: pre-compute the flux
  switch (face) {
  case 0: return -dx() * dy() * Bz();
  case 1: return -dy() * dz() * Bx(); 
  case 2: return -dz() * dx() * By(); 
  case 3: return  dx() * dy() * Bz(); 
  case 4: return  dy() * dz() * Bx(); 
  case 5: return  dz() * dx() * By();
  default: assert(false);
  }
  return 0.0;
}

float GLGPU3DDataset::GaugeTransformation(const float X0[], const float X1[]) const
{
  float gx, gy, gz; 
  float dx = X1[0] - X0[0], 
         dy = X1[1] - X0[1], 
         dz = X1[2] - X0[2]; 
  float x = X0[0] + 0.5*dx, 
         y = X0[1] + 0.5*dy, 
         z = X0[2] + 0.5*dz;

  if (By()>0) { // Y-Z gauge
    gx = dx * Kex(); 
    gy =-dy * x * Bz(); // -dy*x^hat*Bz
    gz = dz * x * By(); //  dz*x^hat*By
  } else { // X-Z gauge
    gx = dx * y * Bz() + dx * Kex(); //  dx*y^hat*Bz + dx*K
    gy = 0; 
    gz =-dz * y * Bx(); // -dz*y^hat*Bx
  }

  return gx + gy + gz; 
}
#endif

#if 0
std::vector<ElemIdType> GLGPU3DDataset::GetNeighborIds(ElemIdType elem_id) const
{
  std::vector<ElemIdType> neighbors; 

  int idx[3], idx1[3];
  ElemId2Idx(elem_id, idx); 

  for (int face=0; face<6; face++) {
    switch (face) {
    case 0: idx1[0] = idx[0];   idx1[1] = idx[1];   idx1[2] = idx[2]-1; break; 
    case 1: idx1[0] = idx[0]-1; idx1[1] = idx[1];   idx1[2] = idx[2];   break;
    case 2: idx1[0] = idx[0];   idx1[1] = idx[1]-1; idx1[2] = idx[2];   break;
    case 3: idx1[0] = idx[0];   idx1[1] = idx[1];   idx1[2] = idx[2]+1; break; 
    case 4: idx1[0] = idx[0]+1; idx1[1] = idx[1];   idx1[2] = idx[2];   break;
    case 5: idx1[0] = idx[0];   idx1[1] = idx[1]+1; idx1[2] = idx[2];   break;
    default: break;
    }

#if 0 // pbc
    for (int i=0; i<3; i++) 
      if (pbc()[i]) {
        idx1[i] = idx1[i] % dims()[i]; 
        if (idx1[i]<0) idx1[i] += dims()[i];
      }
#endif
    
    neighbors.push_back(Idx2ElemId(idx1)); 
  }

  return neighbors; 
}
#endif

#if 0
void GLGPU3DDataset::ComputeSupercurrentField(int slot)
{
  const int nvoxels = dims()[0]*dims()[1]*dims()[2];

  if (_J[slot] != NULL) free(_J[slot]);
  _J[slot] = (float*)malloc(3*sizeof(float)*nvoxels);

  float *J = _J[slot];
  memset(J, 0, 3*sizeof(float)*nvoxels);
 
  float u, v, rho2;
  float du[3], dv[3], A[3], j[3];

  // central difference
  for (int x=1; x<dims()[0]-1; x++) {
    for (int y=1; y<dims()[1]-1; y++) {
      for (int z=1; z<dims()[2]-1; z++) {
        int idx[3] = {x, y, z}; 
        float pos[3]; 
        Idx2Pos(idx, pos);

        du[0] = 0.5 * (Re(x+1, y, z, slot) - Re(x-1, y, z, slot)) / dx();
        du[1] = 0.5 * (Re(x, y+1, z, slot) - Re(x, y-1, z, slot)) / dy();
        du[2] = 0.5 * (Re(x, y, z+1, slot) - Re(x, y, z-1, slot)) / dz();
        
        dv[0] = 0.5 * (Im(x+1, y, z, slot) - Im(x-1, y, z, slot)) / dx();
        dv[1] = 0.5 * (Im(x, y+1, z, slot) - Im(x, y-1, z, slot)) / dy();
        dv[2] = 0.5 * (Im(x, y, z+1, slot) - Im(x, y, z-1, slot)) / dz();

        this->A(pos, A, slot);

        u = Re(x, y, z, slot); 
        v = Im(x, y, z, slot);
        rho2 = u*u + v*v;

        texel3Dv(J, dims(), 3, x, y, z, 0) = j[0] = (u*dv[0] - v*du[0]) - rho2*A[0]; 
        texel3Dv(J, dims(), 3, x, y, z, 1) = j[1] = (u*dv[1] - v*du[1]) - rho2*A[1];
        texel3Dv(J, dims(), 3, x, y, z, 2) = j[2] = (u*dv[2] - v*du[2]) - rho2*A[2];

        // fprintf(stderr, "J={%f, %f, %f}\n", j[0], j[1], j[2]);
      }
    }
  }
}
#endif

bool GLGPU3DDataset::Psi(const float X[3], float &re, float &im) const
{
  // TODO
  return false;
}

bool GLGPU3DDataset::Supercurrent(const float X[3], float J[3], int slot) const
{
  static const int st[3] = {0};
  float gpt[3];
  const float *j[3] = {_Jx[slot], _Jy[slot], _Jz[slot]};
 
  Pos2Grid(X, gpt);
  if (isnan(gpt[0]) || gpt[0]<=1 || gpt[0]>dims()[0]-2 || 
      isnan(gpt[1]) || gpt[1]<=1 || gpt[1]>dims()[1]-2 || 
      isnan(gpt[2]) || gpt[2]<=1 || gpt[2]>dims()[2]-2) return false;

  if (!lerp3D(gpt, st, dims(), 3, j, J))
    return false;
  else return true;
}

CellIdType GLGPU3DDataset::Pos2CellId(const float X[]) const
{
  // TODO
  return false;
}
