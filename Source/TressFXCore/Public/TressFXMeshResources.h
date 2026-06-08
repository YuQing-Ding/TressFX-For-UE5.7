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
#include "TressFXMeshDatas.h"
#include "TressFXInterface.h"


inline uint32 GetBufferTotalNumBytesMesh(const FRDGExternalBufferTFX& In)
{
	return In.Buffer ? In.Buffer->Desc.GetSize() : 0;
}

struct FTressFXMeshRestResource : public FRenderResource
{
	FTressFXMeshRestResource(const FTressFXMeshDatas& TressFXDatas);

	/* Init the buffer */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/* Release the buffer */
	virtual void ReleaseRHI() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FTressFXMeshRestResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytesMesh(MeshRestVertexBuffer);
		Total += GetBufferTotalNumBytesMesh(MeshBoneSkinningBuffer);
		Total += GetBufferTotalNumBytesMesh(MeshBoneIndexBuffer);
		Total += GetBufferTotalNumBytesMesh(MeshRestIndicesBuffer);
		return Total;
	}

	FRDGExternalBufferTFX  MeshRestVertexBuffer;
	FRDGExternalBufferTFX  MeshBoneSkinningBuffer;
	FRDGExternalBufferTFX  MeshBoneIndexBuffer;

	FRDGExternalBufferTFX  MeshRestIndicesBuffer;

#if WITH_EDITOR
	FBufferRHIRef  IndicesBuffer;
#endif

	const FTressFXMeshDatas& TressFXMeshDatas;

	//inline uint32 GetVertexCount() const { return 0; }
};

struct FTressFXMeshDeformedResource : public FRenderResource
{
	FTressFXMeshDeformedResource(const FTressFXMeshDatas& TressFXDatas, const FIntVector& InSDFNumCells);

	/* Init the buffer */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/* Release the buffer */
	virtual void ReleaseRHI() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FTressFXMeshDeformedResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytesMesh(MeshDeformedVertexBuffer);
		Total += GetBufferTotalNumBytesMesh(IntermediateCollisionMeshBoxBuffer);
		Total += GetBufferTotalNumBytesMesh(CollisionMeshBoxBuffer);
		Total += GetBufferTotalNumBytesMesh(SDFBuffer);
		Total += GetBufferTotalNumBytesMesh(SDFDistanceCounter);
		Total += GetBufferTotalNumBytesMesh(SDFDistanceStartOffsetBuffer);
		Total += GetBufferTotalNumBytesMesh(SDFDistanceBuffer);

#if WITH_EDITOR

		Total += GetBufferTotalNumBytesMesh(SDFDirectionCounter);
		Total += GetBufferTotalNumBytesMesh(SDFDirectionStartOffsetBuffer);
		Total += GetBufferTotalNumBytesMesh(SDFDirectionBuffer);

#endif

		return Total;
	}

	FRDGExternalBufferTFX MeshDeformedVertexBuffer;
	FRDGExternalBufferTFX IntermediateCollisionMeshBoxBuffer;
	FRDGExternalBufferTFX CollisionMeshBoxBuffer;
	FRDGExternalBufferTFX SDFBuffer;
	FRDGExternalBufferTFX SDFDistanceCounter;
	FRDGExternalBufferTFX SDFDistanceStartOffsetBuffer;
	FRDGExternalBufferTFX SDFDistanceBuffer;

#if WITH_EDITOR

	FRDGExternalBufferTFX SDFDirectionCounter;
	FRDGExternalBufferTFX SDFDirectionStartOffsetBuffer;
	FRDGExternalBufferTFX SDFDirectionBuffer;

#endif

	//FVector PositionOffset;
	FIntVector NumSDFCells;

	const FTressFXMeshDatas& TressFXMeshDatas;
};


struct FTressFXMTCollisionMeshResource : public FRenderResource
{
	FTressFXMTCollisionMeshResource(class USkeletalMeshComponent* InSkeletalMeshComponent);

	/* Init the buffer */
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;

	/* Release the buffer */
	virtual void ReleaseRHI() override;

	/* Get the resource name */
	virtual FString GetFriendlyName() const override { return TEXT("FTressFXMTCollisionMeshResource"); }

	/* Return the memory size for GPU resources */
	uint32 GetResourcesSize() const
	{
		uint32 Total = 0;
		Total += GetBufferTotalNumBytesMesh(DeformedPositionBuffer);
		Total += GetBufferTotalNumBytesMesh(RestIndicesBuffer);
		return Total;
	}

	FRDGExternalBufferTFX  DeformedPositionBuffer;
	FRDGExternalBufferTFX  RestIndicesBuffer;

	TArray<uint32> IndexData;
	uint32 NumTriangles = 0;

	class USkeletalMeshComponent* SkeletalMeshComponent;
};