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
#include "TressFXSDFComponent.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveSceneInfo.h"
#include "UObject/UObjectIterator.h"
#include "GlobalShader.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Engine/RendererSettings.h"
#include "Animation/AnimationSettings.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "UObject/ConstructorHelpers.h"
#include "RenderTargetPool.h"
#include "TressFXManager.h"


class FTressFXSDFSceneProxy : public FPrimitiveSceneProxy
{
public:

	FTressFXSDFSceneProxy(class UPrimitiveComponent* InComponent, FName ResourceName = NAME_None);

	virtual ~FTressFXSDFSceneProxy();

	virtual uint32 GetMemoryFootprint(void) const override;

	virtual SIZE_T GetTypeHash() const override;

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
	virtual void DestroyRenderThreadResources() override;
private:
	UTressFXSDFComponent* TressFXSDFComponent;
	FTressFXMeshGroupInstance* MeshGroupInstance;
};

FTressFXSDFSceneProxy::FTressFXSDFSceneProxy(UPrimitiveComponent* InComponent, FName ResourceName) :
	FPrimitiveSceneProxy(InComponent, ResourceName)
{
	TressFXSDFComponent = Cast<UTressFXSDFComponent>(InComponent);
	MeshGroupInstance = TressFXSDFComponent->CreateTressFXMeshGroupInstance();
	if (MeshGroupInstance && MeshGroupInstance->IsValid())
		MeshGroupInstance->PrimitiveSceneProxy = this;
}

FTressFXSDFSceneProxy::~FTressFXSDFSceneProxy()
{
	if (MeshGroupInstance)
		TressFXSDFComponent->DestroyTressFXMeshGroupInstance(MeshGroupInstance);
}

void FTressFXSDFSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
{
	if (MeshGroupInstance)
	{
		if (MeshGroupInstance->IsValid())
		{
			RegisterTressFXMeshGroupInstance_RenderThread(MeshGroupInstance);
		}
	}
}

void FTressFXSDFSceneProxy::DestroyRenderThreadResources()
{
	if (MeshGroupInstance && MeshGroupInstance->IsValid())
	{
		UnregisterTressFXMeshGroupInstance_RenderThread(MeshGroupInstance);
	}
}

SIZE_T FTressFXSDFSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

uint32 FTressFXSDFSceneProxy::GetMemoryFootprint(void) const
{
	return 0; //JAKETODO
}

UTressFXSDFComponent::UTressFXSDFComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;
	bSelectable = true;
	Mobility = EComponentMobility::Movable;
	InitializedResources = nullptr;
}

FPrimitiveSceneProxy* UTressFXSDFComponent::CreateSceneProxy()
{
	return new FTressFXSDFSceneProxy(this, TressFXMeshAsset ? FName(*TressFXMeshAsset->GetName()) : FName(*this->GetName()));
}

void UTressFXSDFComponent::SetEnableSimulation(bool EnableSimulation)
{
	EnableSDF = EnableSimulation;
}

void UTressFXSDFComponent::SetTressFXMeshAsset(UTressFXMeshAsset* Asset)
{
	ReleaseResources();
	if (Asset && Asset->IsValid())
	{
		TressFXMeshAsset = Asset;

#if WITH_EDITORONLY_DATA

		TressFXMeshAssetBeingLoaded = nullptr;

#endif
	}
#if WITH_EDITORONLY_DATA
	else if (Asset)
	{
		// The asset is still being loaded. This will allow the assets to be re-set once the groom is finished loading
		TressFXMeshAssetBeingLoaded = Asset;
		//BindingAssetBeingLoaded = InBinding;
	}
#endif
	else
		TressFXMeshAsset = nullptr;

	if (!TressFXMeshAsset)
	{
		return;
	}

	InitResources();
}

void UTressFXSDFComponent::PostLoad()
{
	Super::PostLoad();

	SetTressFXMeshAsset(TressFXMeshAsset);

}

template<typename T>
inline void InternalReleaseResource_SDFComp(T*& Resource)
{
	if (Resource)
	{
		T* InResource = Resource;
		ENQUEUE_RENDER_COMMAND(ReleaseTressFXResourceCommand)(
			[InResource](FRHICommandList& RHICmdList)
			{
				InResource->ReleaseResource();
				delete InResource;
			});
		Resource = nullptr;
	}
}

FTressFXMeshGroupInstance* UTressFXSDFComponent::CreateTressFXMeshGroupInstance()
{
	if (!TressFXMeshAsset)
	{
		return nullptr;
	}

	const EWorldType::Type WorldType = GetWorldType();
	FString WorldName = GetWorld() ? GetWorld()->GetName() : TEXT("None");

	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetAttachParent());;// ValidateBindingAsset(TressFXAsset, BindingAsset, GetAttachParent() ? Cast<USkeletalMeshComponent>(GetAttachParent()) : nullptr, bIsBindingReloading, bValidationEnable, this);
	if (SkeletalMeshComponent && SkeletalMeshComponent->GetSkinnedAsset() == nullptr)
	{
		SkeletalMeshComponent = nullptr;
	}

	{
		FTressFXMeshGroupData& GroupData = TressFXMeshAsset->MeshGroupData;

		TressFXMeshGroupInstance = new FTressFXMeshGroupInstance();

		TressFXMeshGroupInstance->TFXMeshData = &GroupData.TFXMeshData;
		TressFXMeshGroupInstance->MeshRestResource = GroupData.MeshRestResource;
		TressFXMeshGroupInstance->WorldType = WorldType;
		TressFXMeshGroupInstance->LocalSDFId = LocalSDFId;
		TressFXMeshGroupInstance->WorldName = WorldName;
		TressFXMeshGroupInstance->Debug.ComponentId = GetPrimitiveSceneId().PrimIDValue;;
		TressFXMeshGroupInstance->Debug.bEnableVisualizeMesh = TressFXMeshAsset->EnableVisualizeMesh;
		TressFXMeshGroupInstance->Debug.bEnableVisualizeMeshAABB = TressFXMeshAsset->EnableVisualizeMeshAABB;
		TressFXMeshGroupInstance->CollisionMeshBoxMargin = TressFXMeshAsset->CollisionMeshBoxMargin;
		TressFXMeshGroupInstance->Debug.bEnableVisualizeSDF = TressFXMeshAsset->EnableVisualizeSDF;
		TressFXMeshGroupInstance->ParentSDFComp = this;
		TressFXMeshGroupInstance->Debug.SkeletalComponent = RegisteredSkeletalMeshComponent;
		TressFXMeshGroupInstance->NumSDFCells = TressFXMeshAsset->NumSDFCells;
		TressFXMeshGroupInstance->NumGridOffset = TressFXMeshAsset->NumGridOffset;
		TressFXMeshGroupInstance->SDFCollisionMargin = TressFXMeshAsset->SDFCollisionMargin;
		if (!SkeletalMeshComponent)
			TressFXMeshGroupInstance->Debug.bEnableVisualizeMesh = true;

		TressFXMeshGroupInstance->MeshDeformedResource = new FTressFXMeshDeformedResource(GroupData.TFXMeshData, TressFXMeshAsset->NumSDFCells);// GroupData.TFXMeshData.BoundingBox.GetCenter());
		BeginInitResource(TressFXMeshGroupInstance->MeshDeformedResource);

	}

	return TressFXMeshGroupInstance;
}

void UTressFXSDFComponent::InitResources(bool bIsBindingReloading)
{
	ReleaseResources();

	if (!TressFXMeshAsset)
	{
		return;
	}

	InitializedResources = TressFXMeshAsset;

	const EWorldType::Type WorldType = GetWorldType();
	FString WorldName = GetWorld() ? GetWorld()->GetName() : TEXT("None");

	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetAttachParent());;// ValidateBindingAsset(TressFXAsset, BindingAsset, GetAttachParent() ? Cast<USkeletalMeshComponent>(GetAttachParent()) : nullptr, bIsBindingReloading, bValidationEnable, this);
	if (SkeletalMeshComponent && SkeletalMeshComponent->GetSkinnedAsset() == nullptr)
	{
		SkeletalMeshComponent = nullptr;
	}

	if (SkeletalMeshComponent)
	{
		RegisteredSkeletalMeshComponent = SkeletalMeshComponent;
		AddTickPrerequisiteComponent(SkeletalMeshComponent);
	}

}

void UTressFXSDFComponent::DestroyTressFXMeshGroupInstance(FTressFXMeshGroupInstance* InMeshInstance)
{
	FTressFXMeshGroupInstance* LocalInstance = InMeshInstance;

	if (InMeshInstance == TressFXMeshGroupInstance)
		TressFXMeshGroupInstance = nullptr;

	if (LocalInstance)
	{
		ENQUEUE_RENDER_COMMAND(FTressFXReleaseMeshInstance)(
			[LocalInstance](FRHICommandListImmediate& RHICmdList)
			{
				if (LocalInstance->MTCollisionMeshResource)
				{
					InternalReleaseResource_SDFComp(LocalInstance->MTCollisionMeshResource);
				}

				if (LocalInstance->MeshDeformedResource)
				{
					InternalReleaseResource_SDFComp(LocalInstance->MeshDeformedResource);

				}

				delete LocalInstance;
			});
	}
}

void UTressFXSDFComponent::ReleaseResources()
{
	InitializedResources = nullptr;

	TressFXMeshGroupInstance = nullptr;

	// Insure the ticking of the TressFX component always happens after the skeletalMeshComponent.
	if (RegisteredSkeletalMeshComponent)
	{
		RemoveTickPrerequisiteComponent(RegisteredSkeletalMeshComponent);
	}
	RegisteredSkeletalMeshComponent = nullptr;

	MarkRenderStateDirty();
}

EWorldType::Type UTressFXSDFComponent::GetWorldType() const
{
	EWorldType::Type WorldType = GetWorld() ? EWorldType::Type(GetWorld()->WorldType) : EWorldType::None;
	return WorldType == EWorldType::Inactive ? EWorldType::Editor : WorldType;
}

void UTressFXSDFComponent::OnRegister()
{
	Super::OnRegister();

	// Insure the parent skeletal mesh is the same than the registered skeletal mesh, and if not reinitialized resources
	// This can happens when the OnAttachment callback is not called, but the skeletal mesh change (e.g., the skeletal mesh get recompiled within a blueprint)
	USkeletalMeshComponent* SkeletalMeshComponent = GetAttachParent() ? Cast<USkeletalMeshComponent>(GetAttachParent()) : nullptr;

	const bool bNeedInitialization = !InitializedResources || InitializedResources != TressFXMeshAsset || SkeletalMeshComponent != RegisteredSkeletalMeshComponent;
	if (bNeedInitialization)
	{
		InitResources();
	}
	
}

void UTressFXSDFComponent::OnUnregister()
{
	Super::OnUnregister();
	
	ReleaseResources();
}

void UTressFXSDFComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	ReleaseResources();

#if WITH_EDITOR

#endif

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UTressFXSDFComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();
	if (TressFXMeshAsset && !IsBeingDestroyed() && HasBeenCreated())
	{
		USkeletalMeshComponent* NewSkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetAttachParent());
		const bool bHasAttachmentChanged = RegisteredSkeletalMeshComponent != NewSkeletalMeshComponent;
		if (bHasAttachmentChanged)
		{
			InitResources();
		}
	}
}

void UTressFXSDFComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void UTressFXSDFComponent::SendRenderTransform_Concurrent()
{
	if (RegisteredSkeletalMeshComponent)
	{
		if (ShouldComponentAddToScene() && ShouldRender())
		{
			GetWorld()->Scene->UpdatePrimitiveTransform(this);
		}
	}

	Super::SendRenderTransform_Concurrent();
}

FBoxSphereBounds UTressFXSDFComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	if (TressFXMeshAsset)
	{
		if (RegisteredSkeletalMeshComponent)
		{
			const FBox WorldSkeletalBound = RegisteredSkeletalMeshComponent->CalcBounds(InLocalToWorld).GetBox();
			return FBoxSphereBounds(WorldSkeletalBound);
		}
		else
		{
			FBox LocalBounds(EForceInit::ForceInitToZero);
			
			LocalBounds += TressFXMeshAsset->MeshGroupData.TFXMeshData.BoundingBox;
			
			return FBoxSphereBounds(LocalBounds.TransformBy(InLocalToWorld));
		}
	}
	else
	{
		return FBoxSphereBounds(EForceInit::ForceInitToZero);
	}
}

int32 UTressFXSDFComponent::GetDesiredSyncLOD() const
{
	return 0;
}

int32 UTressFXSDFComponent::GetNumSyncLODs() const
{
	return 1;
}

int32 UTressFXSDFComponent::GetNumLODs() const
{
	return 1;// return TressFXAsset ? TressFXAsset->GetLODCount() : 0;
}

int32 UTressFXSDFComponent::GetForcedLOD() const
{
	return 1;// int32(LODForcedIndex);
}

int32 UTressFXSDFComponent::GetBestAvailableLOD() const
{
	// For now we assume all LODs are available. This could be made more accurate in future.
	return 0;
}

void UTressFXSDFComponent::SetForceStreamedLOD(int32 LODIndex)
{
	// Force streaming is not supported yet
}

void UTressFXSDFComponent::SetForceRenderedLOD(int32 InLODIndex)
{
	//SetForcedLOD(InLODIndex);
}

int32 UTressFXSDFComponent::GetForceStreamedLOD() const
{
	// Force streaming is not supported yet
	return INDEX_NONE;
}

int32 UTressFXSDFComponent::GetForceRenderedLOD() const
{
	return GetForcedLOD();
}


#if WITH_EDITORONLY_DATA
FTressFXSDFComponentRecreateRenderStateContext::FTressFXSDFComponentRecreateRenderStateContext(UTressFXMeshAsset* TressFXMeshAsset)
{
	if (!TressFXMeshAsset)
	{
		return;
	}

	for (TObjectIterator<UTressFXSDFComponent> TressFXSDFComponentIt; TressFXSDFComponentIt; ++TressFXSDFComponentIt)
	{
		if (TressFXSDFComponentIt->TressFXMeshAsset == TressFXMeshAsset ||
			TressFXSDFComponentIt->TressFXMeshAssetBeingLoaded == TressFXMeshAsset) // A TressFXAsset was set on the component while it was still loading
		{
			if (TressFXSDFComponentIt->IsRenderStateCreated())
			{
				TressFXSDFComponentIt->DestroyRenderState_Concurrent();
				TressFXSDFComponents.Add(*TressFXSDFComponentIt);
			}
		}
	}

	// Flush the rendering commands generated by the detachments.
	FlushRenderingCommands();
}

FTressFXSDFComponentRecreateRenderStateContext::~FTressFXSDFComponentRecreateRenderStateContext()
{
	const int32 ComponentCount = TressFXSDFComponents.Num();
	for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
	{
		UTressFXSDFComponent* TressFXSDFComponent = TressFXSDFComponents[ComponentIndex];

		if (TressFXSDFComponent->IsRegistered() && !TressFXSDFComponent->IsRenderStateCreated())
		{
			if (TressFXSDFComponent->TressFXMeshAssetBeingLoaded && TressFXSDFComponent->TressFXMeshAssetBeingLoaded->IsValid())
			{
				// Re-set the assets on the component now that they are loaded
				TressFXSDFComponent->SetTressFXMeshAsset(TressFXSDFComponent->TressFXMeshAssetBeingLoaded);// , TressFXComponent->BindingAssetBeingLoaded);
			}
			else
			{
				//TressFXSDFComponent->InitResources();
			}
			TressFXSDFComponent->CreateRenderState_Concurrent(nullptr);
		}
	}
}
#endif
