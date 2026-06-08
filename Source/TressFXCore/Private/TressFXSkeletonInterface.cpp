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
#include "TressFXSkeletonInterface.h"
#include "TressFXCommon.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Components/SkeletalMeshComponent.h"

void SkeletonInterface::SetSkeleton(USkeletalMeshComponent* s)
{ 
    Skeleton = s; 
	if ( Skeleton != nullptr ) Skeleton->bUpdateJointsFromAnimation = true;
}

const char* SkeletonInterface::GetBoneNameByIndex(unsigned int index) const
{
    if (Skeleton == nullptr)
    {
        // You need to set skeleton first.
        return 0;
    }

	// This function does nothing for now because it gave me issues in 4.21
	// Wasn't using it anyways so shouldn't be a problem

	return 0;
}

FBoxSphereBounds SkeletonInterface::GetBoundingBox()
{
	return Skeleton->Bounds;
}

unsigned int SkeletonInterface::GetNumberOfBones() const
{
    if (Skeleton == nullptr)
    {
        // You need to set skeleton first.
        return 0;
    }

	USkinnedMeshComponent* SkeletonInt = Skeleton->LeaderPoseComponent.Get() ? Skeleton->LeaderPoseComponent.Get() : Skeleton;

    return SkeletonInt->GetNumBones();// GetReferenceSkeleton().GetNum();
	
}

unsigned int SkeletonInterface::GetBoneIndexByName(const char* name) const
{
    if (Skeleton == nullptr)
    {
        // You need to set skeleton first.
        return 0;
    }

	USkinnedMeshComponent* SkeletonInt = Skeleton->LeaderPoseComponent.Get() ? Skeleton->LeaderPoseComponent.Get() : Skeleton;

    unsigned int index = SkeletonInt->GetBoneIndex(name);// GetReferenceSkeleton().FindBoneIndex(name);
	
    return index;

}

int SkeletonInterface::GetDataSize()
{
    if (Skeleton == nullptr)
        return 0;

	USkinnedMeshComponent* SkeletonInt = Skeleton->LeaderPoseComponent.Get() ? Skeleton->LeaderPoseComponent.Get() : Skeleton;

    return SkeletonInt->GetComponentSpaceTransforms().Num() * sizeof(FMatrix);
	
}

FMatrix SkeletonInterface::GetComponentToWorld()
{
	USkinnedMeshComponent* SkeletonInt = Skeleton->LeaderPoseComponent.Get() ? Skeleton->LeaderPoseComponent.Get() : Skeleton;

	return SkeletonInt->GetComponentToWorld().ToMatrixWithScale();
	
}

int SkeletonInterface::GetNumMeshVertices() const
{
	return Skeleton->GetSkeletalMeshRenderData()->LODRenderData[0].GetNumVertices();
}

int SkeletonInterface::GetNumMeshTriangle() const
{
	return Skeleton->GetSkeletalMeshRenderData()->LODRenderData[0].GetTotalFaces();
}

FRawStaticIndexBuffer16or32Interface* SkeletonInterface::GetIndexBuffer() 
{
	FSkeletalMeshLODRenderData* LODRenderData;
	LODRenderData = &Skeleton->GetSkeletalMeshRenderData()->LODRenderData[0];
	return LODRenderData->MultiSizeIndexContainer.GetIndexBuffer();
}
