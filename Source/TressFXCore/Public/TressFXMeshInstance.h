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
#include "TressFXCommon.h"
#include "TressFXInterface.h"
#include "TressFXMeshResources.h"


struct TRESSFXCORE_API FTressFXMeshGroupInstance
{
	FTressFXMeshDatas* TFXMeshData = nullptr;

	bool IsValid() const { return TFXMeshData != nullptr && TFXMeshData->GetNumVertices() > 0; }

	FTressFXMeshRestResource* MeshRestResource = nullptr;
	FTressFXMeshDeformedResource* MeshDeformedResource = nullptr;

	struct FDebug
	{
		uint32					ComponentId = ~0;

		bool					bEnableVisualizeMesh = false;
		bool					bEnableVisualizeMeshAABB = false;
		bool					bEnableVisualizeSDF = false;

		USkeletalMeshComponent* SkeletalComponent = nullptr;

	} Debug;

	FPrimitiveSceneProxy* PrimitiveSceneProxy = nullptr;

	FString					WorldName;

	EWorldType::Type				WorldType;
	class UTressFXSDFComponent*		ParentSDFComp = nullptr;
	FIntVector						NumSDFCells = FIntVector(5, 5, 5);
	int32							NumGridOffset = 1;
	float							CollisionMeshBoxMargin = 2.f;
	uint32							LocalSDFId = 0xFFFFFFFF;
	float							SDFCollisionMargin = 0.1f;

	FTressFXMTCollisionMeshResource*	MTCollisionMeshResource = nullptr;
	FBufferRHIRef						MTCollisionMeshIndicesBufferRHI = nullptr;
	uint32								MTCollisionMeshIndicesCount = 0;
	uint32								MTCollisionMeshVertexCount = 0;

};