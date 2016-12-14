#ifndef _DEF_H
#define _DEF_H

#cmakedefine WITH_CUDA 1
#cmakedefine WITH_ARMA 1
#cmakedefine WITH_LIBMESH 1
#cmakedefine WITH_NETCDF 1
#cmakedefine WITH_PNETCDF 1
#cmakedefine WITH_QT 1
#cmakedefine WITH_VTK 1
#cmakedefine WITH_PARAVIEW 1
#cmakedefine WITH_TBB 1
#cmakedefine WITH_ROCKSDB 1
#cmakedefine WITH_FORTRAN 1

// needed for malloc
#include <cstdlib>

// define ElemIdType
#if WITH_LIBMESH
#include <libmesh/id_types.h>
typedef libMesh::dof_id_type NodeIdType;
typedef libMesh::dof_id_type EdgeIdType;
typedef libMesh::dof_id_type FaceIdType;
typedef libMesh::dof_id_type CellIdType;
typedef libMesh::dof_id_type ElemIdType;
#else
typedef unsigned int NodeIdType;
typedef unsigned int EdgeIdType;
typedef unsigned int FaceIdType;
typedef unsigned int CellIdType;
typedef unsigned int ElemIdType;
#endif

typedef signed char ChiralityType;

enum {
  GLGPU3D_MESH_HEX,
  GLGPU3D_MESH_TET
};

// NetCDF error handling
#define NC_SAFE_CALL(call) {\
  int retval = call;\
  if (retval != 0) {\
      fprintf(stderr, "[NetCDF Error] %s, in file '%s', line %i.\n", nc_strerror(retval), __FILE__, __LINE__); \
      exit(EXIT_FAILURE); \
  }\
}

// PNetCDF error handling
#define PNC_SAFE_CALL(call) {\
  int retval = call;\
  if (retval != 0) {\
      fprintf(stderr, "[PNetCDF Error] %s, in file '%s', line %i.\n", ncmpi_strerror(retval), __FILE__, __LINE__); \
      exit(EXIT_FAILURE); \
  }\
}

// CUDA error handling
#define CUDA_SAFE_CALL(call) {\
  cudaError_t err = call;\
  if (cudaSuccess != err) {\
    fprintf(stderr, "[CUDA Error] %s, in file '%s', line%i.\n", cudaGetErrorString(err), __FILE__, __LINE__); \
    exit(EXIT_FAILURE); \
  }\
}

// OpenGL error handling
#define RESET_GLERROR()\
{\
  while (glGetError() != GL_NO_ERROR) {}\
}

#define CHECK_GLERROR()\
{\
  GLenum err = glGetError();\
  if (err != GL_NO_ERROR) {\
    const GLubyte *errString = gluErrorString(err);\
    qDebug("[%s line %d] GL Error: %s\n",\
            __FILE__, __LINE__, errString);\
  }\
}

// mem alloc for 2D arrays
#define malloc2D(name, xDim, yDim, type) do {               \
    name = (type **)malloc(xDim * sizeof(type *));          \
    assert(name != NULL);                                   \
    name[0] = (type *)malloc(xDim * yDim * sizeof(type));   \
    assert(name[0] != NULL);                                \
    for (size_t i = 1; i < xDim; i++)                       \
        name[i] = name[i-1] + yDim;                         \
} while (0)

#endif
