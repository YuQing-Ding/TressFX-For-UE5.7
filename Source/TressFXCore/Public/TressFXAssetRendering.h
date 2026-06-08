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

#include "TressFXAssetRendering.generated.h"



USTRUCT(BlueprintType)
struct TRESSFXCORE_API FTressFXGroupsRendering
{
	GENERATED_BODY()

	FTressFXGroupsRendering();

	UPROPERTY(VisibleAnywhere, Category = "Strands", meta = (DisplayName = "Strands Count"))
	int32 NumStrands = 0;

	UPROPERTY(VisibleAnywhere, Category = "Strands", meta = (DisplayName = "Guides Count"))
	int32 NumGuides = 0;

	UPROPERTY(VisibleAnywhere, Category = "Strands", meta = (DisplayName = "Strands Vertex Count"))
	int32 NumStrandsVertices = 0;

	UPROPERTY(VisibleAnywhere, Category = "Strands", meta = (DisplayName = "Guides Vertex Count"))
	int32 NumGuidesVertices = 0;

	UPROPERTY(VisibleAnywhere, Category = "Strands", meta = (DisplayName = "Vertex Count Per Strand"))
	int32 NumVerticesPerStrand = 0;

	UPROPERTY(EditAnywhere, Category = "Strands")
	FName MaterialSlotName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strands", meta = (ClampMin = "0.0001", UIMin = "0.001", UIMax = "1.0"))
	float StrandsWidth = 0.0065f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strands", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "50.0"))
	float TranslucencyDensity = 1.f;

	UPROPERTY(EditAnywhere, Category = "Strands", meta = (ToolTip = "Visualize tangents on that group"))
	bool EnableVisualizeTangents = false;

	uint32 NumFollowStrands = 0;

	float TipSeparationFactorOfFollowStrands = 1.0f;

	float MaxRadiusAroundGuide = 0.3f;

};
