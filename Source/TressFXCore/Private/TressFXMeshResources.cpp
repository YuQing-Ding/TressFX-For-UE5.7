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
#include "TressFXMeshResources.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/PhysicsObjectVersion.h"
#include "UObject/ReleaseObjectVersion.h"
#include "Async/ParallelFor.h"
#include "RenderGraph.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "TressFXInterface.h"



template<typename FormatType>
void TFXInternalCreateUploadStructuredBufferRDG(FRDGBuilder& GraphBuilder, const TArray<FormatType>& InData, uint32 InSizeInByte, FRDGExternalBufferTFX& Out, const TCHAR* DebugName);

template<typename FormatType>
void TFXInternalCreateStructuredBufferRDG(FRDGBuilder& GraphBuilder, const TArray<FormatType>& InData, uint32 InSizeInByte, FRDGExternalBufferTFX& Out, const TCHAR* DebugName);

template<typename FormatType>
void TFXInternalCreateUploadStructuredBufferRDG(FRDGBuilder& GraphBuilder, const TArray<FormatType>& InData, uint32 InSizeInByte, int32 NumElements, FRDGExternalBufferTFX& Out, const TCHAR* DebugName);

template<typename FormatType>
void TFXInternalCreateStructuredBufferRDG(FRDGBuilder& GraphBuilder, const TArray<FormatType>& InData, uint32 InSizeInByte, int32 NumElements, FRDGExternalBufferTFX& Out, const TCHAR* DebugName);

template<typename FormatType>
static void InternalCreateStructuredBufferRDG_(FRDGBuilder& GraphBuilder, const TArray<FormatType>& InData, uint32 InSizeInByte, FRDGExternalBufferTFX& Out, const TCHAR* DebugName)
{
	FRDGBufferRef Buffer = nullptr;

	const uint32 DataCount = InData.Num();
	const uint32 DataSizeInBytes = InSizeInByte * DataCount;
	if (DataSizeInBytes == 0)
	{
		Out.Buffer = nullptr;
		return;
	}

	Buffer = CreateStructuredBuffer(
		GraphBuilder,
		DebugName,
		InSizeInByte,
		DataCount,
		InData.GetData(),
		DataSizeInBytes,
		ERDGInitialDataFlags::NoCopy);

	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out);
}


FTressFXMeshRestResource::FTressFXMeshRestResource(const FTressFXMeshDatas& InTressFXData) :
	TressFXMeshDatas(InTressFXData)
{}

void FTressFXMeshRestResource::InitRHI(FRHICommandListBase& )
{
	if (GUsingNullRHI) { return; }
	FMemMark Mark(FMemStack::Get());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRDGBuilder GraphBuilder(RHICmdList);

	TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXMeshDatas.Vertices, sizeof(FTressFXMeshVertexData), MeshRestVertexBuffer, TEXT("TressFXMesh_RestVertexBuffer"));

	TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXMeshDatas.BoneSkinningDatas, sizeof(FTressFXBoneSkinningData), MeshBoneSkinningBuffer, TEXT("TressFXMesh_BoneSkinningBuffer"));

	TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXMeshDatas.BoneIndexDatas, sizeof(FTressFXBoneIndexData), MeshBoneIndexBuffer, TEXT("TressFXMesh_BoneIndexBuffer"));

	TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXMeshDatas.Indices, sizeof(uint32), MeshRestIndicesBuffer, TEXT("TressFXMesh_RestIndicesBuffer"));

#if WITH_EDITOR
	uint32 NumIndices = TressFXMeshDatas.Indices.Num();
	const uint32 Stride = sizeof(uint32);

	// Create index buffer. Fill buffer with initial data upon creation
	FRHIResourceCreateInfo CreateInfo(TEXT("CollisionMeshIndexBuffer"));
	IndicesBuffer = RHICmdList.CreateIndexBuffer(Stride, Stride*NumIndices, BUF_Static, CreateInfo);
	void* Buffer = RHICmdList.LockBuffer(IndicesBuffer, 0, NumIndices * Stride, RLM_WriteOnly);
	FMemory::Memcpy(Buffer, TressFXMeshDatas.Indices.GetData(), NumIndices * Stride);
	RHICmdList.UnlockBuffer(IndicesBuffer);
#endif

	GraphBuilder.Execute();
}

void FTressFXMeshRestResource::ReleaseRHI()
{
	MeshRestVertexBuffer.Release();

	MeshBoneSkinningBuffer.Release();
	MeshBoneIndexBuffer.Release();

	MeshRestIndicesBuffer.Release();

#if WITH_EDITOR
	IndicesBuffer.SafeRelease();
#endif
}

FTressFXMeshDeformedResource::FTressFXMeshDeformedResource(const FTressFXMeshDatas& InTressFXData, const FIntVector& InNumSDFCells)
	: NumSDFCells(InNumSDFCells),
	 TressFXMeshDatas(InTressFXData)	
{}

void FTressFXMeshDeformedResource::InitRHI(FRHICommandListBase& )
{
	if (GUsingNullRHI) { return; }
	FMemMark Mark(FMemStack::Get());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRDGBuilder GraphBuilder(RHICmdList);

	TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXMeshDatas.Vertices, sizeof(FTressFXMeshVertexData), MeshDeformedVertexBuffer, TEXT("TressFXMesh_DeformedVertexBuffer"));

	struct FIntermediateCollisionMeshBox
	{
		FIntVector4 Max;
		FIntVector4 Min;
	};
	TArray<FIntermediateCollisionMeshBox> ICMBox;
	ICMBox.AddZeroed();
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, ICMBox, sizeof(FIntermediateCollisionMeshBox), IntermediateCollisionMeshBoxBuffer, TEXT("TressFXMesh_IntermediateCollisionMeshBoxBuffer"));

	struct FCollisionMeshBox
	{
		FVector4f Min;
		FVector4f Max;
		FVector4f CellSize;
	};
	TArray<FCollisionMeshBox> CMBox;
	CMBox.AddZeroed();
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, CMBox, sizeof(FCollisionMeshBox), CollisionMeshBoxBuffer, TEXT("TressFXMesh_CollisionMeshBoxBuffer"));

	TArray<uint32> SDFArray;
	SDFArray.SetNumZeroed(NumSDFCells.X * NumSDFCells.Y * NumSDFCells.Z);
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, SDFArray, sizeof(uint32), SDFBuffer, TEXT("TressFXMesh_SDFBuffer"));

	TArray<uint32> SDFDistanceCounterArray;
	SDFDistanceCounterArray.SetNumZeroed(1);
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, SDFDistanceCounterArray, sizeof(uint32), SDFDistanceCounter, TEXT("TressFXMesh_SDFDistanceCounter"));

	TArray<uint32> SDFDistanceStartOffsetArray;
	SDFDistanceStartOffsetArray.SetNumZeroed(NumSDFCells.X* NumSDFCells.Y* NumSDFCells.Z);
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, SDFDistanceStartOffsetArray, sizeof(uint32), SDFDistanceStartOffsetBuffer, TEXT("TressFXMesh_SDFDistanceStartOffsetBuffer"));

	struct FDistanceAndCellIndex
	{
		float Distance;
		uint32 CellIndex;
		uint32 Next;
	};
	TArray<FDistanceAndCellIndex> DistArray;
	DistArray.AddZeroed(TressFXMeshDatas.GetNumIndices() * 30);
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, DistArray, sizeof(float) * 3, SDFDistanceBuffer, TEXT("TressFXMesh_SDFDistanceBuffer"));

#if WITH_EDITOR

	TArray<uint32> SDFDirectionCounterArray;
	SDFDirectionCounterArray.SetNumZeroed(1);
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, SDFDirectionCounterArray, sizeof(uint32), SDFDirectionCounter, TEXT("TressFXMesh_SDFDirectionCounter"));

	TArray<uint32> SDFDirectionStartOffsetArray;
	SDFDirectionStartOffsetArray.SetNumZeroed(NumSDFCells.X * NumSDFCells.Y * NumSDFCells.Z);
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, SDFDirectionStartOffsetArray, sizeof(uint32), SDFDirectionStartOffsetBuffer, TEXT("TressFXMesh_SDFDirectionStartOffsetBuffer"));


	struct FDirectionAndCellIndex
	{
		FVector3f Direction;
		float Distance;
		uint32 CellIndex;
		uint32 Next;
	};
	TArray<FDirectionAndCellIndex> DirArray;
	DirArray.AddZeroed(TressFXMeshDatas.GetNumIndices()*30);
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, DirArray, sizeof(float)*6, SDFDirectionBuffer, TEXT("TressFXMesh_SDFDirectionBuffer"));

#endif

	GraphBuilder.Execute();
}

void FTressFXMeshDeformedResource::ReleaseRHI()
{
	MeshDeformedVertexBuffer.Release();
	IntermediateCollisionMeshBoxBuffer.Release();
	CollisionMeshBoxBuffer.Release();
	SDFBuffer.Release();

#if WITH_EDITOR

	SDFDirectionCounter.Release();
	SDFDirectionStartOffsetBuffer.Release();
	SDFDirectionBuffer.Release();
#endif
}


FTressFXMTCollisionMeshResource::FTressFXMTCollisionMeshResource(class USkeletalMeshComponent* InSkeletalMeshComponent)
	: SkeletalMeshComponent(InSkeletalMeshComponent)
{

}

void FTressFXMTCollisionMeshResource::InitRHI(FRHICommandListBase& )
{
	if (GUsingNullRHI) { return; }
	FMemMark Mark(FMemStack::Get());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRDGBuilder GraphBuilder(RHICmdList);

	if (SkeletalMeshComponent)
	{
#if WITH_EDITOR
		FSkeletalMeshRenderData* RenderData = SkeletalMeshComponent->GetSkinnedAsset()->IsCompiling() ? nullptr : SkeletalMeshComponent->GetSkinnedAsset()->GetResourceForRendering();
#else
		FSkeletalMeshRenderData* RenderData = SkeletalMeshComponent->SkeletalMesh->GetResourceForRendering();
#endif
		const uint32 LODIndex = SkeletalMeshComponent->GetPredictedLODLevel();// RenderData->PendingFirstLODIdx;
		if (RenderData && RenderData->LODRenderData.Num() > 0)
		{
			FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];
			uint32 NumVertices = LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
			TArray<FVector3f> PositionArray;
			PositionArray.AddZeroed(NumVertices);

			InternalCreateStructuredBufferRDG_(GraphBuilder, PositionArray, sizeof(FVector3f), DeformedPositionBuffer, TEXT("TressFX_MTCollisionMeshDeformedPositionBuffer"));

			LODData.MultiSizeIndexContainer.GetIndexBuffer(IndexData);
			NumTriangles = IndexData.Num() / 3;
			InternalCreateStructuredBufferRDG_(GraphBuilder, IndexData, sizeof(uint32), RestIndicesBuffer, TEXT("TressFX_MTCollisionMeshRestIndicesBuffer"));

		}
	}

	GraphBuilder.Execute();
}

void FTressFXMTCollisionMeshResource::ReleaseRHI()
{
	DeformedPositionBuffer.Release();
	RestIndicesBuffer.Release();
}
