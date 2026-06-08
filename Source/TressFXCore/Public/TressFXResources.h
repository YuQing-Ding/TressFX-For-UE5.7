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

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "RenderResource.h"
#include "TressFXDatas.h"
#include "TressFXInterface.h"


inline uint32 GetBufferTotalNumBytes(const FRDGExternalBufferTFX& In)
{
	return In.Buffer ? In.Buffer->Desc.GetSize() : 0;
}

struct FTressFXClusterCullingResource : public FRenderResource
{
	FTressFXClusterCullingResource(const FTressFXClusterCullingData& Data);

	/* Init the buffer */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/* Release the buffer */
	virtual void ReleaseRHI() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FTressFXClusterResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(ClusterInfoBuffer);
		Total += GetBufferTotalNumBytes(ClusterLODInfoBuffer);
		Total += GetBufferTotalNumBytes(VertexToClusterIdBuffer);
		Total += GetBufferTotalNumBytes(ClusterVertexIdBuffer);
		return Total;
	}

	/* Cluster info buffer */
	FRDGExternalBufferTFX ClusterInfoBuffer;
	FRDGExternalBufferTFX ClusterLODInfoBuffer;

	/* VertexId => ClusterId to know which AABB to contribute to*/
	FRDGExternalBufferTFX VertexToClusterIdBuffer;

	/* Concatenated data for each cluster: list of VertexId pointed to by ClusterInfoBuffer */
	FRDGExternalBufferTFX ClusterVertexIdBuffer;

	const FTressFXClusterCullingData& Data;
};

struct FTressFXGuidesRestResource : public FRenderResource
{
	FTressFXGuidesRestResource(const FTressFXDatas& InTressFXData);

	/* Init the buffer */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/* Release the buffer */
	virtual void ReleaseRHI() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FTressFXGuidesRestResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(GuidesRestPositionBuffer);
		Total += GetBufferTotalNumBytes(GuidesRestLengthBuffer);
		Total += GetBufferTotalNumBytes(GuidesBoneSkinningBuffer);
		Total += GetBufferTotalNumBytes(GuidesBoneIndexBuffer);
		Total += GetBufferTotalNumBytes(GuidesRootUVBuffer);
		return Total;
	}

	FRDGExternalBufferTFX  GuidesRestPositionBuffer;
	FRDGExternalBufferTFX  GuidesRestLengthBuffer;
	FRDGExternalBufferTFX  GuidesBoneSkinningBuffer;
	FRDGExternalBufferTFX  GuidesBoneIndexBuffer;
	FRDGExternalBufferTFX  GuidesRootUVBuffer;
	FRDGExternalBufferTFX  RandomCurveIndexArrBuffer;


	const FTressFXDatas& TressFXData;

	inline uint32 GetVertexCount() const { return TressFXData.StrandsPoints.PointsPosition.Num(); }
};

struct FTressFXGuidesBindingResource : public FRenderResource
{
	FTressFXGuidesBindingResource(const FTressFXMorphTargetBindingLODData& InTressFXBindingDatas);

	/* Init the buffer */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/* Release the buffer */
	virtual void ReleaseRHI() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FTressFXGuidesBindingResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(GuidesRootBindingBuffer);
		return Total;
	}

	FRDGExternalBufferTFX  GuidesRootBindingBuffer;
	FRDGExternalBufferTFX  GuidesLocalToGlobalVertexIndexMapBuffer;

	const uint32 GetBindingVertexCount() const { return TressFXBindingData.LocalToGlobalVertexIndexMap.Num(); }

	const FTressFXMorphTargetBindingLODData& TressFXBindingData;
};

struct FTressFXGuidesDeformedResource : public FRenderResource
{
	FTressFXGuidesDeformedResource(const FTressFXDatas& TressFXDatas);

	/* Init the buffer */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/* Release the buffer */
	virtual void ReleaseRHI() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FTressFXGuidesDeformedResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(GuidesDeformedPositionBuffer[0]);
		Total += GetBufferTotalNumBytes(GuidesDeformedPositionBuffer[1]);
		Total += GetBufferTotalNumBytes(GuidesDeformedPositionBuffer[2]);
		Total += GetBufferTotalNumBytes(GuidesDeformedStrandLevelDataBuffer);

#if WITH_EDITOR

		Total += GetBufferTotalNumBytes(GuidesDeformedPositionBufferDebug);

#endif

		return Total;
	}

	FRDGExternalBufferTFX GuidesDeformedPositionBuffer[3];

	FRDGExternalBufferTFX GuidesDeformedStrandLevelDataBuffer;

#if WITH_EDITOR

	FRDGExternalBufferTFX GuidesDeformedPositionBufferDebug;

#endif
	
	bool bResetPositions = true;

	bool bInitializedTangent = true;

	uint32 CurrentIndex = 0;

	enum EFrameType
	{
		Current=0,
		Previous,
		PreviousPrevious
	};

	inline FRDGExternalBufferTFX& GetBuffer(EFrameType T) { return GuidesDeformedPositionBuffer[T]; }

	const FTressFXDatas& TressFXData;
};

struct FTressFXStrandsResource : public FRenderResource
{
	FTressFXStrandsResource(const FTressFXDatas& InTressFXDatas, 
		const FTressFXClusterCullingData& InClusterCullingData, 
		const uint32 InNumFollowStrands,
		const float InMaxRadiusAroundGuide);

	/* Init the buffer */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/* Release the buffer */
	virtual void ReleaseRHI() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FTressFXStrandsResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(StrandsDeformedPositionBuffer[0]);
		Total += GetBufferTotalNumBytes(StrandsDeformedPositionBuffer[1]);
		Total += GetBufferTotalNumBytes(ClusterLODInfoBuffer);
		Total += GetBufferTotalNumBytes(TangentBuffer);
		Total += GetBufferTotalNumBytes(StrandsIDBuffer);
		Total += GetBufferTotalNumBytes(FollowRootOffsetBuffer);
		Total += GetBufferTotalNumBytes(FollowRootRandomBuffer);
		return Total;
	}

	FRDGExternalBufferTFX TangentBuffer;
	FRDGExternalBufferTFX StrandsIDBuffer;

	FRDGExternalBufferTFX StrandsDeformedPositionBuffer[2];
	bool bInitializedTangent = true;

	uint32 CurrentIndex = 0;

	enum EFrameType
	{
		Previous,
		Current
	};

	FRDGExternalBufferTFX FollowRootOffsetBuffer;
	FRDGExternalBufferTFX FollowRootRandomBuffer;
	TArray<FVector2f> FollowRootRandoms;

	const uint32 NumFollowStrands;
	const float MaxRadiusAroundGuide;

	FRDGExternalBufferTFX ClusterLODInfoBuffer;

	inline uint32 GetIndex(EFrameType T) const { return (T == EFrameType::Current) ? CurrentIndex : 1u - CurrentIndex; }
	inline FRDGExternalBufferTFX& GetBuffer(EFrameType T) { return StrandsDeformedPositionBuffer[GetIndex(T)]; }
	inline void SwapBuffer() { /*if (bDynamic)*/ { CurrentIndex = 1u - CurrentIndex; } }

	bool NeedsToUpdateTangent();

	inline uint32 GetVertexCount() const { return TressFXData.NumStrandsToRender * TressFXData.NumVerticesPerStrand * (1+NumFollowStrands); }

	const FTressFXDatas& TressFXData;
	const FTressFXClusterCullingData& ClusterCullingData;
};

#if WITH_EDITOR

struct FTressFXMorphTargetMeshResource : public FRenderResource
{
	FTressFXMorphTargetMeshResource(class USkeletalMeshComponent* InSkeletalMeshComponent);

	/* Init the buffer */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/* Release the buffer */
	virtual void ReleaseRHI() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FTressFXMorphTargetMeshResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytes(MTMeshDeformedPositionBuffer);
		return Total;
	}

	FRDGExternalBufferTFX  MTMeshDeformedPositionBuffer;

	class USkeletalMeshComponent* SkeletalMeshComponent;
};



#endif