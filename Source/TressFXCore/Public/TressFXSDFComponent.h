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
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Components/MeshComponent.h"
#include "TressFXMeshAsset.h"
#include "RHIDefinitions.h"
#include "LODSyncInterface.h"
#include "TressFXMeshInstance.h"

#include "TressFXSDFComponent.generated.h"


UCLASS(HideCategories = (Object, Physics, Activation, Mobility, "Components|Activation"), editinlinenew, meta = (BlueprintSpawnableComponent), ClassGroup = Rendering)
class TRESSFXCORE_API UTressFXSDFComponent : public UMeshComponent, public ILODSyncInterface
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "SDFMesh", meta = (ToolTip = "Enable the SDF Computation"))
	bool EnableSDF = true;

	UPROPERTY(EditAnywhere, Category = "SDFMesh", meta = (ToolTip = "SDF Component Local Id"))
	uint32 LocalSDFId = 0xFFFFFFFF;

	FTressFXMeshGroupInstance* CreateTressFXMeshGroupInstance();
	void DestroyTressFXMeshGroupInstance(FTressFXMeshGroupInstance* InMeshInstance);

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	//~ Begin UMeshComponent Interface.
	virtual void PostLoad() override;
	//~ End UMeshComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnAttachmentChanged() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void SendRenderTransform_Concurrent() override;
	//~ End UActorComponent Interface.

	///~ Begin ILODSyncInterface Interface.
	virtual int32 GetDesiredSyncLOD() const override;
	virtual int32 GetBestAvailableLOD() const override;
	virtual void SetForceStreamedLOD(int32 LODIndex) override;
	virtual void SetForceRenderedLOD(int32 LODIndex) override;
	virtual int32 GetNumSyncLODs() const override;
	virtual int32 GetForceStreamedLOD() const override;
	virtual int32 GetForceRenderedLOD() const override;
	//~ End ILODSyncInterface

	int32 GetNumLODs() const;
	int32 GetForcedLOD() const;

	UFUNCTION(BlueprintCallable, Category = "TressFXMesh")
	void SetTressFXMeshAsset(UTressFXMeshAsset* Asset);

	/** TressFX Mesh Asset . */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category = "TressFXMesh")
		UTressFXMeshAsset* TressFXMeshAsset;

	UFUNCTION(BlueprintCallable, Category = "TressFXMesh")
	void SetEnableSimulation(bool EnableSimulation);

private:

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
		UTressFXMeshAsset* TressFXMeshAssetBeingLoaded;
#endif


	void InitResources(bool bIsBindingReloading = false);
	void ReleaseResources();

	friend class FTressFXSDFSceneProxy;

private:
	void* InitializedResources = nullptr;
	class USkeletalMeshComponent* RegisteredSkeletalMeshComponent = nullptr;
	bool bIsTressFXAssetCallbackRegistered = false;

	EWorldType::Type GetWorldType() const;

	FTressFXMeshGroupInstance* TressFXMeshGroupInstance = nullptr;

	friend class FTressFXSDFComponentRecreateRenderStateContext;
};

#if WITH_EDITORONLY_DATA
/** Used to recreate render context for all TressFXSDFComponents that use a given TressFXMeshAsset */
class TRESSFXCORE_API FTressFXSDFComponentRecreateRenderStateContext
{
public:
	FTressFXSDFComponentRecreateRenderStateContext(UTressFXMeshAsset* TressFXMeshAsset);
	~FTressFXSDFComponentRecreateRenderStateContext();

private:
	TArray<UTressFXSDFComponent*> TressFXSDFComponents;
};
#endif