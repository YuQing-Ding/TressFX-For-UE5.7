//
// Copyright (c) 2016-2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
#include "TressFXDatas.h"
#include "TressFXCommon.h"


FArchive& operator<<(FArchive& Ar, FTressFXMaterialVertex& Vertex)
{
	Ar << Vertex.BaseColorR;
	Ar << Vertex.BaseColorG;
	Ar << Vertex.BaseColorB;
	Ar << Vertex.Roughness;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTressFXClusterCullingData::FTressFXClusterLODInfo& Info)
{
	Ar << Info.VertexOffset;
	Ar << Info.VertexCount0;
	Ar << Info.VertexCount1;
	Ar << Info.RadiusScale0;
	Ar << Info.RadiusScale1;
	return Ar;
}

void FTressFXPoints::SetNum(const uint32 NumPoints)
{
	PointsPosition.SetNum(NumPoints);
	PointsLength.SetNum(NumPoints);
}

void FTressFXCurves::SetNum(const uint32 NumCurves)
{
	CurvesRootUV.SetNum(NumCurves);
}

void FTressFXPoints::Serialize(FArchive& Ar)
{
	Ar << PointsPosition;
	Ar << PointsLength;

}

void FTressFXCurves::Serialize(FArchive& Ar)
{
	Ar << CurvesRootUV;
}

FArchive& operator<<(FArchive& Ar, FIntVector4& Info)
{
	Ar << Info.X;
	Ar << Info.Y;
	Ar << Info.Z;
	Ar << Info.W;

	return Ar;
}

void FTressFXDatas::FSimData::Serialize(FArchive& Ar)
{
	Ar << BoneSkinningDatas;
	Ar << BoneIndexDatas;
}

void FTressFXDatas::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FTressFXImportDataVersion::GUID);
	int32 Ver = Ar.CustomVer(FTressFXImportDataVersion::GUID);

	if (Ver >= FTressFXImportDataVersion::BaseCustomVersion5_0)
	{
		StrandsPoints.Serialize(Ar);
		StrandsCurves.Serialize(Ar);
		Ar << HairDensity;
		Ar << BoundingBox;
		Ar << NumVerticesPerStrand;
		Ar << NumStrandsToRender;

		SimData.Serialize(Ar);
	}

	if (Ver >= FTressFXImportDataVersion::AddMaxStrandLength)
	{
		Ar << MaxRestLength;
	}

	if (Ver >= FTressFXImportDataVersion::AddRandomCurveIndex)
	{
		Ar << RandomCurveIndexArr;
	}
}