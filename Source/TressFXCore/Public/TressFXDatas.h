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
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RHIDefinitions.h"
#include "PackedNormal.h"
#include "RenderGraphResources.h"
#include "TressFXBoneSkinningData.h"


struct FTressFXMaterialVertex
{
	// sRGB color space
	uint8 BaseColorR;
	uint8 BaseColorG;
	uint8 BaseColorB;

	uint8 Roughness;
};

struct FTressFXInterpolation0Vertex
{
	uint16 Index0;
	uint16 Index1;
	uint16 Index2;

	uint8 VertexWeight0;
	uint8 VertexWeight1;
};

struct FTressFXInterpolation1Vertex
{
	uint8 VertexIndex0;
	uint8 VertexIndex1;
	uint8 VertexIndex2;

	uint8 VertexLerp0;
	uint8 VertexLerp1;
	uint8 VertexLerp2;

	uint8 Pad0;
	uint8 Pad1;
};

struct FTressFXInterpolation0Format
{
	typedef FTressFXInterpolation0Vertex Type;

	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UShort4;
	static const EPixelFormat Format = PF_R16G16B16A16_UINT;
};

struct FTressFXInterpolation1Format
{
	typedef FTressFXInterpolation1Vertex Type;

	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UShort4;
	static const EPixelFormat Format = PF_R16G16B16A16_UINT;
};

struct FTressFXRootIndexFormat
{
	typedef uint32 Type;

	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_UInt;
	static const EPixelFormat Format = PF_R32_UINT;
};

struct FTressFXRaytracingFormat
{
	typedef FVector4f Type;

	static const uint32 ComponentCount = 1;
	static const uint32 SizeInByte = sizeof(Type);
	static const EVertexElementType VertexElementType = VET_Float4;
	static const EPixelFormat Format = PF_A32B32G32R32F;
};

/** Hair strands points attribute */
struct TRESSFXCORE_API FTressFXPoints
{
	/** Serialize the points */
	void Serialize(FArchive& Ar);

	/** Set the number of points */
	void SetNum(const uint32 NumPoints);

	/** Reset the points to 0 */
	void Reset();

	/** Get the number of points */
	uint32 Num() const { return PointsPosition.Num(); }

	/** Points position in local space */
	TArray<FVector4f> PointsPosition;

	/** Normalized length */
	TArray<float> PointsLength;

};

/** TressFX Curves attribute */
struct TRESSFXCORE_API FTressFXCurves
{
	/** Serialize the curves */
	void Serialize(FArchive& Ar);

	/** Set the number of Curves */
	void SetNum(const uint32 NumPoints);

	/** Reset the curves to 0 */
	void Reset();

	/** Get the number of Curves */
	uint32 Num() const { return CurvesRootUV.Num(); }

	/** Roots UV. Support UDIM coordinate up to 256x256 */
	TArray<FVector2f> CurvesRootUV; // 

	/** Strand ID associated with each curve */
	TArray<int> StrandIDs;

	/** Mapping of imported TressFX ID to index */
	TMap<int, int> TressFXIDToIndex;

};

struct TRESSFXCORE_API FTressFXDatas
{
	/** Serialize all the TressFX datas */
	void Serialize(FArchive& Ar);

	/* Get the total number of points */
	uint32 GetNumPoints() const { return StrandsPoints.Num(); }

	/* Get the total number of Curves */
	uint32 GetNumCurves() const { return StrandsCurves.Num(); }

	uint32 GetNumPointsToRender() const { return NumVerticesPerStrand * NumStrandsToRender;}

	void Reset();

	/** List of all the strands points */
	FTressFXPoints StrandsPoints;

	/** List of all the strands curves */
	FTressFXCurves StrandsCurves;

	/** The Standard Hair Density */
	float HairDensity = 1;

	/* Strands bounding box */
	FBox3f BoundingBox;

	uint32 NumVerticesPerStrand = 0;
	uint32 NumStrandsToRender = 0;
	float MaxRestLength = 0.f;

	TArray<uint32> RandomCurveIndexArr;

	struct FSimData
	{
		TArray<FTressFXBoneSkinningData> BoneSkinningDatas;
		TArray<FTressFXBoneIndexData> BoneIndexDatas;

		void Serialize(FArchive& Ar);
	} SimData;

	bool IsBindingAsset() const { return SimData.BoneSkinningDatas.Num()>0; }
};

struct FTressFXClusterCullingData
{
	FTressFXClusterCullingData();
	void Reset();
	void Serialize(FArchive& Ar);
	bool IsValid() const { return ClusterCount > 0 && VertexCount > 0; }

	static const uint32 MaxLOD = 8;
	
	struct FTressFXClusterLODInfo
	{
		uint32 VertexOffset = 0;
		uint32 VertexCount0 = 0;
		uint32 VertexCount1 = 0;
		FFloat16 RadiusScale0 = 0;
		FFloat16 RadiusScale1 = 0;
	};

	TArray<FTressFXClusterLODInfo> ClusterLODInfos;

	/* Number of cluster  */
	uint32 ClusterCount = 0;

	/* Number of vertex  */
	uint32 VertexCount = 0;
};


struct FTressFXMorphTargetBindingLODData
{
	int32 LODIndex{ -1 };
	struct FGuideRootTriangle
	{
		FVector2f TriBarycentrices;
		// Local Vertex Index , should be mapped to global
		uint32 Vertex0Index;
		uint32 Vertex1Index;
		uint32 Vertex2Index;
		void Serialize(FArchive& Ar);
		friend FArchive& operator <<(FArchive& Ar, FGuideRootTriangle& TriData);
	};
	TArray<FGuideRootTriangle> RootBindingTriangles;
	TArray<uint32> LocalToGlobalVertexIndexMap;

	void Serialize(FArchive& Ar);
	friend FArchive& operator <<(FArchive& Ar, FTressFXMorphTargetBindingLODData& BindingLodData);
};

