#include "vtkObjectFactory.h"
#include "vtkSmartPointer.h"
#include "vtkImageData.h"
#include "vtkCellData.h"
#include "vtkPointData.h"
#include "vtkFieldData.h"
#include "vtkRectilinearGrid.h"
#include "vtkStructuredGrid.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkStreamingDemandDrivenPipeline.h"
#include "vtkBDATSeriesReader.h"
#include "io/GLGPU_IO_Helper.h"

vtkStandardNewMacro(vtkBDATSeriesReader);

vtkBDATSeriesReader::vtkBDATSeriesReader()
{
  SetNumberOfInputPorts(0);
  SetNumberOfOutputPorts(1);
}

vtkBDATSeriesReader::~vtkBDATSeriesReader()
{
}

int vtkBDATSeriesReader::RequestInformation(
    vtkInformation*, 
    vtkInformationVector**, 
    vtkInformationVector* outVec)
{
#if 0
  vtkInformation *outInfo = outVec->GetInformationObject(0);

  int ndims; 
  int dims[3];
  bool pbc[3];
  double origins[3], lengths[3], cellLengths[3], B[3];
  double time, Jxext, Kx, V;
  
  bool succ = false;
  if (!succ) {
    succ = GLGPU_IO_Helper_ReadBDAT(
        FileName, ndims, dims, lengths, pbc, 
        time, B, Jxext, Kx, V, NULL, NULL, true); // header only
  } 
  if (!succ) {
    succ = GLGPU_IO_Helper_ReadLegacy(
        FileName, ndims, dims, lengths, pbc, 
        time, B, Jxext, Kx, V, NULL, NULL, true);
  }
 
  for (int i=0; i<ndims; i++) {
    origins[i] = -0.5*lengths[i];
    cellLengths[i] = lengths[i] / (dims[i]-1);
  }

  int ext[6] = {0, dims[0]-1, 0, dims[1]-1, 0, dims[2]-1};

  outInfo->Set(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), ext, 6);
  outInfo->Set(vtkDataObject::SPACING(), cellLengths, 3);
  outInfo->Set(vtkDataObject::ORIGIN(), origins, 3);
  // vtkDataObject::SetPointDataActiveScalarInfo(outInfo, VTK_DOUBLE, 1);
#endif
  return 1;
}

int vtkBDATSeriesReader::RequestData(
    vtkInformation*, 
    vtkInformationVector**, 
    vtkInformationVector* outVec)
{
#if 0
  // load the data
  int ndims; 
  int dims[3];
  bool pbc[3];
  double origins[3], lengths[3], cellLengths[3], B[3];
  double time, Jxext, Kx, V;
  double *re=NULL, *im=NULL;
  double *rho=NULL, *phi=NULL;

  bool succ = false;
  if (!succ) {
    succ = GLGPU_IO_Helper_ReadBDAT(
        FileName, ndims, dims, lengths, pbc, 
        time, B, Jxext, Kx, V, &re, &im);
  }
  if (!succ) {
    succ = GLGPU_IO_Helper_ReadLegacy(
        FileName, ndims, dims, lengths, pbc, 
        time, B, Jxext, Kx, V, &re, &im);
  }
  if (!succ || ndims!=3)
  {
    vtkErrorMacro("Error opening file " << FileName);
    return 0;
  }

  for (int i=0; i<ndims; i++) {
    origins[i] = -0.5*lengths[i];
    cellLengths[i] = lengths[i] / (dims[i]-1);
  }
    
  // vtk data structures
  vtkInformation *outInfo = outVec->GetInformationObject(0);
  vtkImageData *imageData = 
    vtkImageData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));
  imageData->SetDimensions(dims[0], dims[1], dims[2]);
  // imageData->AllocateScalars(VTK_DOUBLE, 1);

  // copy data
  const int arraySize = dims[0]*dims[1]*dims[2];
  vtkSmartPointer<vtkDataArray> dataArrayRe, dataArrayIm, dataArrayRho, dataArrayPhi;

  rho = (double*)malloc(sizeof(double)*arraySize);
  phi = (double*)malloc(sizeof(double)*arraySize);
  for (int i=0; i<arraySize; i++) {
    rho[i] = sqrt(re[i]*re[i] + im[i]*im[i]);
    phi[i] = atan2(im[i], re[i]);
  }
  
  dataArrayRho.TakeReference(vtkDataArray::CreateDataArray(VTK_DOUBLE));
  dataArrayRho->SetNumberOfComponents(1); 
  dataArrayRho->SetNumberOfTuples(arraySize);
  dataArrayRho->SetName("rho");
  memcpy(dataArrayRho->GetVoidPointer(0), rho, sizeof(double)*arraySize);
  
  dataArrayPhi.TakeReference(vtkDataArray::CreateDataArray(VTK_DOUBLE));
  dataArrayPhi->SetNumberOfComponents(1); 
  dataArrayPhi->SetNumberOfTuples(arraySize);
  dataArrayPhi->SetName("phi");
  memcpy(dataArrayPhi->GetVoidPointer(0), phi, sizeof(double)*arraySize);

  dataArrayRe.TakeReference(vtkDataArray::CreateDataArray(VTK_DOUBLE));
  dataArrayRe->SetNumberOfComponents(1); 
  dataArrayRe->SetNumberOfTuples(arraySize);
  dataArrayRe->SetName("re");
  memcpy(dataArrayRe->GetVoidPointer(0), re, sizeof(double)*arraySize);
  
  dataArrayIm.TakeReference(vtkDataArray::CreateDataArray(VTK_DOUBLE));
  dataArrayIm->SetNumberOfComponents(1); 
  dataArrayIm->SetNumberOfTuples(arraySize);
  dataArrayIm->SetName("im");
  memcpy(dataArrayIm->GetVoidPointer(0), im, sizeof(double)*arraySize);

  imageData->GetPointData()->AddArray(dataArrayRho);
  imageData->GetPointData()->AddArray(dataArrayPhi);
  imageData->GetPointData()->AddArray(dataArrayRe);
  imageData->GetPointData()->AddArray(dataArrayIm);

  // global attributes
  vtkSmartPointer<vtkDataArray> dataArrayB, dataArrayPBC, dataArrayJxext, dataArrayKx, dataArrayV;
  
  dataArrayB.TakeReference(vtkDataArray::CreateDataArray(VTK_DOUBLE));
  dataArrayB->SetNumberOfComponents(3);
  dataArrayB->SetNumberOfTuples(1);
  dataArrayB->SetName("B");
  dataArrayB->SetTuple(0, B);
 
  dataArrayPBC.TakeReference(vtkDataArray::CreateDataArray(VTK_UNSIGNED_CHAR));
  dataArrayPBC->SetNumberOfComponents(3);
  dataArrayPBC->SetNumberOfTuples(1);
  dataArrayPBC->SetName("pbc");
  dataArrayPBC->SetTuple3(0, pbc[0], pbc[1], pbc[2]);

  dataArrayJxext.TakeReference(vtkDataArray::CreateDataArray(VTK_DOUBLE));
  dataArrayJxext->SetNumberOfComponents(1);
  dataArrayJxext->SetNumberOfTuples(1);
  dataArrayJxext->SetName("Jxext");
  dataArrayJxext->SetTuple1(0, Jxext);
  
  dataArrayKx.TakeReference(vtkDataArray::CreateDataArray(VTK_DOUBLE));
  dataArrayKx->SetNumberOfComponents(1);
  dataArrayKx->SetNumberOfTuples(1);
  dataArrayKx->SetName("Kx");
  dataArrayKx->SetTuple1(0, Kx);
  
  dataArrayV.TakeReference(vtkDataArray::CreateDataArray(VTK_DOUBLE));
  dataArrayV->SetNumberOfComponents(1);
  dataArrayV->SetNumberOfTuples(1);
  dataArrayV->SetName("V");
  dataArrayV->SetTuple1(0, V);
  
  imageData->GetFieldData()->AddArray(dataArrayB);
  imageData->GetFieldData()->AddArray(dataArrayPBC);
  imageData->GetFieldData()->AddArray(dataArrayJxext);
  imageData->GetFieldData()->AddArray(dataArrayKx);
  imageData->GetFieldData()->AddArray(dataArrayV);

  free(rho);
  free(phi);
  free(re);
  free(im);
#endif

  return 1;
}
