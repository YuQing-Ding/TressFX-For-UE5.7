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

struct TRESSFXCORE_API FTressFXMeshVertexData
{
	FVector4f Position;
	FVector4f Normal;
};

struct TRESSFXCORE_API FTressFXMeshDatas
{
	/** Serialize all the TressFX datas */
	void Serialize(FArchive& Ar);

	uint32 GetNumVertices() const { return Vertices.Num(); }
	uint32 GetNumIndices() const { return Indices.Num(); }


	TArray<FTressFXMeshVertexData> Vertices;
	TArray<FTressFXBoneSkinningData> BoneSkinningDatas;
	TArray<FTressFXBoneIndexData> BoneIndexDatas;
	TArray<uint32> Indices;

	FBox BoundingBox;
};
