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

#include "TressFXBoneSkeletonInterface.h"
#include "TressFXCommon.h"
#include "Math/Matrix.h"

class USkeletalMeshComponent;
class FRawStaticIndexBuffer16or32Interface;

class SkeletonInterface : public TressFXSkeletonInterface
{
public:
    virtual unsigned int GetBoneIndexByName(const char* name) const;
    virtual const char* GetBoneNameByIndex(unsigned int index) const;

    virtual  unsigned int GetNumberOfBones() const;
    void SetSkeleton(USkeletalMeshComponent* skeleton);
    int GetDataSize();

	FMatrix GetComponentToWorld();
    TArray<FMatrix>  GetBoneMatrices();
	FBoxSphereBounds GetBoundingBox();

	int GetNumMeshVertices() const;
	int GetNumMeshTriangle() const;

	FRawStaticIndexBuffer16or32Interface* GetIndexBuffer();
    bool HasSkeleton() const { return Skeleton != nullptr; }

private:
    USkeletalMeshComponent* Skeleton;
};