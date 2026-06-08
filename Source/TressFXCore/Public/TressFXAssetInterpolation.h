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
#include "UObject/PerPlatformProperties.h"
#include "TressFXAssetInterpolation.generated.h"



USTRUCT(BlueprintType)
struct TRESSFXCORE_API FTressFXInterpolationSettings
{
	GENERATED_USTRUCT_BODY()

	FTressFXInterpolationSettings();

	/** Flag to override the imported guides with generated guides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InterpolationSettings", meta = (ToolTip = "If checked, override imported guides with generated ones."))
		bool bOverrideGuides;

	/** Density factor for converting hair into guide curve if no guides are provided. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InterpolationSettings", meta = (ClampMin = "0", UIMin = "0", UIMax = "1.0"))
		float HairToGuideDensity;

	/** Randomize which guides affect a given hair strand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InterpolationSettings")
		bool bRandomizeGuide;

	/** Force a hair strand to be affected by a unique guide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InterpolationSettings")
		bool bUseUniqueGuide;

	bool operator==(const FTressFXInterpolationSettings& A) const;
};

USTRUCT(BlueprintType)
struct TRESSFXCORE_API FTressFXGroupsInterpolation
{
	GENERATED_USTRUCT_BODY()

	FTressFXGroupsInterpolation();


	UPROPERTY(EditAnywhere, Category = "InterpolationSettings", meta = (ToolTip = "Interpolation settings"))
	FTressFXInterpolationSettings InterpolationSettings;

};

USTRUCT(BlueprintType)
struct TRESSFXCORE_API FTressFXLODSettings
{
	GENERATED_BODY();
	
	/** Reduce the number of hair strands */
	UPROPERTY(EditAnywhere, Category = "DecimationSettings", meta = (ToolTip = "Reduce the number of strands in a uniform manner", ClampMin = "0", UIMin = "0", UIMax = "1.0"))
	float StrandsDecimation = 1;

	/** Screen size at which this LOD should be enabled */
	UPROPERTY(EditAnywhere, Category = "DecimationSettings", meta = (ToolTip = "Reduce the number of strands in a uniform manner", ClampMin = "0", UIMin = "0", UIMax = "1.0"))
	float ScreenSize = 1;


};

USTRUCT(BlueprintType)
struct TRESSFXCORE_API FTressFXGroupsLOD
{
	GENERATED_USTRUCT_BODY()

	FTressFXGroupsLOD();

	//UPROPERTY(EditAnywhere, Category = "LODSettings", meta = (ToolTip = "Enable Continuous LOD"))
	bool EnableContinuousLOD = false;

	UPROPERTY(EditAnywhere, Category = "LODSettings", meta = (ToolTip = "Reduce the number of strands in a uniform manner", ClampMin = "0", UIMin = "0", UIMax = "1.0"))
	TArray<FTressFXLODSettings> LODs;

};
