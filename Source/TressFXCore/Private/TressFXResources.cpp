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
#include "TressFXResources.h"
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
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "CommonRenderResources.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshLODRenderData.h"



template<typename FormatType>
void TFXInternalCreateUploadStructuredBufferRDG(FRDGBuilder& GraphBuilder, const TArray<FormatType>& InData, uint32 InSizeInByte, FRDGExternalBufferTFX& Out, const TCHAR* DebugName)
{
	const uint32 DataCount = InData.Num();
	const uint32 DataSizeInBytes = InSizeInByte * DataCount;
	if (DataSizeInBytes == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("TressFX Create Buffer ERROR!!!,DataCount==0!!!!!."));
		Out.Buffer = nullptr;
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("%s"), DebugName);

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(InSizeInByte, DataCount);
	FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, DebugName, ERDGBufferFlags::MultiFrame);
	GraphBuilder.QueueBufferUpload(Buffer, InData.GetData(), DataSizeInBytes, ERDGInitialDataFlags::None);

	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out);
}

template<typename FormatType>
void TFXInternalCreateStructuredBufferRDG(FRDGBuilder& GraphBuilder, const TArray<FormatType>& InData, uint32 InSizeInByte, FRDGExternalBufferTFX& Out, const TCHAR* DebugName)
{
	const uint32 DataCount = InData.Num();
	const uint32 DataSizeInBytes = InSizeInByte * DataCount;
	if (DataSizeInBytes == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("TressFX Create Buffer ERROR!!!,DataCount==0!!!!!."));
		Out.Buffer = nullptr;
		return;
	}

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(InSizeInByte, DataCount);
	FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, DebugName, ERDGBufferFlags::MultiFrame);

	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out);
}

template<typename FormatType>
void TFXInternalCreateUploadStructuredBufferRDG(FRDGBuilder& GraphBuilder, const TArray<FormatType>& InData, uint32 InSizeInByte, int32 NumElements, FRDGExternalBufferTFX& Out, const TCHAR* DebugName)
{
	const uint32 DataCount = NumElements;// InData.Num();
	const uint32 DataSizeInBytes = InSizeInByte * DataCount;
	if (DataSizeInBytes == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("TressFX Create Buffer ERROR!!!,DataCount==0!!!!!."));
		Out.Buffer = nullptr;
		return;
	}


	FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(InSizeInByte, DataCount);
	FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, DebugName, ERDGBufferFlags::MultiFrame);
	GraphBuilder.QueueBufferUpload(Buffer, InData.GetData(), DataSizeInBytes, ERDGInitialDataFlags::None);

	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out);
}

template<typename FormatType>
void TFXInternalCreateStructuredBufferRDG(FRDGBuilder& GraphBuilder, const TArray<FormatType>& InData, uint32 InSizeInByte, int32 NumElements, FRDGExternalBufferTFX& Out, const TCHAR* DebugName)
{
	const uint32 DataCount = NumElements;// InData.Num();
	const uint32 DataSizeInBytes = InSizeInByte * DataCount;
	if (DataSizeInBytes == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("TressFX Create Buffer ERROR!!!,DataCount==0!!!!!."));
		Out.Buffer = nullptr;
		return;
	}

	FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(InSizeInByte, DataCount);
	FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(Desc, DebugName, ERDGBufferFlags::MultiFrame);

	ConvertToExternalBufferWithViews(GraphBuilder, Buffer, Out);
}


FArchive& operator<<(FArchive& Ar, FTressFXClusterCullingData::FTressFXClusterLODInfo& Info);

FTressFXClusterCullingData::FTressFXClusterCullingData()
{

}

void FTressFXClusterCullingData::Reset()
{
	*this = FTressFXClusterCullingData();
}

void FTressFXClusterCullingData::Serialize(FArchive& Ar)
{
	Ar << ClusterCount;
	Ar << VertexCount;
	Ar << ClusterLODInfos;
}

FTressFXClusterCullingResource::FTressFXClusterCullingResource(const FTressFXClusterCullingData& InData)
	: Data(InData)
{

}


void FTressFXClusterCullingResource::InitRHI(FRHICommandListBase& )
{
	if (GUsingNullRHI) { return; }
	FMemMark Mark(FMemStack::Get());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRDGBuilder GraphBuilder(RHICmdList);

	TFXInternalCreateStructuredBufferRDG(GraphBuilder, Data.ClusterLODInfos, sizeof(FTressFXClusterCullingData::FTressFXClusterLODInfo), ClusterLODInfoBuffer, TEXT("TressFXClusterCulling_ClusterLODInfoBuffer"));

	GraphBuilder.Execute();
}

void FTressFXClusterCullingResource::ReleaseRHI()
{
	ClusterLODInfoBuffer.Release();
}


FTressFXGuidesRestResource::FTressFXGuidesRestResource(const FTressFXDatas& InTressFXData) :
	TressFXData(InTressFXData)
{}

void FTressFXGuidesRestResource::InitRHI(FRHICommandListBase& )
{
	if (GUsingNullRHI) { return; }
	FMemMark Mark(FMemStack::Get());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRDGBuilder GraphBuilder(RHICmdList);

	if(TressFXData.StrandsPoints.PointsPosition.Num())
		TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXData.StrandsPoints.PointsPosition, sizeof(FVector4f), GuidesRestPositionBuffer, TEXT("TressFXGuides_RestPositionBuffer"));

	if (TressFXData.StrandsPoints.PointsLength.Num())
		TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXData.StrandsPoints.PointsLength, sizeof(float), GuidesRestLengthBuffer, TEXT("TressFXGuides_RestLengthBuffer"));

	if(TressFXData.SimData.BoneSkinningDatas.Num())
	{
		TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXData.SimData.BoneSkinningDatas, sizeof(FTressFXBoneSkinningData), GuidesBoneSkinningBuffer, TEXT("TressFXGuides_BoneSkinningBuffer"));
	}
	else
	{
		FTressFXBoneSkinningData DefaultSkinningData;
		DefaultSkinningData.IndexBone = 0;
		DefaultSkinningData.Weight = 1.0f;
		TArray<FTressFXBoneSkinningData> DefaultSkinningDatas;
		DefaultSkinningDatas.Add(DefaultSkinningData);
		TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, DefaultSkinningDatas, sizeof(FTressFXBoneSkinningData), GuidesBoneSkinningBuffer, TEXT("TressFXGuides_DefaultBoneSkinningBuffer"));
	}

	if (TressFXData.SimData.BoneIndexDatas.Num())
	{
		TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXData.SimData.BoneIndexDatas, sizeof(FTressFXBoneIndexData), GuidesBoneIndexBuffer, TEXT("TressFXGuides_BoneIndexBuffer"));
	}
	else if (TressFXData.NumVerticesPerStrand > 0)
	{
		const uint32 NumGuideStrands = TressFXData.StrandsPoints.PointsPosition.Num() / TressFXData.NumVerticesPerStrand;
		TArray<FTressFXBoneIndexData> DefaultBoneIndexDatas;
		DefaultBoneIndexDatas.SetNum(NumGuideStrands);
		for (FTressFXBoneIndexData& BoneIndexData : DefaultBoneIndexDatas)
		{
			BoneIndexData.StartIdx = 0;
			BoneIndexData.BoneCount = 1;
		}
		TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, DefaultBoneIndexDatas, sizeof(FTressFXBoneIndexData), GuidesBoneIndexBuffer, TEXT("TressFXGuides_DefaultBoneIndexBuffer"));
	}

	if (TressFXData.StrandsCurves.CurvesRootUV.Num())
		TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXData.StrandsCurves.CurvesRootUV, sizeof(FVector2f), GuidesRootUVBuffer, TEXT("TressFXGuides_RootUVBuffer"));

	if (TressFXData.RandomCurveIndexArr.Num())
		TFXInternalCreateUploadStructuredBufferRDG(GraphBuilder, TressFXData.RandomCurveIndexArr, sizeof(uint32), RandomCurveIndexArrBuffer, TEXT("TressFX_RandomCurveIndexArrBuffer"));

	GraphBuilder.Execute();
}

void FTressFXGuidesRestResource::ReleaseRHI()
{
	GuidesRestPositionBuffer.Release();
	GuidesRestLengthBuffer.Release();
	GuidesBoneSkinningBuffer.Release();
	GuidesBoneIndexBuffer.Release();
	GuidesRootUVBuffer.Release();
}

FTressFXGuidesBindingResource::FTressFXGuidesBindingResource(const FTressFXMorphTargetBindingLODData& InTressFXBindingData)
	: TressFXBindingData(InTressFXBindingData)
{

}

void FTressFXGuidesBindingResource::InitRHI(FRHICommandListBase& )
{
	if (GUsingNullRHI) { return; }
	FMemMark Mark(FMemStack::Get());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRDGBuilder GraphBuilder(RHICmdList);

	if (TressFXBindingData.RootBindingTriangles.Num())
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, TressFXBindingData.RootBindingTriangles, sizeof(FTressFXMorphTargetBindingLODData::FGuideRootTriangle), GuidesRootBindingBuffer, TEXT("TressFXGuides_RootBindingBuffer"));

	if (TressFXBindingData.LocalToGlobalVertexIndexMap.Num())
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, TressFXBindingData.LocalToGlobalVertexIndexMap, sizeof(uint32), GuidesLocalToGlobalVertexIndexMapBuffer, TEXT("TressFXGuides_LocalToGlobalVertexIndexMapBuffer"));

	GraphBuilder.Execute();
}

void FTressFXGuidesBindingResource::ReleaseRHI()
{
	GuidesRootBindingBuffer.Release();
	GuidesLocalToGlobalVertexIndexMapBuffer.Release();
}

struct FStrandLevelData
{
	FVector4f SkinningQuat;
	FVector4f VspQuat;
	FVector4f VspTranslation;
};

FTressFXGuidesDeformedResource::FTressFXGuidesDeformedResource(const FTressFXDatas& InTressFXData) :
	TressFXData(InTressFXData)
{}

void FTressFXGuidesDeformedResource::InitRHI(FRHICommandListBase& )
{

	if (GUsingNullRHI) { return; }
	FMemMark Mark(FMemStack::Get());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRDGBuilder GraphBuilder(RHICmdList);

	const uint32 VertexCount = TressFXData.StrandsPoints.PointsPosition.Num();
	{
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, TressFXData.StrandsPoints.PointsPosition, sizeof(FVector4f), GuidesDeformedPositionBuffer[0], TEXT("TressFXGuides_DeformedPositionBuffer"));
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, TressFXData.StrandsPoints.PointsPosition, sizeof(FVector4f), GuidesDeformedPositionBuffer[1], TEXT("TressFXGuides_DeformedPrevPositionBuffer"));
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, TressFXData.StrandsPoints.PointsPosition, sizeof(FVector4f), GuidesDeformedPositionBuffer[2], TEXT("TressFXGuides_DeformedPrevPrevPositionBuffer"));

#if WITH_EDITOR

		TFXInternalCreateStructuredBufferRDG(GraphBuilder, TressFXData.StrandsPoints.PointsPosition, sizeof(FVector4f), GuidesDeformedPositionBufferDebug, TEXT("TressFXGuides_DeformedPositionBufferDebug"));

#endif
	}

	if (TressFXData.NumVerticesPerStrand > 0)
	{
		const uint32 GuidesStrandsCount = VertexCount / TressFXData.NumVerticesPerStrand;
		TArray<FStrandLevelData> StrandLevelDatas;
		StrandLevelDatas.AddZeroed(GuidesStrandsCount);
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, StrandLevelDatas, sizeof(FStrandLevelData), GuidesDeformedStrandLevelDataBuffer, TEXT("TressFXGuides_StrandLevelDataBuffer"));
	}

	GraphBuilder.Execute();
}

void FTressFXGuidesDeformedResource::ReleaseRHI()
{

	GuidesDeformedPositionBuffer[0].Release();
	GuidesDeformedPositionBuffer[1].Release();
	GuidesDeformedPositionBuffer[2].Release();

#if WITH_EDITOR

	GuidesDeformedPositionBufferDebug.Release();

#endif

	GuidesDeformedStrandLevelDataBuffer.Release();
}

FTressFXStrandsResource::FTressFXStrandsResource(const FTressFXDatas& InTressFXDatas, 
	const FTressFXClusterCullingData& InClusterCullingData, 
	const uint32 InNumFollowStrands,
	const float InMaxRadiusAroundGuide)
	: NumFollowStrands(InNumFollowStrands),
	MaxRadiusAroundGuide(InMaxRadiusAroundGuide),
	TressFXData(InTressFXDatas),
	ClusterCullingData(InClusterCullingData)
{}

static float GetRandom(float Min, float Max)
{
	return ((float(rand()) / float(RAND_MAX)) * (Max - Min)) + Min;
}

static FVector3f GetTressFXSafeTangent(const TArray<FVector4f>& Positions, uint32 StrandStartIndex, uint32 VertexIndexInStrand, uint32 NumVerticesPerStrand)
{
	const uint32 CurrentIndex = StrandStartIndex + VertexIndexInStrand;
	const uint32 PreviousIndex = StrandStartIndex + (VertexIndexInStrand > 0 ? VertexIndexInStrand - 1 : VertexIndexInStrand);
	const uint32 NextIndex = StrandStartIndex + (VertexIndexInStrand + 1 < NumVerticesPerStrand ? VertexIndexInStrand + 1 : VertexIndexInStrand);

	const FVector4f& PreviousPosition = Positions[PreviousIndex];
	const FVector4f& NextPosition = Positions[NextIndex];
	FVector3f Tangent = FVector3f(
		NextPosition.X - PreviousPosition.X,
		NextPosition.Y - PreviousPosition.Y,
		NextPosition.Z - PreviousPosition.Z).GetSafeNormal();
	if (Tangent.IsNearlyZero())
	{
		Tangent = FVector3f(0.0f, 0.0f, 1.0f);
	}

	return Tangent;
}

static FVector3f GetTressFXSafePerpendicular(const FVector3f& Tangent)
{
	const FVector3f Reference = FMath::Abs(Tangent.Z) < 0.9f ? FVector3f(0.0f, 0.0f, 1.0f) : FVector3f(1.0f, 0.0f, 0.0f);
	FVector3f Perpendicular = FVector3f::CrossProduct(Reference, Tangent).GetSafeNormal();
	if (Perpendicular.IsNearlyZero())
	{
		Perpendicular = FVector3f(1.0f, 0.0f, 0.0f);
	}
	return Perpendicular;
}

void FTressFXStrandsResource::InitRHI(FRHICommandListBase& )
{
	if (GUsingNullRHI) { return; }
	FMemMark Mark(FMemStack::Get());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRDGBuilder GraphBuilder(RHICmdList);

	uint32 VertexCount = GetVertexCount();

	TArray<FVector4f> Positions;
	Positions.SetNumZeroed(VertexCount);
	const uint32 NumVerticesPerStrand = TressFXData.NumVerticesPerStrand;
	const uint32 NumGuideStrands = NumVerticesPerStrand > 0 ? TressFXData.StrandsPoints.PointsPosition.Num() / NumVerticesPerStrand : 0;
	const uint32 NumRenderStrands = NumVerticesPerStrand > 0 ? VertexCount / NumVerticesPerStrand : 0;

	if (NumGuideStrands > 0 && NumVerticesPerStrand > 0)
	{
		for (uint32 RenderStrandIndex = 0; RenderStrandIndex < NumRenderStrands; ++RenderStrandIndex)
		{
			const uint32 SourceStrandIndex = RenderStrandIndex % NumGuideStrands;
			const uint32 SourceStartIndex = SourceStrandIndex * NumVerticesPerStrand;
			const uint32 DestinationStartIndex = RenderStrandIndex * NumVerticesPerStrand;

			for (uint32 VertexIndex = 0; VertexIndex < NumVerticesPerStrand; ++VertexIndex)
			{
				Positions[DestinationStartIndex + VertexIndex] = TressFXData.StrandsPoints.PointsPosition[SourceStartIndex + VertexIndex];
			}
		}
	}

	{
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, Positions, sizeof(FVector4f), StrandsDeformedPositionBuffer[0], TEXT("TressFXStrands_DeformedPositionBuffer0"));
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, Positions, sizeof(FVector4f), StrandsDeformedPositionBuffer[1], TEXT("TressFXStrands_DeformedPositionBuffer1"));
	}

	if (ClusterCullingData.IsValid())
	{
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, ClusterCullingData.ClusterLODInfos, sizeof(FTressFXClusterCullingData::FTressFXClusterLODInfo), ClusterLODInfoBuffer, TEXT("TressFXClusterCulling_ClusterLODInfoBuffer"));
	}

	TArray<FVector3f> Tangents;
	Tangents.SetNumZeroed(VertexCount*2);
	if (NumRenderStrands > 0 && NumVerticesPerStrand > 0)
	{
		for (uint32 RenderStrandIndex = 0; RenderStrandIndex < NumRenderStrands; ++RenderStrandIndex)
		{
			const uint32 StrandStartIndex = RenderStrandIndex * NumVerticesPerStrand;
			for (uint32 VertexIndex = 0; VertexIndex < NumVerticesPerStrand; ++VertexIndex)
			{
				const uint32 VertexBufferIndex = StrandStartIndex + VertexIndex;
				const FVector3f TangentZ = GetTressFXSafeTangent(Positions, StrandStartIndex, VertexIndex, NumVerticesPerStrand);
				const FVector3f TangentX = GetTressFXSafePerpendicular(TangentZ);
				Tangents[VertexBufferIndex * 2 + 0] = TangentX;
				Tangents[VertexBufferIndex * 2 + 1] = TangentZ;
			}
		}
	}
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, Tangents, sizeof(FVector3f), TangentBuffer, TEXT("TressFXStrands_TangentBuffer"));

	TArray<uint32> StrandsIDArray;
	StrandsIDArray.SetNumZeroed(TressFXData.NumStrandsToRender * (1 + NumFollowStrands));
	for (uint32 StrandIndex = 0; StrandIndex < uint32(StrandsIDArray.Num()); ++StrandIndex)
	{
		StrandsIDArray[StrandIndex] = NumGuideStrands > 0 ? StrandIndex % NumGuideStrands : 0;
	}
	TFXInternalCreateStructuredBufferRDG(GraphBuilder, StrandsIDArray, sizeof(uint32), StrandsIDBuffer, TEXT("TressFXStrands_StrandsIDBuffer"));

	if (NumFollowStrands > 0)
	{
		TArray<FVector4f> FollowRootOffsets;
		uint32 NumStrandsToRender = TressFXData.NumStrandsToRender * NumFollowStrands;
		FollowRootOffsets.SetNumZeroed(NumStrandsToRender);
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, FollowRootOffsets, sizeof(FVector4f), FollowRootOffsetBuffer, TEXT("TressFXStrands_FollowRootOffsetBuffer"));
		
		
		FollowRootRandoms.SetNumZeroed(NumStrandsToRender);
		for (uint32 i = 0; i < NumStrandsToRender; ++i)
		{
			FollowRootRandoms[i].X = GetRandom(-MaxRadiusAroundGuide,MaxRadiusAroundGuide);
			FollowRootRandoms[i].Y = GetRandom(-MaxRadiusAroundGuide,MaxRadiusAroundGuide);
		}
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, FollowRootRandoms, sizeof(FVector2f), FollowRootRandomBuffer, TEXT("TressFXStrands_FollowRootRandomBuffer"));

	}
	else
	{
		TArray<FVector4f> FollowRootOffsets;
		uint32 NumStrandsToRender = 1;
		FollowRootOffsets.SetNumZeroed(NumStrandsToRender);
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, FollowRootOffsets, sizeof(FVector4f), FollowRootOffsetBuffer, TEXT("TressFXStrands_FollowRootOffsetBufferZero"));

		FollowRootRandoms.SetNumZeroed(NumStrandsToRender);
		TFXInternalCreateStructuredBufferRDG(GraphBuilder, FollowRootRandoms, sizeof(FVector2f), FollowRootRandomBuffer, TEXT("TressFXStrands_FollowRootRandomBufferZero"));

	}

	GraphBuilder.Execute();
}

void FTressFXStrandsResource::ReleaseRHI()
{
	StrandsDeformedPositionBuffer[0].Release();
	StrandsDeformedPositionBuffer[1].Release();
	
	ClusterLODInfoBuffer.Release();
	
	TangentBuffer.Release();
	StrandsIDBuffer.Release();

	FollowRootOffsetBuffer.Release();
	FollowRootRandomBuffer.Release();
	
}

bool FTressFXStrandsResource::NeedsToUpdateTangent()
{
	return true;
	//{
	//	const bool bInit = bInitializedTangent;
	//	bInitializedTangent = false;
	//	return bInit;
	//}
}

#if WITH_EDITOR

FTressFXMorphTargetMeshResource::FTressFXMorphTargetMeshResource(class USkeletalMeshComponent* InSkeletalMeshComponent)
	: SkeletalMeshComponent(InSkeletalMeshComponent)
{

}

void FTressFXMorphTargetMeshResource::InitRHI(FRHICommandListBase& )
{
	if (GUsingNullRHI) { return; }
	FMemMark Mark(FMemStack::Get());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	FRDGBuilder GraphBuilder(RHICmdList);
	
	if (SkeletalMeshComponent)
	{
		FSkeletalMeshRenderData* RenderData = SkeletalMeshComponent->GetSkinnedAsset()->GetResourceForRendering();

		const uint32 LODIndex = SkeletalMeshComponent->GetPredictedLODLevel();// RenderData->PendingFirstLODIdx;
		if (RenderData && RenderData->LODRenderData.Num() > 0)
		{
			FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];
			uint32 NumVertices = LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
			TArray<FVector> PositionArray;
			PositionArray.AddZeroed(NumVertices);

			TFXInternalCreateStructuredBufferRDG(GraphBuilder, PositionArray, sizeof(FVector), MTMeshDeformedPositionBuffer, TEXT("TressFX_MTMeshDeformedPositionBuffer"));
		}
	}

	GraphBuilder.Execute();
}

void FTressFXMorphTargetMeshResource::ReleaseRHI()
{
	MTMeshDeformedPositionBuffer.Release();
}

#endif
