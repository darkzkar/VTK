/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkParametricFunctionSource.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen 
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkParametricFunctionSource.h"
#include "vtkParametricFunction.h"
#include "vtkObjectFactory.h"
#include "vtkMath.h"
#include "vtkFloatArray.h"
#include "vtkPoints.h"
#include "vtkTriangleFilter.h"
#include "vtkPolyDataNormals.h"
#include "vtkPointData.h"
#include "vtkCellArray.h"
#include "vtkPolyData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"

#include <math.h>
#include <vtkstd/string>

vtkCxxRevisionMacro(vtkParametricFunctionSource, "1.6");
vtkStandardNewMacro(vtkParametricFunctionSource);


vtkParametricFunctionSource::vtkParametricFunctionSource() :
  ParametricFunction(NULL)
  , NumberOfUPoints(50)
  , NumberOfVPoints(50)
  , NumberOfWPoints(50)
  , ScalarMode(vtkParametricFunctionSource::SCALAR_NONE)
{
  this->SetNumberOfInputPorts(0);
}


vtkParametricFunctionSource::~vtkParametricFunctionSource()
{
}

void vtkParametricFunctionSource::SetParametricFunction(vtkParametricFunction *f)
{
  if ( f != this->ParametricFunction )
    {
    if ( this->ParametricFunction != NULL )
      {
      this->ParametricFunction->UnRegister(this);
      }
    this->ParametricFunction = f;
    if ( this->ParametricFunction != NULL )
      {
      this->ParametricFunction->Register(this);
      }
    this->Modified();
    }
}

void vtkParametricFunctionSource::MakeTriangleStrips ( vtkCellArray * strips, 
                                                       int PtsU, int PtsV )
{
  int id1;
  int id2;

  vtkDebugMacro(<< "Executing MakeTriangleStrips()");

  for ( int i = 0; i < PtsU - 1; ++i )
    {
    // Allocate space
    if ( this->ParametricFunction->GetJoinV() )
      {
      strips->InsertNextCell( PtsV * 2 + 2 );
      }
    else
      {
      strips->InsertNextCell( PtsV * 2 );
      }

    // Fill the allocated space with the indexes to the points.
    for ( int j = 0; j < PtsV; ++j )
      {
      id1 = j + i * PtsV;
      id2 = (i + 1 ) * PtsV + j;
      if ( this->ParametricFunction->GetClockwiseOrdering() )
        { 
        strips->InsertCellPoint(id1);
        strips->InsertCellPoint(id2); 
        }
      else
        {
        strips->InsertCellPoint(id2);
        strips->InsertCellPoint(id1);
        }
      }
    // If necessary, connect the ends of the triangle strip.
    if ( this->ParametricFunction->GetJoinV() )
      {
      if ( this->ParametricFunction->GetTwistV() )
        {
        id1 = (i + 1) * PtsV;
        id2 = i * PtsV;
        }
      else
        {
        id1 = i * PtsV;
        id2 = (i + 1) * PtsV;
        }
      if ( this->ParametricFunction->GetClockwiseOrdering() )
        { 
        strips->InsertCellPoint(id1);
        strips->InsertCellPoint(id2); 
        }
      else
        {
        strips->InsertCellPoint(id2);
        strips->InsertCellPoint(id1);
        }
      }
    }
  // If required, connect the last triangle strip to the first by 
  // adding a new triangle strip and filling it with the indexes
  // to the points.
  if ( this->ParametricFunction->GetJoinU() )
    {
    if ( this->ParametricFunction->GetJoinV() )
      {
      strips->InsertNextCell( PtsV * 2 + 2 );
      }
    else
      {
      strips->InsertNextCell( PtsV * 2 );
      }
    for ( int j = 0; j < PtsV; ++j )
      {
      if ( this->ParametricFunction->GetTwistU() )
        {
        id1 = ( PtsU - 1 ) * PtsV + j;
        id2 = PtsV - 1 - j;
        }
      else
        {
        id1 = ( PtsU - 1 ) * PtsV + j;
        id2 = j;
        }
      if ( this->ParametricFunction->GetClockwiseOrdering() )
        { 
        strips->InsertCellPoint(id1);
        strips->InsertCellPoint(id2); 
        }
      else
        {
        strips->InsertCellPoint(id2);
        strips->InsertCellPoint(id1);
        }
      }

    // If necessary, connect the ends of the triangle strip.
    if ( this->ParametricFunction->GetJoinV() )
      {
      if ( this->ParametricFunction->GetTwistU() )
        {
        if ( this->ParametricFunction->GetTwistV() )
          {
          id1 = PtsV - 1;
          id2 = ( PtsU - 1 ) * PtsV;
          }
        else
          {
          id1 = ( PtsU - 1 ) * PtsV;
          id2 = PtsV - 1;
          }
        }
      else
        {
        if ( this->ParametricFunction->GetTwistV() )
          {
          id1 = 0;
          id2 = ( PtsU - 1 ) * PtsV;
          }
        else
          {
          id1 = ( PtsU - 1 ) * PtsV;
          id2 = 0;
          }
        }
      if ( this->ParametricFunction->GetClockwiseOrdering() )
        { 
        strips->InsertCellPoint(id1);
        strips->InsertCellPoint(id2); 
        }
      else
        {
        strips->InsertCellPoint(id2);
        strips->InsertCellPoint(id1);
        }
      }
    }
  vtkDebugMacro(<< "MakeTriangleStrips() finished.");
}

int vtkParametricFunctionSource::RequestData(vtkInformation *vtkNotUsed(info),
                                             vtkInformationVector **vtkNotUsed(inputV),
                                             vtkInformationVector *output)
{
  vtkDebugMacro(<< "Executing");
  
  // Check that a parametric function has been defined
  if ( !this->ParametricFunction )
    {
    vtkErrorMacro(<<"Parametric function not defined");
    return 1;
    }
  
  // Used to hold the surface
  vtkPolyData * pd = vtkPolyData::New();

  // Adjust so the range this->MinimumU ... this->ParametricFunction->GetMaximumU(), this->MinimumV
  // ... this->ParametricFunction->GetMaximumV() is included in the triangulation.
  double MaxU = this->ParametricFunction->GetMaximumU() + 
    (this->ParametricFunction->GetMaximumU() - this->ParametricFunction->GetMinimumU()) /
    this->NumberOfUPoints;
  int PtsU = this->NumberOfUPoints + 1;
  double MaxV = this->ParametricFunction->GetMaximumV() + 
    (this->ParametricFunction->GetMaximumV() - this->ParametricFunction->GetMinimumV()) / 
    this->NumberOfVPoints;
  int PtsV = this->NumberOfVPoints + 1;
  int totPts = PtsU * PtsV;

  // Scalars associated with each point 
  vtkFloatArray * sval = vtkFloatArray::New(); 
  sval->SetNumberOfTuples( totPts ); 

  // The normals to the surface
  vtkFloatArray * nval = vtkFloatArray::New();
  nval->SetNumberOfComponents(3);
  nval->SetNumberOfTuples(totPts); 

  vtkPoints * points = vtkPoints::New();
  points->SetNumberOfPoints( totPts );

  double uStep = ( MaxU - this->ParametricFunction->GetMinimumU() ) / PtsU;
  double vStep = ( MaxV - this->ParametricFunction->GetMinimumV() ) / PtsV;

  // Find the mid points of the (u,v) map.
  double u0 = this->ParametricFunction->GetMinimumU();
  double u_mp = (MaxU - u0)/2.0 + u0 - uStep;
  while ( u0 < u_mp ) 
    {
    u0 += uStep;
    }
  
  double v0 = this->ParametricFunction->GetMinimumV();
  double v_mp = (MaxV - v0)/2.0 + v0 - vStep;
  while ( v0 < v_mp )  
    {
    v0 += vStep;
    }
  u_mp += uStep;
  v_mp += vStep;

  // At this point (u_mp, v_mp) is the midpoint of the (u,v) map and (u0,v0)
  // corresponds to the nearest grid point to the midpoint of the (u,v) map.
  //
  double rel_u = 0; // will be u - u_mp
  double rel_v = 0; // will be v - v_mp

  int k = 0;
  double uv[3];
  uv[0] = this->ParametricFunction->GetMinimumU() - uStep;

  for ( int i = 0; i < PtsU; ++i )
    {
    uv[0] += uStep;
    uv[1] = this->ParametricFunction->GetMinimumV() - vStep;

    for ( int j = 0; j < PtsV; ++j )
      {
      uv[1] += vStep;

      // The point
      double Pt[3];
      // Partial derivative at Pt with respect to u,v,w.
      double Du[9];
      // Partial derivative at Pt with respect to v.
      double *Dv = Du+3;

      // Calculate fn(u)->(Pt,Du).
      this->ParametricFunction->Evaluate(uv,Pt,Du);

      // Insert the points and scalar.
      points->InsertPoint(k, Pt[0], Pt[1], Pt[2]);
      double scalar;

      if ( this->ScalarMode != SCALAR_NONE )
        {
        switch ( this->ScalarMode )
          {
          case SCALAR_U:
            scalar = uv[0];
            break;
          case SCALAR_V:
            scalar = uv[1];
            break;
          case SCALAR_U0:
            scalar = uv[0] == u0 ? 1 : 0;
            break;
          case SCALAR_V0:
            scalar = uv[1] == v0 ? 1 : 0;
            break;
          case SCALAR_U0V0:
            scalar = 0;
            // u0, v0
            if ( uv[0] == u0 && uv[1] == v0 ) 
              {
              scalar = 3;
              }
            else
              {
              // u0 line
              if ( uv[0] == u0 ) 
                {
                scalar = 1;
                }
              else 
                {
                // v0 line
                if ( uv[1] == v0 ) scalar = 2;
                }
              }
            break;
          case SCALAR_MODULUS:
            rel_u = uv[0] - u_mp;
            rel_v = uv[1] - v_mp;
            scalar = sqrt(rel_u * rel_u + rel_v * rel_v);
            break;
          case SCALAR_PHASE:
            rel_u = uv[0] - u_mp;
            rel_v = uv[1] - v_mp;
            scalar = vtkMath::RadiansToDegrees() * atan2(rel_v,rel_u);
            if ( scalar < 0 ) scalar += 360;
            break;
          case SCALAR_QUADRANT:
            if ( uv[0] >= u0 && uv[1] >= v0 )
              {
              scalar = 1;
              break;
              }
            if ( uv[0] < u0 && uv[1] >= v0 ) 
              {
              scalar = 2;
              break;
              }
            if ( uv[0] < u0 && uv[1] < v0 ) 
              {
              scalar = 3;
              }
            else 
              {
              scalar = 4;
              }
            break;
          case SCALAR_X:
            scalar = Pt[0];
            break;
          case SCALAR_Y:
            scalar = Pt[1];
            break;
          case SCALAR_Z:
            scalar = Pt[2];
            break;
          case SCALAR_DISTANCE:
            scalar = sqrt(Pt[0]*Pt[0] + Pt[1]*Pt[1] + Pt[2]*Pt[2]);
            break;
          case SCALAR_FUNCTION_DEFINED:
            scalar = this->ParametricFunction->EvaluateScalar(uv, Pt, Du);
            break;
          case SCALAR_NONE:
          default:
            scalar = 0;
          }
        sval->SetValue(k, scalar);
        }

      // Calculate the normal.
      if ( this->ParametricFunction->GetDerivativesAvailable() )
        {
        double n[3];
        vtkMath::Cross(Du,Dv,n);
        vtkMath::Normalize(n);
        nval->SetTuple3(k, n[0], n[1], n[2]);
        }

      ++k;
      }
    }

  // Make the triangle strips
  vtkCellArray * strips = vtkCellArray::New(); 
  // This is now a list of ID's defining the triangles.
  this->MakeTriangleStrips ( strips, PtsU, PtsV ); 

  pd->SetPoints( points ); 
  pd->SetVerts( strips );
  pd->SetStrips( strips );
  if ( this->ScalarMode != SCALAR_NONE )
    {
    pd->GetPointData()->SetScalars( sval );
    }

  if ( this->ParametricFunction->GetDerivativesAvailable() )
    {
    pd->GetPointData()->SetNormals( nval );
    }
  pd->Modified();

  vtkTriangleFilter * tri = vtkTriangleFilter::New();
  vtkPolyDataNormals * norm = vtkPolyDataNormals::New();
  if ( this->ParametricFunction->GetDerivativesAvailable() )
    {
    //Generate polygons from the triangle strips
    tri->SetInput(pd);
    }
  else
    {
    // Calculate Normals
    norm->SetInput(pd);
    // Generate polygons from the triangle strips
    tri->SetInput(norm->GetOutput());
    }
  tri->PassLinesOn();
  tri->PassVertsOff();
  tri->Update();

  vtkInformation *outInfo = output->GetInformationObject(0);
  vtkPolyData *outData = static_cast<vtkPolyData*>(outInfo->Get( vtkDataObject::DATA_OBJECT() ));
  outData->DeepCopy(tri->GetOutput());

  // Were done, clean up.
  points->Delete();
  sval->Delete();
  nval->Delete();
  strips->Delete();
  pd->Delete();
  tri->Delete();
  norm->Delete();

  return 1;
}

/*
void vtkParametricFunctionSource::GetAllParametricTriangulatorParameters (
  int & numberOfUPoints,
  int & numberOfVPoints,
  double & minimumU,
  double & maximumU,
  double & minimumV,
  double & maximumV,
  int & joinU,
  int & joinV,
  int & twistU,
  int & twistV,
  int & clockwiseOrdering,
  int & scalarMode)
{
  numberOfUPoints = this->NumberOfUPoints;
  numberOfVPoints = this->NumberOfVPoints;
  minimumU = this->MinimumU;
  maximumU = this->ParametricFunction->GetMaximumU();
  minimumV = this->ParametricFunction->GetMinimumV();
  maximumV = this->ParametricFunction->GetMaximumV();
  joinU = this->ParametricFunction->GetJoinU();
  joinV = this->ParametricFunction->GetJoinV;
  twistU = this->ParametricFunction->GetTwistU();
  twistV = this->ParametricFunction->GetTwistV();
  clockwiseOrdering = this->ParametricFunction->GetClockwiseOrdering();
  scalarMode = this->ScalarMode;
}

void vtkParametricFunctionSource::SetAllParametricTriangulatorParameters (
  int numberOfUPoints,
  int numberOfVPoints,
  double minimumU,
  double maximumU,
  double minimumV,
  double maximumV,
  int joinU,
  int joinV,
  int twistU,
  int twistV,
  int ParametricFunction->GetclockwiseOrdering(),
  int scalarMode)
{
  this->NumberOfUPoints = numberOfUPoints;
  this->NumberOfVPoints = numberOfVPoints;
  this->ParametricFunction->SetMinimumU( minimumU );
  this->ParametricFunction->SetMaximumU( maximumU );
  this->ParametricFunction->SetMinimumV( minimumV );
  this->ParametricFunction->SetMaximumV( maximumV );
  this->ParametricFunction->SetJoinU( joinU );
  this->ParametricFunction->SetJoinV( joinV );
  this->ParametricFunction->SetTwistU( twistU );
  this->ParametricFunction->SetTwistV( twistV );
  this->ParametricFunction->SetClockwiseOrdering( clockwiseOrdering );
  this->ScalarMode = scalarMode;
  if ( ScalarMode < SCALAR_NONE || ScalarMode > SCALAR_USER_DEFINED )
    {
    this->ScalarMode = SCALAR_NONE;
    }
  this->Modified();
}
*/

void vtkParametricFunctionSource::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "NumberOfUPoints: " << this->NumberOfUPoints << "\n";
  os << indent << "NumberOfVPoints: " << this->NumberOfVPoints << "\n";
  os << indent << "NumberOfWPoints: " << this->NumberOfWPoints << "\n";

  vtkstd::string s;
  switch ( this->ScalarMode )
  {
  case SCALAR_NONE:
    s = "SCALAR_NONE";
    break;
  case SCALAR_U:
    s = "SCALAR_U";
    break; 
  case SCALAR_V:
    s = "SCALAR_V";
    break;
  case SCALAR_U0:
    s = "SCALAR_U0";
    break;
  case SCALAR_V0:
    s = "SCALAR_V0";
    break;
  case SCALAR_U0V0:
    s = "SCALAR_U0V0";
    break;
  case SCALAR_MODULUS:
    s = "SCALAR_MODULUS";
    break;
  case SCALAR_PHASE:
    s = "SCALAR_PHASE";
    break;
  case SCALAR_QUADRANT:
    s = "SCALAR_QUADRANT";
    break;
  case SCALAR_X:
    s = "SCALAR_X";
    break;
  case SCALAR_Y:
    s = "SCALAR_Y";
    break; 
  case SCALAR_Z:
    s = "SCALAR_Z";
    break;
  case SCALAR_DISTANCE:
    s = "SCALAR_DISTANCE";
    break;
  case SCALAR_FUNCTION_DEFINED:
    s = "SCALAR_FUNCTION_DEFINED";
    break;
  default:
    s = "Unknown scalar mode.";
   }
  os << indent << "Scalar Mode: " << s.c_str() << "\n";

}
