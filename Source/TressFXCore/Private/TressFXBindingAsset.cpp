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


#include "TressFXBindingAsset.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Async/ParallelFor.h"



UTressFXBindingAsset::UTressFXBindingAsset(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	TressFXAsset = nullptr;
	TargetSkeletalMesh = nullptr;
}


namespace TressFXUtility
{
	struct FTriangleGrid
	{
		struct FTriangle
		{
			uint32  TriangleIndex;
			uint32  SectionIndex;
			uint32  SectionBaseIndex;

			uint32  Index0;
			uint32  Index1;
			uint32  Index2;

			FVector3f P0;
			FVector3f P1;
			FVector3f P2;

			FVector2f UV0;
			FVector2f UV1;
			FVector2f UV2;
		};

		struct FCell
		{
			TArray<FTriangle> Triangles;
		};
		typedef TArray<const FCell*> FCells;

		FTriangleGrid(const FVector3f& InMinBound, const FVector3f& InMaxBound, float InVoxelWorldSize)
		{
			MinBound = InMinBound;
			MaxBound = InMaxBound;

			// Compute the voxel volume resolution, and snap the max bound to the voxel Grid
			GridResolution = FIntVector::ZeroValue;
			FVector3f VoxelResolutionF = (MaxBound - MinBound) / InVoxelWorldSize;
			GridResolution = FIntVector(FMath::CeilToInt(VoxelResolutionF.X), FMath::CeilToInt(VoxelResolutionF.Y), FMath::CeilToInt(VoxelResolutionF.Z));
			MaxBound = MinBound + FVector3f(GridResolution) * InVoxelWorldSize;

			Cells.SetNum(GridResolution.X * GridResolution.Y * GridResolution.Z);
		}

		FORCEINLINE bool IsValid(const FIntVector& P) const
		{
			return
				0 <= P.X && P.X < GridResolution.X &&
				0 <= P.Y && P.Y < GridResolution.Y &&
				0 <= P.Z && P.Z < GridResolution.Z;
		}

		FORCEINLINE bool IsOutside(const FVector3f& MinP, const FVector3f& MaxP) const
		{
			return
				(MaxP.X <= MinBound.X || MaxP.Y <= MinBound.Y || MaxP.Z <= MinBound.Z) ||
				(MinP.X >= MaxBound.X || MinP.Y >= MaxBound.Y || MinP.Z >= MaxBound.Z);
		}

		FORCEINLINE FIntVector ClampToVolume(const FIntVector& CellCoord, bool& bIsValid) const
		{
			bIsValid = IsValid(CellCoord);
			return FIntVector(
				FMath::Clamp(CellCoord.X, 0, GridResolution.X - 1),
				FMath::Clamp(CellCoord.Y, 0, GridResolution.Y - 1),
				FMath::Clamp(CellCoord.Z, 0, GridResolution.Z - 1));
		}

		FORCEINLINE FIntVector ToCellCoord(const FVector3f& P) const
		{
			bool bIsValid = false;
			const FVector3f F = ((P - MinBound) / (MaxBound - MinBound));
			const FIntVector CellCoord = FIntVector(FMath::FloorToInt(F.X * GridResolution.X), FMath::FloorToInt(F.Y * GridResolution.Y), FMath::FloorToInt(F.Z * GridResolution.Z));
			return ClampToVolume(CellCoord, bIsValid);
		}

		uint32 ToIndex(const FIntVector& CellCoord) const
		{
			uint32 CellIndex = CellCoord.X + CellCoord.Y * GridResolution.X + CellCoord.Z * GridResolution.X * GridResolution.Y;
			check(CellIndex < uint32(Cells.Num()));
			return CellIndex;
		}

		FCells ToCells(const FVector3f& P)
		{
			FCells Out;

			bool bIsValid = false;
			const FIntVector Coord = ToCellCoord(P);
			{
				const uint32 LinearIndex = ToIndex(Coord);
				if (Cells[LinearIndex].Triangles.Num() > 0)
				{
					Out.Add(&Cells[LinearIndex]);
					bIsValid = true;
				}
			}

			int32 Kernel = 1;
			while (!bIsValid)
			{
				for (int32 Z = -Kernel; Z <= Kernel; ++Z)
					for (int32 Y = -Kernel; Y <= Kernel; ++Y)
						for (int32 X = -Kernel; X <= Kernel; ++X)
						{
							// Do kernel box filtering layer, by layer
							if (FMath::Abs(X) != Kernel && FMath::Abs(Y) != Kernel && FMath::Abs(Z) != Kernel)
								continue;

							const FIntVector Offset(X, Y, Z);
							FIntVector C = Coord + Offset;
							C.X = FMath::Clamp(C.X, 0, GridResolution.X - 1);
							C.Y = FMath::Clamp(C.Y, 0, GridResolution.Y - 1);
							C.Z = FMath::Clamp(C.Z, 0, GridResolution.Z - 1);

							const uint32 LinearIndex = ToIndex(C);
							if (Cells[LinearIndex].Triangles.Num() > 0)
							{
								Out.Add(&Cells[LinearIndex]);
								bIsValid = true;
							}
						}
				++Kernel;

				// If no cells have been found in the entire Grid, return
				if (Kernel >= FMath::Max3(GridResolution.X, GridResolution.Y, GridResolution.Z))
				{
					break;
				}
			}

			return Out;
		}

		void Insert(const FTriangle& T)
		{
			FVector3f TriMinBound;
			TriMinBound.X = FMath::Min(T.P0.X, FMath::Min(T.P1.X, T.P2.X));
			TriMinBound.Y = FMath::Min(T.P0.Y, FMath::Min(T.P1.Y, T.P2.Y));
			TriMinBound.Z = FMath::Min(T.P0.Z, FMath::Min(T.P1.Z, T.P2.Z));

			FVector3f TriMaxBound;
			TriMaxBound.X = FMath::Max(T.P0.X, FMath::Max(T.P1.X, T.P2.X));
			TriMaxBound.Y = FMath::Max(T.P0.Y, FMath::Max(T.P1.Y, T.P2.Y));
			TriMaxBound.Z = FMath::Max(T.P0.Z, FMath::Max(T.P1.Z, T.P2.Z));

			if (IsOutside(TriMinBound, TriMaxBound))
				return;

			const FIntVector MinCoord = ToCellCoord(TriMinBound);
			const FIntVector MaxCoord = ToCellCoord(TriMaxBound);

			// Insert Triangle in all cell covered by the AABB of the Triangle
			for (int32 Z = MinCoord.Z; Z <= MaxCoord.Z; ++Z)
			{
				for (int32 Y = MinCoord.Y; Y <= MaxCoord.Y; ++Y)
				{
					for (int32 X = MinCoord.X; X <= MaxCoord.X; ++X)
					{
						const FIntVector CellIndex(X, Y, Z);
						if (IsValid(CellIndex))
						{
							const uint32 CellLinearIndex = ToIndex(CellIndex);
							Cells[CellLinearIndex].Triangles.Add(T);
						}
					}
				}
			}
		}

		FVector3f MinBound;
		FVector3f MaxBound;
		FIntVector GridResolution;
		TArray<FCell> Cells;
	};

	// Closest point on A Triangle from another point
	// Code from the book "Real-Time Collision Detection" by Christer Ericson
	struct FTrianglePoint
	{
		FVector3f P;
		FVector3f Barycentric;
	};

	static FTrianglePoint ComputeClosestPoint(const FTriangleGrid::FTriangle& Tri, const FVector3f& P)
	{
		const FVector3f A = Tri.P0;
		const FVector3f B = Tri.P1;
		const FVector3f C = Tri.P2;

		// Check if P is in vertex region outside A.
		FVector3f AB = B - A;
		FVector3f AC = C - A;
		FVector3f AP = P - A;
		float D1 = FVector3f::DotProduct(AB, AP);
		float D2 = FVector3f::DotProduct(AC, AP);
		if (D1 <= 0.f && D2 <= 0.f)
		{
			FTrianglePoint Out;
			Out.P = A;
			Out.Barycentric = FVector3f(1, 0, 0);
			return Out;
		}

		// Check if P is in vertex region outside B.
		FVector3f BP = P - B;
		float D3 = FVector3f::DotProduct(AB, BP);
		float D4 = FVector3f::DotProduct(AC, BP);
		if (D3 >= 0.f && D4 <= D3)
		{
			FTrianglePoint Out;
			Out.P = B;
			Out.Barycentric = FVector3f(0, 1, 0);
			return Out;
		}

		// Check if P is in edge region of AB, and if so, return the projection of P onto AB.
		float VC = D1 * D4 - D3 * D2;
		if (VC <= 0.f && D1 >= 0.f && D3 <= 0.f)
		{
			float V = D1 / (D1 - D3);

			FTrianglePoint Out;
			Out.P = A + V * AB;
			Out.Barycentric = FVector3f(1 - V, V, 0);
			return Out;
		}

		// Check if P is in vertex region outside C.
		FVector3f CP = P - C;
		float D5 = FVector3f::DotProduct(AB, CP);
		float D6 = FVector3f::DotProduct(AC, CP);
		if (D6 >= 0.f && D5 <= D6)
		{
			FTrianglePoint Out;
			Out.P = C;
			Out.Barycentric = FVector3f(0, 0, 1);
			return Out;
		}

		// Check if P is in edge region of AC, and if so, return the projection of P onto AC.
		float VB = D5 * D2 - D1 * D6;
		if (VB <= 0.f && D2 >= 0.f && D6 <= 0.f)
		{
			float W = D2 / (D2 - D6);
			FTrianglePoint Out;
			Out.P = A + W * AC;
			Out.Barycentric = FVector3f(1 - W, 0, W);
			return Out;
		}

		// Check if P is in edge region of BC, and if so, return the projection of P onto BC.
		float VA = D3 * D6 - D5 * D4;
		if (VA <= 0.f && D4 - D3 >= 0.f && D5 - D6 >= 0.f)
		{
			float W = (D4 - D3) / (D4 - D3 + D5 - D6);
			FTrianglePoint Out;
			Out.P = B + W * (C - B);
			Out.Barycentric = FVector3f(0, 1 - W, W);
			return Out;
		}

		// P must be inside the face region. Compute the closest point through its barycenTric coordinates (u,V,W).
		float Denom = 1.f / (VA + VB + VC);
		float V = VB * Denom;
		float W = VC * Denom;

		FTrianglePoint Out;
		Out.P = A + AB * V + AC * W;
		Out.Barycentric = FVector3f(1 - V - W, V, W);
		return Out;
	}
}

using namespace TressFXUtility;

void UTressFXBindingAsset::Build()
{
	const FTressFXGroupData& TressFXGroupData = TressFXAsset->TressFXGroupsData[0];
	const FTressFXGroupsRendering& TressFXGroupRendering = TressFXAsset->TressFXGroupsRendering;

	const uint32 NumVerticesPerStrand = TressFXGroupData.TFXData.NumVerticesPerStrand;
	const uint32 NumGuideStrands = TressFXGroupRendering.NumGuides;
	const FVector4f* StrandPosBuffer = TressFXGroupData.TFXData.StrandsPoints.PointsPosition.GetData();
	FBox3f StrandBound;
	StrandBound.Init();
	for (uint32 GuideIndex = 0; GuideIndex < NumGuideStrands; ++GuideIndex)
	{
		const uint32 RootVetexIndex = GuideIndex * NumVerticesPerStrand;
		for (uint32 VertexIndex = 0; VertexIndex < NumVerticesPerStrand; VertexIndex++)
		{
			FVector4f Pos4 = StrandPosBuffer[VertexIndex + RootVetexIndex];
			FVector3f Pos3(Pos4.X, Pos4.Y, Pos4.Z);
			StrandBound += Pos3;
		}
	}

	if (TargetSkeletalMesh != nullptr)
	{
		FSkeletalMeshRenderData* SkeletalMeshRenderData = TargetSkeletalMesh->GetResourceForRendering();
		const TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderData = SkeletalMeshRenderData->LODRenderData;
		const uint32 LODCount = LODRenderData.Num();

		BindingData.SetNumZeroed(LODCount);
		for (uint32 LODIndex = 0; LODIndex < LODCount; LODIndex++)
		{
			FTressFXMorphTargetBindingLODData& CurrentBindingData = BindingData[LODIndex];
			CurrentBindingData.RootBindingTriangles.SetNumZeroed(NumGuideStrands);
			CurrentBindingData.LODIndex = LODIndex;

			const FSkeletalMeshLODRenderData& CurrentLOD = LODRenderData[LODIndex];
			TArray<uint32> IndexBuffer;
			CurrentLOD.MultiSizeIndexContainer.GetIndexBuffer(IndexBuffer);
			const TArray<FSkelMeshRenderSection>& RenderSections = CurrentLOD.RenderSections;
			const int32 SectionCount = RenderSections.Num();
			FBox3f MeshBound;
			MeshBound.Init();
			const FPositionVertexBuffer& VertexPosBuffer = CurrentLOD.StaticVertexBuffers.PositionVertexBuffer;
			for (int32 SecIndex = 0; SecIndex < SectionCount; SecIndex++)
			{
				const auto& CurRenderSection = RenderSections[SecIndex];
				const uint32 TriCount = CurRenderSection.NumTriangles;
				const uint32 SecBaseIndex = CurRenderSection.BaseIndex;
				for (uint32 TriIndex = 0; TriIndex < TriCount; TriIndex++)
				{
					for (uint8 i = 0; i < 3; i++)
					{
						MeshBound += VertexPosBuffer.VertexPosition(IndexBuffer[SecBaseIndex + TriIndex * 3 + i]);
					}
				}
			}

			FVector3f GridMin, GridMax;
			if (StrandBound.GetExtent().Size() < MeshBound.GetExtent().Size())
			{
				GridMin = StrandBound.Min;
				GridMax = StrandBound.Max;
			}
			else
			{
				GridMin = MeshBound.Min;
				GridMax = MeshBound.Max;
			}

			const float VoxelWorldSize = 2.0f;
			FTriangleGrid Grid(GridMin, GridMax, VoxelWorldSize);
			for (int32 SecIndex = 0; SecIndex < SectionCount; SecIndex++)
			{
				const FSkelMeshRenderSection& CurRenderSection = RenderSections[SecIndex];
				const uint32 TriCount = CurRenderSection.NumTriangles;
				const uint32 SecBaseIndex = CurRenderSection.BaseIndex;
				for (uint32 TriIndex = 0; TriIndex < TriCount; TriIndex++)
				{
					FTriangleGrid::FTriangle Tri;
					Tri.Index0 = IndexBuffer[SecBaseIndex + TriIndex * 3 + 0];
					Tri.Index1 = IndexBuffer[SecBaseIndex + TriIndex * 3 + 1];
					Tri.Index2 = IndexBuffer[SecBaseIndex + TriIndex * 3 + 2];
					Tri.P0 = VertexPosBuffer.VertexPosition(IndexBuffer[SecBaseIndex + TriIndex * 3 + 0]);
					Tri.P1 = VertexPosBuffer.VertexPosition(IndexBuffer[SecBaseIndex + TriIndex * 3 + 1]);
					Tri.P2 = VertexPosBuffer.VertexPosition(IndexBuffer[SecBaseIndex + TriIndex * 3 + 2]);
					Grid.Insert(Tri);
				}
			}
			TAtomic<uint32> IsValid{ 1 };
			ParallelFor(NumGuideStrands, [NumVerticesPerStrand, StrandPosBuffer, &Grid, &IsValid, &CurrentBindingData](uint32 StrandIndex)
				{
					const uint32 StrandRootIndex = StrandIndex * NumVerticesPerStrand;
					const FVector3f& RootPos = StrandPosBuffer[StrandRootIndex];
					const FTriangleGrid::FCells Cells = Grid.ToCells(RootPos);
					if (Cells.Num() == 0)
					{
						IsValid = 0;
						return;
					}
					float ClosestDist = FLT_MAX;
					FTriangleGrid::FTriangle ClosestTri;
					FVector2f ClosestBarycentrices;
					for (const FTriangleGrid::FCell* Cell : Cells)
					{
						for (const FTriangleGrid::FTriangle& CellTri : Cell->Triangles)
						{
							const FTrianglePoint Tri = ComputeClosestPoint(CellTri, RootPos);
							const float Dist = FVector3f::Dist(Tri.P, RootPos);
							if (Dist < ClosestDist)
							{
								ClosestDist = Dist;
								ClosestBarycentrices = FVector2f(Tri.Barycentric.X, Tri.Barycentric.Y);
								ClosestTri = CellTri;
							}
						}
					}
					FTressFXMorphTargetBindingLODData::FGuideRootTriangle& CurStrandBindingData 
						= CurrentBindingData.RootBindingTriangles[StrandIndex];
					CurStrandBindingData.Vertex0Index = ClosestTri.Index0;
					CurStrandBindingData.Vertex1Index = ClosestTri.Index1;
					CurStrandBindingData.Vertex2Index = ClosestTri.Index2;
					CurStrandBindingData.TriBarycentrices = ClosestBarycentrices;

				}
			);

			// Remap VertexIndex from Global to Local
			for (uint32 StrandIndex = 0; StrandIndex < NumGuideStrands; ++StrandIndex)
			{
				FTressFXMorphTargetBindingLODData::FGuideRootTriangle& CurStrandBindingData
					= CurrentBindingData.RootBindingTriangles[StrandIndex];

				CurStrandBindingData.Vertex0Index = CurrentBindingData.LocalToGlobalVertexIndexMap.AddUnique(CurStrandBindingData.Vertex0Index);
				CurStrandBindingData.Vertex1Index = CurrentBindingData.LocalToGlobalVertexIndexMap.AddUnique(CurStrandBindingData.Vertex1Index);
				CurStrandBindingData.Vertex2Index = CurrentBindingData.LocalToGlobalVertexIndexMap.AddUnique(CurStrandBindingData.Vertex2Index);

			}
		}
		
		if (BindingData.Num())
			InitResources();
	}

}

void UTressFXBindingAsset::Serialize(FArchive& Ar)
{
	
	Super::Serialize(Ar);

	Ar << BindingData;

}

void UTressFXBindingAsset::PostLoad()
{
	Super::PostLoad();

	InitResources();
}

void UTressFXBindingAsset::BeginDestroy()
{
	ReleaseResources();

	Super::BeginDestroy();
}


template<typename T>
inline void InternalReleaseResource_BindingAsset(T*& Resource)
{
	if (Resource)
	{
		T* InResource = Resource;
		ENQUEUE_RENDER_COMMAND(ReleaseTressFXResourceCommand)(
			[InResource](FRHICommandList& RHICmdList)
			{
				InResource->ReleaseResource();
				delete InResource;
			});
		Resource = nullptr;
	}
}

void UTressFXBindingAsset::InitResources()
{
	if (bIsInitialized)
		ReleaseResources();

	GuidesBindingResource = new FTressFXGuidesBindingResource(BindingData[0]);
	BeginInitResource(GuidesBindingResource);

	bIsInitialized = true;
}

void UTressFXBindingAsset::ReleaseResources()
{
	bIsInitialized = false;
	InternalReleaseResource_BindingAsset(GuidesBindingResource);
}

void FTressFXMorphTargetBindingLODData::FGuideRootTriangle::Serialize(FArchive& Ar)
{
	if (!Ar.IsObjectReferenceCollector())
	{
		Ar << TriBarycentrices;
		Ar << Vertex0Index;
		Ar << Vertex1Index;
		Ar << Vertex2Index;
	}
}

FArchive& operator <<(FArchive& Ar, FTressFXMorphTargetBindingLODData::FGuideRootTriangle& TriData)
{
	TriData.Serialize(Ar);
	return Ar;
}

void FTressFXMorphTargetBindingLODData::Serialize(FArchive& Ar)
{
	if (!Ar.IsObjectReferenceCollector())
	{
		Ar << RootBindingTriangles;
		Ar << LocalToGlobalVertexIndexMap;
	}
}

FArchive& operator <<(FArchive& Ar, FTressFXMorphTargetBindingLODData& BindingLODData)
{
	BindingLODData.Serialize(Ar);
	return Ar;
}

