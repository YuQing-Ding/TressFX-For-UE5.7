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

#include "TressFXMeshDatas.h"
#include "TressFXMeshResources.h"

#include "TressFXMeshAsset.generated.h"


struct TRESSFXCORE_API FTressFXMeshGroupData
{
	FTressFXMeshDatas TFXMeshData;

	bool HasValidData() const { return TFXMeshData.GetNumVertices() > 0; }

	FTressFXMeshRestResource* MeshRestResource;
	FTressFXMeshDeformedResource* MeshDeformedResource;

};


UCLASS(BlueprintType, hidecategories = (Object, "Hidden"))
class TRESSFXCORE_API UTressFXMeshAsset : public UObject, public IInterface_AssetUserData
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	/** Notification when anything changed */
	DECLARE_MULTICAST_DELEGATE(FOnTressFXMeshAssetChanged);
	DECLARE_MULTICAST_DELEGATE(FOnTressFXMeshAssetResourcesChanged);
	DECLARE_MULTICAST_DELEGATE(FOnTressFXMeshAsyncLoadFinished);
#endif

public:

	UPROPERTY(EditAnywhere, Category = "CollisionMesh", meta = (ToolTip = "Enable the Collision Mesh Visualization"))
	bool EnableVisualizeMesh = false;

	UPROPERTY(EditAnywhere, Category = "CollisionMesh", meta = (ToolTip = "Enable the Collision Mesh AABB Visualization"))
	bool EnableVisualizeMeshAABB = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CollisionMesh", meta = (ClampMin = "2.0", UIMin = "2.0", UIMax = "5.0", SliderExponent = 6))
	float CollisionMeshBoxMargin = 2.f;

	UPROPERTY(EditAnywhere, Category = "Mesh SDF", meta = (ToolTip = "Enable the SDF Visualization"))
	bool EnableVisualizeSDF = false;

	UPROPERTY(EditAnywhere, Category = "Mesh SDF", meta = (ClampMin = "2", UIMin = "2", UIMax = "10", ToolTip = "Set SDF Num Cells"))
	FIntVector NumSDFCells = FIntVector(5,5,5);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh SDF", meta = (ClampMin = "1", UIMin = "1", UIMax = "4", ToolTip = "SDF Grid Offset"))
	int32 NumGridOffset = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh SDF", meta = (ClampMin = "0.02", UIMin = "0.02", UIMax = "1.0", SliderExponent = 6))
	float SDFCollisionMargin = 0.05f;


	bool IsValid() const { return bIsInitialized; }

	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	FOnTressFXMeshAssetChanged& GetOnTressFXMeshAssetChanged() { return OnTressFXMeshAssetChanged; }
	FOnTressFXMeshAssetResourcesChanged& GetOnTressFXMeshAssetResourcesChanged() { return OnTressFXMeshAssetResourcesChanged; }
	FOnTressFXMeshAsyncLoadFinished& GetOnTressFXMeshAsyncLoadFinished() { return OnTressFXMeshAsyncLoadFinished; }

	/**  Part of Uobject interface  */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif // WITH_EDITOR

	void InitResources();
	void ReleaseResource();

	bool CacheDerivedDatas();


	FTressFXMeshGroupData MeshGroupData;

#if WITH_EDITOR
	FOnTressFXMeshAssetChanged OnTressFXMeshAssetChanged;
	FOnTressFXMeshAssetResourcesChanged OnTressFXMeshAssetResourcesChanged;
	FOnTressFXMeshAsyncLoadFinished OnTressFXMeshAsyncLoadFinished;
#endif

private:

	bool bIsInitialized = false;
};