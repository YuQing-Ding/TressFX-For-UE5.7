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

#include "Interfaces/Interface_AssetUserData.h"

#include "TressFXAssetRendering.h"
#include "TressFXAssetInterpolation.h"
#include "TressFXAssetSimulation.h"

#include "TressFXTypes.h"
#include "TressFXCommon.h"
#include "TressFXSkeletonInterface.h"
#include "TressFXResources.h"

#include "TressFXAsset.generated.h"


#define TRESSFX_MAX_INFLUENTIAL_BONE_COUNT 16



USTRUCT(BlueprintType)
struct TRESSFXCORE_API FTressFXGroupInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Info")
		int32 GroupID = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Curve Count"))
		int32 NumCurves = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Guide Count"))
		int32 NumGuides = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Curve Vertex Count"))
		int32 NumCurveVertices = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info", meta = (DisplayName = "Guide Vertex Count"))
		int32 NumGuideVertices = 0;
};

class TRESSFXCORE_API FTressFXAsset
{
public:
	FTressFXAsset();
	~FTressFXAsset();

	//Hair data from *.tfx
	TArray<FVector4f> positions;
	TArray<FVector2f> strandUV;
	TArray<FVector4f> tangents;
	TArray<FVector4f> followRootOffsets;
	TArray<int32>  strandTypes;
	TArray<float>  thicknessCoeffs;
	TArray<float>  restLengths;
	TArray<uint32>  triangleIndices;

	float maxRestLength = 0;

	// Bone skinning data from *.tfxbone
	TArray<FTressFXBoneSkinningData> boneSkinningData;
	TArray<FTressFXBoneIndexData> boneIndexData;

	// counts on hair data
	uint32 numStrandsInFile;
	int32 numTotalStrands;
	int32 numTotalVertices;
	int32 numVerticesPerStrand;
	int32 numGuideStrands;
	int32 numGuideVertices;
	int32 numFollowStrandsPerGuide;

	// Loads *.tfx hair data
	bool LoadHairData(FMemoryReader ioObject);

	//Generates follow hairs procedually.  If numFollowHairsPerGuideHair is zero, then this function won't do anything. 
	bool GenerateFollowHairs(int numFollowHairsPerGuideHair = 0, float tipSeparationFactor = 0, float maxRadiusAroundGuideHair = 0);

	//// Computes various parameters for simulation and rendering. After calling this function, data is ready to be passed to hair render data.
	bool ProcessAsset();

	// Loads *.tfxbone data
	bool LoadBoneData(const TressFXSkeletonInterface& skeletonData, FMemoryReader ioObject);
	bool LoadBoneData(const USkeletalMesh& skeletonData, FMemoryReader ioObject);

	// Resets variables and clears up allocated buffers.
	void Clear();

	inline uint32 GetNumHairSegments() { return numTotalStrands * (numVerticesPerStrand - 1); }
	inline uint32 GetNumHairTriangleIndices() { return 6 * GetNumHairSegments(); }
	inline uint32 GetNumHairLineIndices() { return 2 * GetNumHairSegments(); }
private:

	// Helper functions for ProcessAsset.
	void ComputeThicknessCoeffs();
	void ComputeStrandTangent();
	void ComputeRestLengths();
	void FillTriangleIndexArray();
};

USTRUCT(BlueprintType)
struct TRESSFXCORE_API FTressFXGroupsMaterial
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Material")
		UMaterialInterface* Material = nullptr;

	UPROPERTY(EditAnywhere, Category = "Material")
		FName SlotName = NAME_None;
};

/** Describe all data & resource for a TressFX asset's hair group */
struct TRESSFXCORE_API FTressFXGroupData
{
	FTressFXDatas TFXData;

	bool HasValidData() const { return TFXData.GetNumPoints() > 0; }

	FTressFXClusterCullingData ClusterCullingData;

	FTressFXGuidesRestResource* GuidesRestResource;
	FTressFXGuidesDeformedResource* GuidesDeformedResource;

	FTressFXStrandsResource* StrandsResource;
};

struct FProcessedTressFXDescription
{
	typedef TPair<FTressFXGroupInfo, FTressFXGroupData> FHairGroup;
	typedef TMap<int32, FHairGroup> FHairGroups;
	FHairGroups HairGroups;
	bool bCanUseClosestGuidesAndWeights = false;
	bool bHasUVData = false;
	bool IsValid() const;
};

UCLASS(BlueprintType, AutoExpandCategories = ("TressFXRendering", "TressFXSimulation", "TressFXInterpolation"), hidecategories = (Object, "Hidden"))
class TRESSFXCORE_API UTressFXAsset : public UObject, public IInterface_AssetUserData
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	/** Notification when anything changed */
	DECLARE_MULTICAST_DELEGATE(FOnTressFXAssetChanged);
	DECLARE_MULTICAST_DELEGATE(FOnTressFXAssetResourcesChanged);
	DECLARE_MULTICAST_DELEGATE(FOnTressFXAsyncLoadFinished);
#endif

public:
	
	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "Rendering", meta = (DisplayName = "Group"))
	FTressFXGroupsRendering TressFXGroupsRendering;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "Interpolation", meta = (DisplayName = "Group"))
	FTressFXGroupsInterpolation TressFXGroupsInterpolation;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "LODs", meta = (DisplayName = "Group"))
	FTressFXGroupsLOD TressFXGroupsLOD;

	UPROPERTY(EditAnywhere, EditFixedSize, Category = "Materials", meta = (DisplayName = "Group"))
	FTressFXGroupsMaterial TressFXGroupsMaterials;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "DefaultSimulationSettings", meta = (DisplayName = "Group"))
	FTressFXGroupsSimulation TressFXSimulationSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Simulation Settings Array for different animations")
	TArray<FTressFXGroupsSimulation> TressFXSimulationSettingsArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation and its Simulation Settings Map")
		TArray<FAnimationTressFXSimulationSettings> AnimationSimulationSettingsMap;

	/** Store strands/meshes data */
	TArray<FTressFXGroupData> TressFXGroupsData;

	int32 GetNumTressFXGroups() const;

	//~ Begin UObject Interface.
	//virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	FOnTressFXAssetChanged& GetOnTressFXAssetChanged() { return OnTressFXAssetChanged; }
	FOnTressFXAssetResourcesChanged& GetOnTressFXAssetResourcesChanged() { return OnTressFXAssetResourcesChanged; }
	FOnTressFXAsyncLoadFinished& GetOnTressFXAsyncLoadFinished() { return OnTressFXAsyncLoadFinished; }

	/**  Part of Uobject interface  */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif // WITH_EDITOR

	
	bool CacheDerivedData(uint32 GroupIndex, FProcessedTressFXDescription& ProcessedHairDescription);
	bool CacheDerivedDatas();
	bool CacheStrandsData(uint32 GroupIndex, FProcessedTressFXDescription& ProcessedHairDescription, FString& OutDerivedDataKey);

	bool IsValid() const { return bIsInitialized; }


	void InitResources();
	void InitGuideResources();
	void InitStrandsResources();

#if WITH_EDITOR
	void UpdateResource();
#endif
	void ReleaseResource();
	
	
	int32 GetMaterialIndex(FName MaterialSlotName) const;
	bool IsMaterialSlotNameValid(FName MaterialSlotName) const;
	bool IsMaterialUsed(int32 MaterialIndex) const;
	TArray<FName> GetMaterialSlotNames() const;


#if WITH_EDITOR
	FOnTressFXAssetChanged OnTressFXAssetChanged;
	FOnTressFXAssetResourcesChanged OnTressFXAssetResourcesChanged;
	FOnTressFXAsyncLoadFinished OnTressFXAsyncLoadFinished;
#endif

	UMaterialInterface* Strands_DefaultMaterial;
private:

	bool bIsInitialized = false;

	bool bRetryLoadFromGameThread = false;

};

