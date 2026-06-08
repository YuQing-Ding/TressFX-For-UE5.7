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
#include "TressFXAsset.h"
#include "TressFXBindingAsset.h"
#include "RHIDefinitions.h"
#include "LODSyncInterface.h"
#include "TressFXInstance.h"

#include "TressFXComponent.generated.h"


UCLASS(HideCategories = (Object, Physics, Activation, Mobility, "Components|Activation"), editinlinenew, meta = (BlueprintSpawnableComponent), ClassGroup = Rendering)
class TRESSFXCORE_API UTressFXComponent : public UMeshComponent, public ILODSyncInterface
{
	GENERATED_UCLASS_BODY()

public:

	/** TressFX asset . */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category = "TressFX")
	UTressFXAsset* TressFXAsset = nullptr;

	UPROPERTY(EditAnywhere, Category = "TressFX", meta = (ToolTip = "SDF Component Local Id"))
	bool EnableSimulation = true;

	UPROPERTY(EditAnywhere, Category = "TressFX", meta = (ToolTip = "SDF Component Local Id"))
	uint32 LocalSDFIdRef = 0xFFFFFFFF;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, interp, Category = "TressFX")
	UTressFXBindingAsset* TressFXBindingAsset = nullptr;

	UPROPERTY()
	UMaterialInterface* Strands_DebugMaterial;
	UPROPERTY()
	UMaterialInterface* Strands_DefaultMaterial;
	UPROPERTY()
	UMaterialInterface* Cards_DefaultMaterial;
	UPROPERTY()
	UMaterialInterface* Meshes_DefaultMaterial;


	UFUNCTION(BlueprintCallable, Category = "TressFX")
	void SetTressFXAsset(UTressFXAsset* Asset);

	UFUNCTION(BlueprintCallable, Category = "TressFX")
	void SetEnableSimulation(bool InEnableSimulation) { EnableSimulation = InEnableSimulation; }

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	FTressFXGroupInstance* CreateTressFXGroupInstance();
	void DestroyTressFXGroupInstance(FTressFXGroupInstance* Instance);

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnAttachmentChanged() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void SendRenderTransform_Concurrent() override;
	//~ End UActorComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual void PostLoad() override;
	//~ End UMeshComponent Interface.

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
#if WITH_EDITOR
	//virtual void CheckForErrors() override;
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//virtual bool CanEditChange(const FProperty* InProperty) const override;
	//void ValidateMaterials(bool bMapCheck) const;
	//void Invalidate();
	//void InvalidateAndRecreate();
#endif

	//~ Begin UPrimitiveComponent Interface
	ETressFXGeometryType GetMaterialGeometryType(int32 ElementIndex) const;
	UMaterialInterface* GetMaterial(int32 ElementIndex, ETressFXGeometryType GeometryType, bool bUseDefaultIfIncompatible) const;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual int32 GetMaterialIndex(FName MaterialSlotName) const override;
	virtual TArray<FName> GetMaterialSlotNames() const override;
	virtual bool IsMaterialSlotNameValid(FName MaterialSlotName) const override;
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual int32 GetNumMaterials() const override;
	//~ End UPrimitiveComponent Interface

	// For demo only
	UFUNCTION(BlueprintCallable, Category = "TressFX")
	void SetWindDirection(const FVector3f& WindDir);
	UFUNCTION(BlueprintCallable, Category = "TressFX")
	void SetWindForce(const float WindForce);
	UFUNCTION(BlueprintCallable, Category = "TressFX")
	void SetGSS(const float GSS);
	UFUNCTION(BlueprintCallable, Category = "TressFX")
	void SetGSR(const float GSR);
	// End for demo only

private:
	FTressFXGroupInstance* TressFXGroupInstance = nullptr;
	FTressFXGroupsSimulation* TressFXSimulationSettings = nullptr;
	FAnimationTressFXSimulationSettings* TressFXAnimSimulationSettings = nullptr;

	UAnimationAsset* LastAnimAsset = nullptr;
	UAnimationAsset* CurrentAnimAsset = nullptr;

	float LODForcedIndex = -1.f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
		UTressFXAsset* TressFXAssetBeingLoaded;

	//UPROPERTY(Transient)
		//UTressFXBindingAsset* BindingAssetBeingLoaded;
#endif


	void InitResources(bool bIsBindingReloading = false);
	void ReleaseResources();


	friend class FTressFXSceneProxy;

private:
	void* InitializedResources;
	class USkeletalMeshComponent* RegisteredSkeletalMeshComponent;
	bool bIsTressFXAssetCallbackRegistered;
	bool bIsTressFXBindingAssetCallbackRegistered;

	EWorldType::Type GetWorldType() const;

	friend class FTressFXComponentRecreateRenderStateContext;
};

#if WITH_EDITORONLY_DATA
/** Used to recreate render context for all TressFXComponents that use a given TressFXAsset */
class TRESSFXCORE_API FTressFXComponentRecreateRenderStateContext
{
public:
	FTressFXComponentRecreateRenderStateContext(UTressFXAsset* TressFXAsset);
	~FTressFXComponentRecreateRenderStateContext();

private:
	TArray<UTressFXComponent*> TressFXComponents;
};
#endif