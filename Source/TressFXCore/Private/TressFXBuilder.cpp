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

#include "TressFXBuilder.h"
#include "TressFXAsset.h"




static void InternalBuildClusterData(
	const FTressFXDatas& InRenStrandsData,
	const float InGroomAssetRadius,
	const FTressFXGroupsLOD& InSettings,
	FTressFXClusterCullingData& Out)
{
	Out.ClusterCount = 1;
	Out.VertexCount = InRenStrandsData.GetNumPointsToRender();
	if (Out.VertexCount == 0)
		Out.VertexCount = InRenStrandsData.GetNumPoints();

	Out.ClusterLODInfos.AddDefaulted();
	Out.ClusterLODInfos[0].VertexCount0 = InRenStrandsData.GetNumPointsToRender();
	if (0 == Out.ClusterLODInfos[0].VertexCount0)
		Out.ClusterLODInfos[0].VertexCount0 = InRenStrandsData.GetNumPoints();
}

void FTressFXBuilder::BuildClusterData(UTressFXAsset* TressFXAsset, const float TressFXBoundRadius, uint32 GroupIndex)
{
	if (TressFXAsset)
	{
		FTressFXGroupData& GroupData = TressFXAsset->TressFXGroupsData[0];
		InternalBuildClusterData(
			GroupData.TFXData,
			TressFXBoundRadius,
			TressFXAsset->TressFXGroupsLOD,
			GroupData.ClusterCullingData);
	}
}

void FTressFXBuilder::BuildClusterData(UTressFXAsset* TressFXAsset, const FProcessedTressFXDescription& ProcessedHairDescription, uint32 GroupIndex)
{
	if (TressFXAsset)
	{
		const float TressFXBoundRadius = 1.f;// FTressFXBuilder::ComputeTressFXBoundRadius(ProcessedHairDescription);
		FTressFXBuilder::BuildClusterData(TressFXAsset, TressFXBoundRadius, GroupIndex);
	}
}

