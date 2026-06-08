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
#include "TressFXComponent.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "PrimitiveDrawInterface.h"
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
#include "TressFXVertexFactory.h"
#include "Runtime/Engine/Classes/Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "PrimitiveUniformShaderParametersBuilder.h"

static bool ShouldRunTressFXSimulationInWorld(EWorldType::Type WorldType)
{
	return WorldType != EWorldType::None && WorldType != EWorldType::Editor;
}

#if WITH_EDITOR
static int32 GTressFXPreviewGuideLineFallback = 0;
static FAutoConsoleVariableRef CVarTressFXPreviewGuideLineFallback(
	TEXT("r.TressFX.PreviewGuideLineFallback"),
	GTressFXPreviewGuideLineFallback,
	TEXT("Draw TressFX guide strands as CPU debug lines in asset preview viewports."));

static int32 GTressFXPreviewGuideLineMaxStrands = 512;
static FAutoConsoleVariableRef CVarTressFXPreviewGuideLineMaxStrands(
	TEXT("r.TressFX.PreviewGuideLineMaxStrands"),
	GTressFXPreviewGuideLineMaxStrands,
	TEXT("Maximum guide strands drawn by the CPU fallback preview."));

static int32 GTressFXDebugLogs = 0;
static FAutoConsoleVariableRef CVarTressFXDebugLogs(
	TEXT("r.TressFX.DebugLogs"),
	GTressFXDebugLogs,
	TEXT("Enable verbose TressFX diagnostic logging."));
#endif

class FTressFXSceneProxy final : public FPrimitiveSceneProxy
{
private:
	struct FHairGroup
	{
#if RHI_RAYTRACING
		FRayTracingGeometry* RayTracingGeometry_Strands = nullptr;
#endif
		FTressFXGroupPublicData* PublicData = nullptr;
		bool bIsVsibible = true;
	};


public:

	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FTressFXSceneProxy(UTressFXComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, TressFXVertexFactory(Component->TressFXGroupInstance, GetScene().GetFeatureLevel(), "FTressFXSceneProxy")
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	{
		bVerifyUsedMaterials = false;

		TFXComponent = Cast<UTressFXComponent>(Component);

		TressFXGroupInstance = Component->CreateTressFXGroupInstance();
		if (TressFXGroupInstance)
			TressFXGroupInstance->PrimitiveSceneProxy = this;

		//ComponentId = Component->ComponentId.PrimIDValue;

		//const int32 GroupCount = 1;// Component->TressFXAsset->GetNumTressFXGroups();
		//check(Component->TressFXAsset->TressFXGroupsData.Num() == Component->TressFXGroupInstances.Num());
		//for (int32 GroupIt = 0; GroupIt < GroupCount; GroupIt++)
		{
			const bool bIsVisible = true;//Component->TressFXAsset->HairGroupsInfo[GroupIt].bIsVisible;
			{
				FHairGroup& OutGroupData = HairGroups.AddDefaulted_GetRef();
				OutGroupData.bIsVsibible = bIsVisible;

			}
			check(TressFXGroupInstance->TressFXGroupPublicData);
			TressFXGroupInstance->ProxyBounds = &GetBounds();
			TressFXGroupInstance->ProxyLocalBounds = &GetLocalBounds();

		}

		FTressFXVertexFactory::FDataType TressFXVFData;
		TressFXVFData.Instance = TressFXGroupInstance;

		const EShaderPlatform Platform = GetScene().GetShaderPlatform();

		FTressFXVertexFactory* LocalTressFXVertexFactory = &TressFXVertexFactory;
		ENQUEUE_RENDER_COMMAND(InitTressFXVertexFactory)(
			[Platform, LocalTressFXVertexFactory, TressFXVFData](FRHICommandListImmediate& RHICmdList)
			{
				if (IsTressFXEnabled(ETressFXShaderType::Strands, Platform))
				{
					LocalTressFXVertexFactory->SetData(TressFXVFData,RHICmdList);
					LocalTressFXVertexFactory->InitResource(RHICmdList);
				}
			});
	}

	virtual ~FTressFXSceneProxy()
	{

		TFXComponent->DestroyTressFXGroupInstance(TressFXGroupInstance);
	}

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList)
	{
		FPrimitiveSceneProxy::CreateRenderThreadResources(RHICmdList);

		if (TressFXGroupInstance)
		{
			RegisterTressFXGroupInstance_RenderThread(TressFXGroupInstance);
		}
	}

	virtual void DestroyRenderThreadResources() override
	{
		FPrimitiveSceneProxy::DestroyRenderThreadResources();
		TressFXVertexFactory.ReleaseResource();

		UnregisterTressFXGroupInstance_RenderThread(TressFXGroupInstance);
	}

	virtual void OnTransformChanged(FRHICommandListBase& RHICmdList) override
	{
		if (TressFXGroupInstance)
		{
			TressFXGroupInstance->LocalToWorld = FTransform(GetLocalToWorld());
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		//const FTressFXGroupInstance* Instance = TressFXVertexFactory.GetData().Instance;

		bool bUseCardsOrMesh = true;
		const uint32 GroupCount = 1;// Instances.Num();
		
		FPrimitiveViewRelevance Result;

		// Special pass for tressfx geometry (not part of the base pass, and shadowing is handlded in a custom fashion). When cards rendering is enabled we reusethe base pass
		Result.bDrawRelevance = true;
		Result.bRenderInMainPass = bUseCardsOrMesh;
		Result.bShadowRelevance = true;
		Result.bDynamicRelevance = true;
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		Result.bVelocityRelevance = false;
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();

		// Selection only
#if WITH_EDITOR
		{
			Result.bEditorStaticSelectionRelevance = true;
		}
#endif
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}


	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		const EShaderPlatform Platform = ViewFamily.GetShaderPlatform();
		const bool bStrandsEnabled = IsTressFXEnabled(ETressFXShaderType::Strands, Platform);

		FTressFXGroupInstance* Instance = TressFXGroupInstance;
		if (!Instance)
		{
			return;
		}

		const uint32 GroupCount = 1;// Instances.Num();

		FMaterialRenderProxy* Strands_MaterialProxy = nullptr;


		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FSceneView* View = Views[ViewIndex];
			if (View->bIsReflectionCapture || View->bIsPlanarReflection)
			{
				continue;
			}

			if (IsShown(View) && (VisibilityMap & (1 << ViewIndex)))
			{
#if WITH_EDITOR
				if (GTressFXPreviewGuideLineFallback && Instance->WorldType == EWorldType::EditorPreview)
				{
					DrawGuideLineFallback(ViewIndex, Collector, Instance);
				}
#endif
				if (!bStrandsEnabled)
				{
					continue;
				}

				for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
				{
					FMeshBatch* MeshBatch = CreateMeshBatch(View, ViewFamily, Collector, HairGroups[GroupIt], Instance, GroupIt, Strands_MaterialProxy);
					if (MeshBatch == nullptr)
					{
						continue;
					}
					Collector.AddMesh(ViewIndex, *MeshBatch);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					// Render bounds
					RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#endif
				}
			}
		}
	}

#if WITH_EDITOR
	void DrawGuideLineFallback(int32 ViewIndex, FMeshElementCollector& Collector, const FTressFXGroupInstance* Instance) const
	{
		const FTressFXDatas* TFXData = Instance ? Instance->TFXData : nullptr;
		if (!TFXData)
		{
			return;
		}

		const TArray<FVector4f>& Points = TFXData->StrandsPoints.PointsPosition;
		const int32 NumVerticesPerStrand = static_cast<int32>(TFXData->NumVerticesPerStrand);
		if (NumVerticesPerStrand < 2 || Points.Num() < NumVerticesPerStrand)
		{
			return;
		}

		FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
		if (!PDI)
		{
			return;
		}

		const int32 NumStrands = Points.Num() / NumVerticesPerStrand;
		const int32 MaxPreviewStrands = FMath::Max(1, GTressFXPreviewGuideLineMaxStrands);
		const int32 StrandStride = FMath::Max(1, FMath::DivideAndRoundUp(NumStrands, MaxPreviewStrands));
		const FMatrix& HairLocalToWorld = GetLocalToWorld();

		if (GTressFXDebugLogs && !bLoggedGuideLineFallback)
		{
			UE_LOG(LogTemp, Warning, TEXT("TressFX preview line fallback: points=%d strands=%d vertsPerStrand=%d stride=%d bounds=%s localBounds=%s"),
				Points.Num(),
				NumStrands,
				NumVerticesPerStrand,
				StrandStride,
				*GetBounds().GetBox().ToString(),
				*GetLocalBounds().GetBox().ToString());
			bLoggedGuideLineFallback = true;
		}

		for (int32 StrandIndex = 0; StrandIndex < NumStrands; StrandIndex += StrandStride)
		{
			const int32 StrandStartIndex = StrandIndex * NumVerticesPerStrand;
			for (int32 VertexIndex = 0; VertexIndex + 1 < NumVerticesPerStrand; ++VertexIndex)
			{
				const FVector4f& P0 = Points[StrandStartIndex + VertexIndex];
				const FVector4f& P1 = Points[StrandStartIndex + VertexIndex + 1];
				const FVector Start = HairLocalToWorld.TransformPosition(FVector(P0.X, P0.Y, P0.Z));
				const FVector End = HairLocalToWorld.TransformPosition(FVector(P1.X, P1.Y, P1.Z));
				const float Alpha = static_cast<float>(VertexIndex) / static_cast<float>(NumVerticesPerStrand - 1);
				const FLinearColor SegmentColor = FLinearColor(
					0.05f,
					FMath::Lerp(1.0f, 0.55f, Alpha),
					FMath::Lerp(0.15f, 1.0f, Alpha),
					1.0f);

				PDI->DrawLine(Start, End, SegmentColor, SDPG_Foreground, 0.35f, 0.0f, true);
			}
		}
	}
#endif

	FMeshBatch* CreateMeshBatch(
		const FSceneView* View,
		const FSceneViewFamily& ViewFamily,
		FMeshElementCollector& Collector,
		const FHairGroup& GroupData,
		const FTressFXGroupInstance* Instance,
		uint32 GroupIndex,
		FMaterialRenderProxy* Strands_MaterialProxy) const
	{
		const ETressFXGeometryType GeometryType = Instance->GeometryType;
		if (GeometryType == ETressFXGeometryType::NoneGeometry_)
		{
			return nullptr;
		}

		const int32 IntLODIndex = 0;//Instance->TressFXGroupPublicData->GetIntLODIndex();
		const FVertexFactory* VertexFactory = nullptr;
		FIndexBuffer* IndexBuffer = nullptr;
		FMaterialRenderProxy* MaterialRenderProxy = nullptr;

		uint32 NumPrimitive = 0;
		uint32 HairVertexCount = 0;
		uint32 MaxVertexIndex = 0;
		bool bUseCulling = false;
		bool bWireframe = false;
		
		// if (GeometryType == EHairGeometryType::Strands)
		{
			VertexFactory = (FVertexFactory*)&TressFXVertexFactory;
			HairVertexCount = Instance->Strands.StrandsResource->GetVertexCount();
			MaxVertexIndex = HairVertexCount > 0 ? HairVertexCount * 6 - 1 : 0;
			NumPrimitive = HairVertexCount * 2;
			bUseCulling = false;
			MaterialRenderProxy = Strands_MaterialProxy == nullptr ? Instance->Strands.Material->GetRenderProxy() : Strands_MaterialProxy;
			bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
		}

		if (!MaterialRenderProxy)
			return nullptr;

		FMeshBatch& Mesh = Collector.AllocateMesh();

		const bool bUseCardsOrMeshes = true;
		Mesh.CastShadow = bUseCardsOrMeshes;
		Mesh.bUseForMaterial = true;
		Mesh.bUseForDepthPass = bUseCardsOrMeshes;
		Mesh.SegmentIndex = 0;

		FMeshBatchElement& BatchElement = Mesh.Elements[0];
		BatchElement.IndexBuffer = IndexBuffer;
		Mesh.bWireframe = bWireframe;
		Mesh.VertexFactory = VertexFactory;
		Mesh.MaterialRenderProxy = MaterialRenderProxy;
		bool bHasPrecomputedVolumetricLightmap;
		FMatrix PreviousLocalToWorld;
		int32 SingleCaptureIndex;
		bool bOutputVelocity = false;
		bool bDrawVelocity = false; // Velocity vector is done in a custom fashion
		GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
		bOutputVelocity = false;

		// Use default SceneProxy builder values
		FPrimitiveUniformShaderParametersBuilder Builder;
		BuildUniformShaderParameters(Builder);

		// Override transforms and the local bound.
		// The original local bound relative to the component local to world transform. If we override the local to world transform, 
		// we need to recompute the local bound relative to this new transform. It is important that the new local bound is correct 
		// as otherwise the GPUScene (which use bot the local to world transform and the local bound for culling purpose) will issue 
		// incorrect visibility test.
		FBoxSphereBounds NewLocalBound = GetLocalBounds();
		Builder
			.LocalToWorld(GetLocalToWorld())
			.PreviousLocalToWorld(PreviousLocalToWorld)
			.LocalBounds(NewLocalBound)
			.OutputVelocity(bOutputVelocity)
			.UseVolumetricLightmap(false);

		// Create primitive uniform buffer
		FRHICommandListBase& RHICmdList = Collector.GetRHICommandList();
		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
		DynamicPrimitiveUniformBuffer.UniformBuffer.BufferUsage = UniformBuffer_SingleFrame;
		DynamicPrimitiveUniformBuffer.UniformBuffer.SetContents(RHICmdList, Builder.Build());
		DynamicPrimitiveUniformBuffer.UniformBuffer.InitResource(RHICmdList);
		BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

		BatchElement.FirstIndex = 0;
		BatchElement.NumInstances = 1;
		BatchElement.PrimitiveIdMode = PrimID_DynamicPrimitiveShaderData; // Force using per primitive uniform buffer.
		if (bUseCulling)
		{
			BatchElement.NumPrimitives = 0;
			BatchElement.IndirectArgsBuffer = bUseCulling ? Instance->TressFXGroupPublicData->GetDrawIndirectBuffer().Buffer->GetRHI() : nullptr;
			BatchElement.IndirectArgsOffset = 0;
		}
		else
		{
			BatchElement.NumPrimitives = NumPrimitive;
			BatchElement.IndirectArgsBuffer = nullptr;
			BatchElement.IndirectArgsOffset = 0;
		}

		BatchElement.VertexFactoryUserData = const_cast<void*>(reinterpret_cast<const void*>(Instance->TressFXGroupPublicData));

		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = MaxVertexIndex;
		BatchElement.UserData = reinterpret_cast<void*>(uint64(ComponentId));
		Mesh.ReverseCulling = bUseCardsOrMeshes ? IsLocalToWorldDeterminantNegative() : false;
		Mesh.Type = PT_TriangleList;
		Mesh.DepthPriorityGroup = SDPG_World;
		Mesh.bCanApplyViewModeOverrides = false;

		return &Mesh;
	}

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

	FTressFXGroupInstance* TressFXGroupInstance = nullptr;

private:
	uint32 ComponentId = 0;

	FTressFXVertexFactory TressFXVertexFactory;
	FMaterialRelevance MaterialRelevance;

	class UTressFXComponent* TFXComponent;

	TArray<FHairGroup> HairGroups;

#if WITH_EDITOR
	mutable bool bLoggedGuideLineFallback = false;
#endif
};



UTressFXComponent::UTressFXComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;
	bSelectable = true;
	InitializedResources = nullptr;
	Mobility = EComponentMobility::Movable;
	bIsTressFXAssetCallbackRegistered = false;
	bIsTressFXBindingAssetCallbackRegistered = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Strands_DebugMaterialRef(TEXT("/TressFX/Materials/HairDebugMaterial.HairDebugMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Strands_DefaultMaterialRef(TEXT("/TressFX/Materials/HairDefaultMaterial.HairDefaultMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Cards_DefaultMaterialRef(TEXT("/TressFX/Materials/HairCardsDefaultMaterial.HairCardsDefaultMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Meshes_DefaultMaterialRef(TEXT("/TressFX/Materials/HairMeshesDefaultMaterial.HairMeshesDefaultMaterial"));

	Strands_DebugMaterial = Strands_DebugMaterialRef.Object;
	Strands_DefaultMaterial = Strands_DefaultMaterialRef.Object;
	Cards_DefaultMaterial = Cards_DefaultMaterialRef.Object;
	Meshes_DefaultMaterial = Meshes_DefaultMaterialRef.Object;

#if WITH_EDITORONLY_DATA
	TressFXAssetBeingLoaded = nullptr;
	//BindingAssetBeingLoaded = nullptr;
#endif
}

#if WITH_EDITOR

void UTressFXComponent::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	const FName PropertyName = PropertyAboutToChange ? PropertyAboutToChange->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTressFXComponent, TressFXAsset))
	{
		// Remove the callback on the TressFXAsset about to be replaced
		if (bIsTressFXAssetCallbackRegistered && TressFXAsset)
		{
			TressFXAsset->GetOnTressFXAssetChanged().RemoveAll(this);
			TressFXAsset->GetOnTressFXAssetResourcesChanged().RemoveAll(this);
		}
		bIsTressFXAssetCallbackRegistered = false;
	}
}

void UTressFXComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	const FName PropertyName = PropertyThatChanged ? PropertyThatChanged->GetFName() : NAME_None;

}

#endif

int32 UTressFXComponent::GetNumMaterials() const
{
	return 1;
}

ETressFXGeometryType UTressFXComponent::GetMaterialGeometryType(int32 ElementIndex) const
{
	if (!TressFXAsset)
	{
		// If we don't know, enforce strands, as it has the most requirement.
		return ETressFXGeometryType::Strands_;
	}

	const EShaderPlatform Platform = GetScene() ? GetScene()->GetShaderPlatform() : EShaderPlatform::SP_NumPlatforms;
	//for (uint32 GroupIt = 0, GroupCount = TressFXAsset->TressFXGroupsRendering.Num(); GroupIt < GroupCount; ++GroupIt)
	{
		// Material - Strands
		const FTressFXGroupData& InGroupData = TressFXAsset->TressFXGroupsData[0];
		if (IsTressFXEnabled(ETressFXShaderType::Strands, Platform))
		{
			const int32 SlotIndex = TressFXAsset->GetMaterialIndex(TressFXAsset->TressFXGroupsRendering.MaterialSlotName);
			if (SlotIndex == ElementIndex)
			{
				return ETressFXGeometryType::Strands_;
			}
		}

	}
	// If we don't know, enforce strands, as it has the most requirement.
	return ETressFXGeometryType::Strands_;
}

enum class ETressFXMaterialCompatibility : uint8
{
	Valid,
	Invalid_UsedWithTressFX,
	Invalid_ShadingModel,
	Invalid_BlendMode,
	Invalid_IsNull
};

static ETressFXMaterialCompatibility IsTressFXMaterialCompatible(UMaterialInterface* MaterialInterface, ERHIFeatureLevel::Type FeatureLevel, ETressFXGeometryType GeometryType)
{
	// Hair material and opaque material are enforced for strands material as the strands system is tailored for this type of shading 
	// (custom packing of material attributes). However this is not needed/required for cards/meshes, when the relaxation for these type 
	// of goemetry
	if (MaterialInterface)
	{
		if (GeometryType != Strands_)
		{
			return ETressFXMaterialCompatibility::Valid;
		}
		if (!MaterialInterface->GetShadingModels().HasShadingModel(MSM_Hair) && GeometryType == ETressFXGeometryType::Strands_)
		{
			return ETressFXMaterialCompatibility::Invalid_ShadingModel;
		}
		if (MaterialInterface->GetBlendMode() != BLEND_Opaque && MaterialInterface->GetBlendMode() != BLEND_Masked && GeometryType == ETressFXGeometryType::Strands_)
		{
			return ETressFXMaterialCompatibility::Invalid_BlendMode;
		}
	}
	else
	{
		return ETressFXMaterialCompatibility::Invalid_IsNull;
	}

	return ETressFXMaterialCompatibility::Valid;
}

UMaterialInterface* UTressFXComponent::GetMaterial(int32 ElementIndex, ETressFXGeometryType GeometryType, bool bUseDefaultIfIncompatible) const
{
	UMaterialInterface* OverrideMaterial = Super::GetMaterial(ElementIndex);

	bool bUseHairDefaultMaterial = false;

	if (!OverrideMaterial && TressFXAsset)// && ElementIndex < TressFXAsset->TressFXGroupsMaterials.Num())
	{
		if (UMaterialInterface* Material = TressFXAsset->TressFXGroupsMaterials.Material)
		{
			OverrideMaterial = Material;
		}
		else if (bUseDefaultIfIncompatible)
		{
			bUseHairDefaultMaterial = true;
		}
	}

	if (bUseDefaultIfIncompatible)
	{
		const ERHIFeatureLevel::Type FeatureLevel = GetScene() ? GetScene()->GetFeatureLevel() : ERHIFeatureLevel::Num;
		if (FeatureLevel != ERHIFeatureLevel::Num && IsTressFXMaterialCompatible(OverrideMaterial, FeatureLevel, GeometryType) != ETressFXMaterialCompatibility::Valid)
		{
			bUseHairDefaultMaterial = true;
		}
	}

	if (bUseHairDefaultMaterial)
	{
		if (GeometryType == ETressFXGeometryType::Strands_)
		{
			OverrideMaterial = Strands_DefaultMaterial;
		}
	}

	return OverrideMaterial;
}

UMaterialInterface* UTressFXComponent::GetMaterial(int32 ElementIndex) const
{
	const ETressFXGeometryType GeometryType = GetMaterialGeometryType(ElementIndex);
	return GetMaterial(ElementIndex, GeometryType, true);
}

int32 UTressFXComponent::GetMaterialIndex(FName MaterialSlotName) const
{
	if (TressFXAsset)
	{
		return TressFXAsset->GetMaterialIndex(MaterialSlotName);
	}

	return INDEX_NONE;
}

TArray<FName> UTressFXComponent::GetMaterialSlotNames() const
{
	TArray<FName> MaterialNames;
	if (TressFXAsset)
	{
		MaterialNames = TressFXAsset->GetMaterialSlotNames();
	}

	return MaterialNames;
}

bool UTressFXComponent::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	return GetMaterialIndex(MaterialSlotName) != INDEX_NONE;
}

void UTressFXComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	UMeshComponent::GetUsedMaterials(OutMaterials, bGetDebugMaterials);
	if (bGetDebugMaterials)
	{
		if (IsTressFXEnabled(ETressFXShaderType::Tool))
		{
			OutMaterials.Add(Strands_DebugMaterial);
		}
	}

	if (IsTressFXEnabled(ETressFXShaderType::Strands))
	{
		OutMaterials.Add(Strands_DefaultMaterial);
	}
}

template<typename T>
void InternalResourceRelease(T*& In)
{
	if (In)
	{
		In->ReleaseResource();
		delete In;
		In = nullptr;
	}
}

FTressFXGroupInstance* UTressFXComponent::CreateTressFXGroupInstance()
{
	if (!TressFXAsset || TressFXAsset->GetNumTressFXGroups() == 0)
	{
		return nullptr;
	}

	//UE_LOG(LogTemp, Warning, TEXT("Create TressFX Group Instance"));

	InitializedResources = TressFXAsset;

	TressFXSimulationSettings = nullptr;
	TressFXAnimSimulationSettings = nullptr;
	LastAnimAsset = nullptr;
	CurrentAnimAsset = nullptr;

	const EWorldType::Type WorldType = GetWorldType();
	FString WorldName = GetWorld() ? GetWorld()->GetName() : TEXT("None");

	const bool bIsStrandsEnabled = IsTressFXEnabled(ETressFXShaderType::Strands);

	// Insure that the binding asset is compatible, otherwise no binding will be used
	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetAttachParent());;// ValidateBindingAsset(TressFXAsset, BindingAsset, GetAttachParent() ? Cast<USkeletalMeshComponent>(GetAttachParent()) : nullptr, bIsBindingReloading, bValidationEnable, this);
	if (SkeletalMeshComponent && SkeletalMeshComponent->GetSkinnedAsset() == nullptr)
	{
		SkeletalMeshComponent = nullptr;
	}

	int32 GroupIt = 0;
	//for (int32 GroupIt = 0, GroupCount = TressFXAsset->TressFXGroupsData.Num(); GroupIt < GroupCount; ++GroupIt)
	{
		FTressFXGroupData& GroupData = TressFXAsset->TressFXGroupsData[GroupIt];

		TressFXGroupInstance = new FTressFXGroupInstance();
		TressFXGroupInstance->WorldType = WorldType;
		TressFXGroupInstance->LocalSDFIdRef = LocalSDFIdRef;
		TressFXGroupInstance->Debug.ComponentId = GetPrimitiveSceneId().PrimIDValue;
		TressFXGroupInstance->Debug.GroupIndex = GroupIt;
		TressFXGroupInstance->Debug.GroupCount = 1;
		TressFXGroupInstance->ParentComponent = GetAttachParent();
		TressFXGroupInstance->SkeletalComponent = SkeletalMeshComponent;
		TressFXGroupInstance->LocalToWorld = GetComponentTransform();
		TressFXGroupInstance->WorldName = WorldName;
		TressFXGroupInstance->Debug.bEnableVisualizeTangents = TressFXAsset->TressFXGroupsRendering.EnableVisualizeTangents;

		TressFXGroupInstance->GeometryType = ETressFXGeometryType::Strands_;
		TressFXGroupInstance->StrandsWidth = TressFXAsset->TressFXGroupsRendering.StrandsWidth;
		TressFXGroupInstance->TranslucencyDensity = TressFXAsset->TressFXGroupsRendering.TranslucencyDensity;
		TressFXGroupInstance->NumFollowStrands = TressFXAsset->TressFXGroupsRendering.NumFollowStrands;
		TressFXGroupInstance->TipSeparationFactor = TressFXAsset->TressFXGroupsRendering.TipSeparationFactorOfFollowStrands;
		TressFXGroupInstance->MaxRadiusAroundGuide = TressFXAsset->TressFXGroupsRendering.MaxRadiusAroundGuide;

		// Simulation

		if (TressFXGroupInstance->SkeletalComponent && TressFXAsset->AnimationSimulationSettingsMap.Num())
		{
			UAnimSingleNodeInstance* AnimInst = TressFXGroupInstance->SkeletalComponent->GetSingleNodeInstance();
			if (AnimInst)
				CurrentAnimAsset = AnimInst->GetCurrentAsset();
			//CurrentAnimAsset = TressFXGroupInstance->SkeletalComponent->AnimationData.AnimToPlay.Get();
			FString CurSimulationSettingsName;
			for (int32 i = 0; i < TressFXAsset->AnimationSimulationSettingsMap.Num(); ++i)
			{
				if (CurrentAnimAsset == TressFXAsset->AnimationSimulationSettingsMap[i].Animation.Get())
				{
					CurSimulationSettingsName = TressFXAsset->AnimationSimulationSettingsMap[i].TressFXSimulationSettingsName;
					TressFXAnimSimulationSettings = &TressFXAsset->AnimationSimulationSettingsMap[i];
					break;
				}
			}

			for (int32 i = 0; i < TressFXAsset->TressFXSimulationSettingsArray.Num(); ++i)
			{
				if (CurSimulationSettingsName == TressFXAsset->TressFXSimulationSettingsArray[i].Name)
				{
					TressFXSimulationSettings = &TressFXAsset->TressFXSimulationSettingsArray[i];
					break;
				}
			}
		}

		if (!TressFXSimulationSettings)
			TressFXSimulationSettings = &TressFXAsset->TressFXSimulationSettings;

		TressFXGroupInstance->Guides.GravityMagnitude = TressFXSimulationSettings->GravityMagnitude;
		TressFXGroupInstance->Guides.Damping = TressFXSimulationSettings->Damping;
		if (TressFXSimulationSettings->EnableGlobalShapeConstraint)
		{
			TressFXGroupInstance->Guides.GlobalShapeStiffness = TressFXSimulationSettings->GlobalShapeStiffness;
			TressFXGroupInstance->Guides.GlobalShapeEffectiveRange = TressFXSimulationSettings->GlobalShapeEffectiveRange;
		}
		else
		{
			TressFXGroupInstance->Guides.GlobalShapeStiffness = 0.f;
			TressFXGroupInstance->Guides.GlobalShapeEffectiveRange = 0.f;
		}

		TressFXGroupInstance->Guides.NumVerticesFromRootNoSimulation = TressFXSimulationSettings->NumVerticesFromRootNoSimulation;

		TressFXGroupInstance->Guides.VSPStiffness.AddZeroed(GroupData.TFXData.NumVerticesPerStrand);
		FRuntimeFloatCurve& VSPStiffnessCurve = TressFXSimulationSettings->VSPStiffness;
		for (uint32 i = 0; i < GroupData.TFXData.NumVerticesPerStrand; ++i)
			TressFXGroupInstance->Guides.VSPStiffness[i] =
			VSPStiffnessCurve.GetRichCurve()->Eval((float)i / GroupData.TFXData.NumVerticesPerStrand);

		TressFXGroupInstance->Guides.EnableVelocityShockPropagation = TressFXSimulationSettings->EnableVelocityShockPropagation;
		TressFXGroupInstance->Guides.VelocityShockPropagation =
			FVector3f(TressFXSimulationSettings->VSPCoefficient,
				TressFXSimulationSettings->VSPAccelThresholdMax,
				TressFXSimulationSettings->VSPAccelThresholdMin);

		TressFXGroupInstance->Guides.EnableLengthConstraint = TressFXSimulationSettings->EnableLengthConstraint;
		TressFXGroupInstance->Guides.LengthConstraintsIterations =
			TressFXSimulationSettings->LengthConstraintsIterations;
		TressFXGroupInstance->Guides.WindDirection = TressFXSimulationSettings->WindDirection;
		TressFXGroupInstance->Guides.WindMagnitude = TressFXSimulationSettings->WindMagnitude;

		TressFXGroupInstance->Guides.EnableLocalShapeConstraint = TressFXSimulationSettings->EnableLocalShapeConstraint;
		TressFXGroupInstance->Guides.LocalShapeStiffness.AddZeroed(GroupData.TFXData.NumVerticesPerStrand);
		FRuntimeFloatCurve& LocalShapeStiffnessCurve = TressFXSimulationSettings->LocalShapeStiffness;
		for (uint32 i = 0; i < GroupData.TFXData.NumVerticesPerStrand; ++i)
			TressFXGroupInstance->Guides.LocalShapeStiffness[i] =
			FMath::Min(LocalShapeStiffnessCurve.GetRichCurve()->Eval((float)i / GroupData.TFXData.NumVerticesPerStrand), 0.95f);

		const bool bDynamicResources = true;//IsTressFXSimulationEnable() || IsTressFXBindingEnable();

		if (GroupData.HasValidData() && (bDynamicResources || bIsStrandsEnabled))
		{
			TressFXGroupInstance->Guides.bIsSimulationEnable = EnableSimulation && ShouldRunTressFXSimulationInWorld(WorldType);// TressFXSimulationSettings->EnableSimulation;

			TressFXGroupInstance->TFXData = &GroupData.TFXData;

			TressFXGroupInstance->Guides.GuidesRestResource = GroupData.GuidesRestResource;

			if (TressFXBindingAsset)
			{
				TressFXGroupInstance->Guides.GuidesBindingResource = TressFXBindingAsset->GuidesBindingResource;
#if WITH_EDITOR
				TressFXGroupInstance->MTMeshResource = new FTressFXMorphTargetMeshResource(SkeletalMeshComponent);
				BeginInitResource(TressFXGroupInstance->MTMeshResource);
#endif
			}

			TressFXGroupInstance->Guides.GuidesDeformedResource = new FTressFXGuidesDeformedResource(GroupData.TFXData);
			BeginInitResource(TressFXGroupInstance->Guides.GuidesDeformedResource);

#if WITH_EDITOR
			if (GTressFXDebugLogs)
			{
				UE_LOG(LogTemp, Warning, TEXT("TressFX group instance data: asset=%s points=%d strands=%d vertsPerStrand=%u strandsToRender=%u bounds=%s"),
					*GetNameSafe(TressFXAsset),
					GroupData.TFXData.StrandsPoints.PointsPosition.Num(),
					GroupData.TFXData.NumVerticesPerStrand > 0 ? GroupData.TFXData.StrandsPoints.PointsPosition.Num() / static_cast<int32>(GroupData.TFXData.NumVerticesPerStrand) : 0,
					GroupData.TFXData.NumVerticesPerStrand,
					GroupData.TFXData.NumStrandsToRender,
					*GroupData.TFXData.BoundingBox.ToString());
			}
#endif
		}

		// Initialize LOD screen size & visibility
		{
			TressFXGroupInstance->TressFXGroupPublicData = new FTressFXGroupPublicData(GroupIt);
			
			const FTressFXGroupsLOD& GroupLOD = TressFXAsset->TressFXGroupsLOD;
			TressFXGroupInstance->TressFXGroupPublicData->SetContinuousLOD(GroupLOD.EnableContinuousLOD);
			TressFXGroupInstance->TressFXGroupPublicData->VFInput.Strands.NumVerticesPerStrand = GroupData.TFXData.NumVerticesPerStrand;
			TressFXGroupInstance->TressFXGroupPublicData->VFInput.Strands.MaxLength = GroupData.TFXData.MaxRestLength;
			TressFXGroupInstance->TressFXGroupPublicData->CurrentActiveStrandsCount = GroupData.TFXData.NumStrandsToRender;
			TressFXGroupInstance->TressFXGroupPublicData->ContinuousLodRadiusScale = 1.0f;

		}

		if (bIsStrandsEnabled && GroupData.HasValidData())
		{
			TressFXGroupInstance->Strands.Material = TressFXAsset->TressFXGroupsMaterials.Material;
			UMaterialInterface* Material0 = GetMaterial(0);
			if (Material0 && Material0->GetRenderProxy())
			{
				TressFXGroupInstance->Strands.Material = Material0;
			}

			TressFXGroupInstance->Strands.StrandsResource =
				new FTressFXStrandsResource(
					GroupData.TFXData,
					GroupData.ClusterCullingData,
					TressFXGroupInstance->NumFollowStrands,
					TressFXGroupInstance->MaxRadiusAroundGuide
				);
			BeginInitResource(TressFXGroupInstance->Strands.StrandsResource);

			TressFXGroupInstance->TressFXGroupPublicData->SetClusters(GroupData.ClusterCullingData.ClusterCount, GroupData.TFXData.GetNumPoints());
			BeginInitResource(TressFXGroupInstance->TressFXGroupPublicData);
		}

	}

	return TressFXGroupInstance;
}

void UTressFXComponent::DestroyTressFXGroupInstance(FTressFXGroupInstance* Instance)
{
	//UE_LOG(LogTemp, Warning, TEXT("Destroy TressFX Group Instance"));

	if (Instance)
	{
		if (TressFXGroupInstance == Instance)
			TressFXGroupInstance = nullptr;
		{
			FTressFXGroupInstance* LocalInstance = Instance;
			ENQUEUE_RENDER_COMMAND(FTressFXReleaseBuffers)(
				[LocalInstance](FRHICommandListImmediate& RHICmdList)
				{
					// Guides
					if (LocalInstance->IsValid())
					{
						InternalResourceRelease(LocalInstance->Guides.GuidesDeformedResource);

						InternalResourceRelease(LocalInstance->Strands.StrandsResource);
#if WITH_EDITOR
						InternalResourceRelease(LocalInstance->MTMeshResource);
#endif
					}

					InternalResourceRelease(LocalInstance->TressFXGroupPublicData);
					delete LocalInstance;
				});
		}
	}
}

void UTressFXComponent::InitResources(bool bIsBindingReloading)
{
	ReleaseResources();

	if (!TressFXAsset || TressFXAsset->GetNumTressFXGroups() == 0)
	{
		return;
	}

	InitializedResources = TressFXAsset;

	TressFXSimulationSettings = nullptr;
	TressFXAnimSimulationSettings = nullptr;
	LastAnimAsset = nullptr;
	CurrentAnimAsset = nullptr;

	const EWorldType::Type WorldType = GetWorldType(); 
	FString WorldName = GetWorld() ? GetWorld()->GetName() : TEXT("None");

	const bool bIsStrandsEnabled = IsTressFXEnabled(ETressFXShaderType::Strands);

	// Insure that the binding asset is compatible, otherwise no binding will be used
	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetAttachParent());;// ValidateBindingAsset(TressFXAsset, BindingAsset, GetAttachParent() ? Cast<USkeletalMeshComponent>(GetAttachParent()) : nullptr, bIsBindingReloading, bValidationEnable, this);
	if (SkeletalMeshComponent && SkeletalMeshComponent->GetSkinnedAsset() == nullptr)
	{
		SkeletalMeshComponent = nullptr;
	}

	// Insure the ticking of the TressFX component always happens after the skeletalMeshComponent.
	if (SkeletalMeshComponent)
	{
		RegisteredSkeletalMeshComponent = SkeletalMeshComponent;
		AddTickPrerequisiteComponent(SkeletalMeshComponent);
	}

}

void UTressFXComponent::ReleaseResources()
{
	//
	InitializedResources = nullptr;
	TressFXGroupInstance = nullptr;

	// Insure the ticking of the TressFX component always happens after the skeletalMeshComponent.
	if (RegisteredSkeletalMeshComponent)
	{
		RemoveTickPrerequisiteComponent(RegisteredSkeletalMeshComponent);
	}
	RegisteredSkeletalMeshComponent = nullptr;

	MarkRenderStateDirty();
}

void UTressFXComponent::SetTressFXAsset(UTressFXAsset* Asset)
{
	ReleaseResources();
	if (Asset && Asset->IsValid())
	{
		TressFXAsset = Asset;

#if WITH_EDITORONLY_DATA

		TressFXAssetBeingLoaded = nullptr;

#endif
	}
#if WITH_EDITORONLY_DATA
	else if (Asset)
	{
		// The asset is still being loaded. This will allow the assets to be re-set once the groom is finished loading
		TressFXAssetBeingLoaded = Asset;
		//BindingAssetBeingLoaded = InBinding;
	}
#endif
	else
		TressFXAsset = nullptr;
	
	if (!TressFXAsset)// || !TressFXAsset->IsValid())
	{
		Bounds = FBoxSphereBounds(EForceInit::ForceInitToZero);
		return;
	}

	InitResources();
	Bounds = CalcBounds(GetComponentTransform());
	MarkRenderTransformDirty();
	MarkRenderStateDirty();
}

EWorldType::Type UTressFXComponent::GetWorldType() const
{
	EWorldType::Type WorldType = GetWorld() ? EWorldType::Type(GetWorld()->WorldType) : EWorldType::None;
	return WorldType == EWorldType::Inactive ? EWorldType::Editor : WorldType;
}

void UTressFXComponent::OnRegister()
{
	Super::OnRegister();

	// Insure the parent skeletal mesh is the same than the registered skeletal mesh, and if not reinitialized resources
	// This can happens when the OnAttachment callback is not called, but the skeletal mesh change (e.g., the skeletal mesh get recompiled within a blueprint)
	USkeletalMeshComponent* SkeletalMeshComponent = GetAttachParent() ? Cast<USkeletalMeshComponent>(GetAttachParent()) : nullptr;

	const bool bNeedInitialization = !InitializedResources || InitializedResources != TressFXAsset || SkeletalMeshComponent != RegisteredSkeletalMeshComponent;
	if (bNeedInitialization)
	{
		InitResources();
	}

}

void UTressFXComponent::OnUnregister()
{
	Super::OnUnregister();
	
	ReleaseResources();
}

void UTressFXComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	ReleaseResources();

#if WITH_EDITOR
	
#endif

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UTressFXComponent::OnAttachmentChanged()
{
	Super::OnAttachmentChanged();
	if (TressFXAsset && !IsBeingDestroyed() && HasBeenCreated())
	{
		USkeletalMeshComponent* NewSkeletalMeshComponent = Cast<USkeletalMeshComponent>(GetAttachParent());
		const bool bHasAttachmentChanged = RegisteredSkeletalMeshComponent != NewSkeletalMeshComponent;
		if (bHasAttachmentChanged)
		{
			InitResources();
		}
	}
}

void UTressFXComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (TressFXGroupInstance)
	{
		const EWorldType::Type WorldType = GetWorldType();
		TressFXGroupInstance->LocalToWorld = GetComponentTransform();
		TressFXGroupInstance->Guides.bIsSimulationEnable = EnableSimulation && ShouldRunTressFXSimulationInWorld(WorldType);
		TressFXGroupInstance->TimeStep = TressFXGroupInstance->Guides.bIsSimulationEnable ? DeltaTime : 0.0f;
	}

	if (TressFXGroupInstance && TressFXGroupInstance->SkeletalComponent)
	{
		if (TressFXAsset->AnimationSimulationSettingsMap.Num())
		{
			LastAnimAsset = CurrentAnimAsset;
			UAnimSingleNodeInstance* AnimInst = TressFXGroupInstance->SkeletalComponent->GetSingleNodeInstance();
			if (AnimInst)
				CurrentAnimAsset = AnimInst->GetCurrentAsset();

			if (CurrentAnimAsset != LastAnimAsset)
			{
				TressFXAnimSimulationSettings = nullptr;
				FString CurSimulationSettingsName;
				bool FindInMap = false;
				for (int32 i = 0; i < TressFXAsset->AnimationSimulationSettingsMap.Num(); ++i)
				{
					if (CurrentAnimAsset == TressFXAsset->AnimationSimulationSettingsMap[i].Animation.Get())
					{
						CurSimulationSettingsName = TressFXAsset->AnimationSimulationSettingsMap[i].TressFXSimulationSettingsName;
						TressFXAnimSimulationSettings = &TressFXAsset->AnimationSimulationSettingsMap[i];
						FindInMap = true;
						break;
					}
				}
				bool FindInArray = false;
				for (int32 i = 0; i < TressFXAsset->TressFXSimulationSettingsArray.Num(); ++i)
				{
					if (CurSimulationSettingsName == TressFXAsset->TressFXSimulationSettingsArray[i].Name)
					{
						TressFXSimulationSettings = &TressFXAsset->TressFXSimulationSettingsArray[i];
						FindInArray = true;
						break;
					}
				}

				if (!(FindInMap && FindInArray))
					TressFXSimulationSettings = &TressFXAsset->TressFXSimulationSettings;

				{
					FTressFXGroupData& GroupData = TressFXAsset->TressFXGroupsData[0];

					TressFXGroupInstance->Guides.GravityMagnitude = TressFXSimulationSettings->GravityMagnitude;
					TressFXGroupInstance->Guides.Damping = TressFXSimulationSettings->Damping;
					if (TressFXSimulationSettings->EnableGlobalShapeConstraint)
					{
						TressFXGroupInstance->Guides.GlobalShapeStiffness = TressFXSimulationSettings->GlobalShapeStiffness;
						TressFXGroupInstance->Guides.GlobalShapeEffectiveRange = TressFXSimulationSettings->GlobalShapeEffectiveRange;
					}
					else
					{
						TressFXGroupInstance->Guides.GlobalShapeStiffness = 0.f;
						TressFXGroupInstance->Guides.GlobalShapeEffectiveRange = 0.f;
					}

					TressFXGroupInstance->Guides.NumVerticesFromRootNoSimulation = TressFXSimulationSettings->NumVerticesFromRootNoSimulation;

					TressFXGroupInstance->Guides.VSPStiffness.AddZeroed(GroupData.TFXData.NumVerticesPerStrand);
					FRuntimeFloatCurve& VSPStiffnessCurve = TressFXSimulationSettings->VSPStiffness;
					for (uint32 i = 0; i < GroupData.TFXData.NumVerticesPerStrand; ++i)
						TressFXGroupInstance->Guides.VSPStiffness[i] =
						VSPStiffnessCurve.GetRichCurve()->Eval((float)i / GroupData.TFXData.NumVerticesPerStrand);

					TressFXGroupInstance->Guides.EnableVelocityShockPropagation = TressFXSimulationSettings->EnableVelocityShockPropagation;
					TressFXGroupInstance->Guides.VelocityShockPropagation =
						FVector3f(TressFXSimulationSettings->VSPCoefficient,
							TressFXSimulationSettings->VSPAccelThresholdMax,
							TressFXSimulationSettings->VSPAccelThresholdMin);

					TressFXGroupInstance->Guides.EnableLengthConstraint = TressFXSimulationSettings->EnableLengthConstraint;
					TressFXGroupInstance->Guides.LengthConstraintsIterations =
						TressFXSimulationSettings->LengthConstraintsIterations;
					TressFXGroupInstance->Guides.WindDirection = TressFXSimulationSettings->WindDirection;
					TressFXGroupInstance->Guides.WindMagnitude = TressFXSimulationSettings->WindMagnitude;

					TressFXGroupInstance->Guides.EnableLocalShapeConstraint = TressFXSimulationSettings->EnableLocalShapeConstraint;
					TressFXGroupInstance->Guides.LocalShapeStiffness.AddZeroed(GroupData.TFXData.NumVerticesPerStrand);
					FRuntimeFloatCurve& LocalShapeStiffnessCurve = TressFXSimulationSettings->LocalShapeStiffness;
					for (uint32 i = 0; i < GroupData.TFXData.NumVerticesPerStrand; ++i)
						TressFXGroupInstance->Guides.LocalShapeStiffness[i] =
						FMath::Min(LocalShapeStiffnessCurve.GetRichCurve()->Eval((float)i / GroupData.TFXData.NumVerticesPerStrand), 0.95f);

				}
			}
		}


		UAnimSingleNodeInstance* AnimInstance = TressFXGroupInstance->SkeletalComponent->GetSingleNodeInstance();
		//TressFXGroupInstance->SkeletalComponent->AnimationData.AnimToPlay.Get();
		if (TressFXSimulationSettings && AnimInstance && AnimInstance->IsPlaying())
		{
			EWorldType::Type WorldType = GetWorld()->WorldType;
			float CurPos = AnimInstance->GetCurrentTime();
			if (WorldType != EWorldType::Editor)
			{
				if (CurPos < 0.033f)
				{
					if (TressFXSimulationSettings->ResetPositionAtStart)
					{
						TressFXGroupInstance->Guides.GuidesDeformedResource->bResetPositions = true;
					}
				}
				else
				{
					if (TressFXAnimSimulationSettings && TressFXAnimSimulationSettings->AnimResetPositionPeriods.Num())
					{
						for (int32 i = 0; i < TressFXAnimSimulationSettings->AnimResetPositionPeriods.Num(); ++i)
						{
							if (CurPos >= TressFXAnimSimulationSettings->AnimResetPositionPeriods[i].Start &&
								CurPos <= TressFXAnimSimulationSettings->AnimResetPositionPeriods[i].End)
							{
								TressFXGroupInstance->Guides.GuidesDeformedResource->bResetPositions = true;
								break;
							}
						}
					}
				}

			}
		}
	}

	USkeletalMeshComponent* SkeletalMeshComponent = RegisteredSkeletalMeshComponent;
	{
		if (SkeletalMeshComponent)
		{
			// When a skeletal object with projection is enabled, activate the refresh of the bounding box to 
			// insure the component/proxy bounding box always lies onto the actual skinned mesh
			MarkRenderTransformDirty();
		}
	}
}

void UTressFXComponent::SendRenderTransform_Concurrent()
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

FBoxSphereBounds UTressFXComponent::CalcBounds(const FTransform& InLocalToWorld) const
{
	if (TressFXAsset && TressFXAsset->GetNumTressFXGroups() > 0)
	{
		FBox LocalHairBounds(EForceInit::ForceInit);

		for (const FTressFXGroupData& GroupData : TressFXAsset->TressFXGroupsData)
		{
			if (GroupData.HasValidData())
			{
				FBox GroupBounds;
				GroupBounds.Min = FVector(GroupData.TFXData.BoundingBox.Min.X, GroupData.TFXData.BoundingBox.Min.Y, GroupData.TFXData.BoundingBox.Min.Z);
				GroupBounds.Max = FVector(GroupData.TFXData.BoundingBox.Max.X, GroupData.TFXData.BoundingBox.Max.Y, GroupData.TFXData.BoundingBox.Max.Z);
				GroupBounds.IsValid = 1;
				LocalHairBounds += GroupBounds;
			}
		}

		if (!LocalHairBounds.IsValid)
		{
			for (const FTressFXGroupData& GroupData : TressFXAsset->TressFXGroupsData)
			{
				for (const FVector4f& Point : GroupData.TFXData.StrandsPoints.PointsPosition)
				{
					LocalHairBounds += FVector(Point.X, Point.Y, Point.Z);
				}
			}
		}

		if (!LocalHairBounds.IsValid)
		{
			return FBoxSphereBounds(EForceInit::ForceInitToZero);
		}

		FBox LocalBounds = LocalHairBounds;
		if (RegisteredSkeletalMeshComponent)
		{
			FBoxSphereBounds SkelRestBounds;
			RegisteredSkeletalMeshComponent->GetPreSkinnedLocalBounds(SkelRestBounds);
			const FBox SkelBox = SkelRestBounds.GetBox();
			if (SkelBox.IsValid)
			{
				LocalBounds += SkelBox;
			}
		}

		const FBoxSphereBounds OutBounds(LocalBounds.TransformBy(InLocalToWorld));
#if WITH_EDITOR
		if (GTressFXDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("TressFX CalcBounds: asset=%s registeredSkel=%s localHair=%s out=%s"),
				*GetNameSafe(TressFXAsset),
				*GetNameSafe(RegisteredSkeletalMeshComponent),
				*LocalHairBounds.ToString(),
				*OutBounds.GetBox().ToString());
		}
#endif
		return OutBounds;
	}
	else
	{
		return FBoxSphereBounds(EForceInit::ForceInitToZero);
	}
}

FPrimitiveSceneProxy* UTressFXComponent::CreateSceneProxy()
{
	if (!TressFXAsset || TressFXAsset->GetNumTressFXGroups() == 0)
	{
#if WITH_EDITOR
		if (GTressFXDebugLogs)
		{
			UE_LOG(LogTemp, Warning, TEXT("TressFX CreateSceneProxy skipped: asset=%s groups=%d"),
				*GetNameSafe(TressFXAsset),
				TressFXAsset ? TressFXAsset->GetNumTressFXGroups() : 0);
		}
#endif
		return nullptr;
	}

	Bounds = CalcBounds(GetComponentTransform());

#if WITH_EDITOR
	if (GTressFXDebugLogs)
	{
		UE_LOG(LogTemp, Warning, TEXT("TressFX CreateSceneProxy: asset=%s groups=%d bounds=%s"),
			*GetNameSafe(TressFXAsset),
			TressFXAsset->GetNumTressFXGroups(),
			*Bounds.GetBox().ToString());
	}
#endif

	return new FTressFXSceneProxy(this);
}

void UTressFXComponent::PostLoad()
{
	
	Super::PostLoad();


	SetTressFXAsset(TressFXAsset);

}

int32 UTressFXComponent::GetDesiredSyncLOD() const
{
	return 0;
}

int32 UTressFXComponent::GetNumSyncLODs() const
{
	return 1;
}

int32 UTressFXComponent::GetNumLODs() const
{
	return 1;// return TressFXAsset ? TressFXAsset->GetLODCount() : 0;
}

int32 UTressFXComponent::GetForcedLOD() const
{
	return int32(LODForcedIndex);
}

int32 UTressFXComponent::GetBestAvailableLOD() const
{
	// For now we assume all LODs are available. This could be made more accurate in future.
	return 0;
}

void UTressFXComponent::SetForceStreamedLOD(int32 LODIndex)
{
	// Force streaming is not supported yet
}

void UTressFXComponent::SetForceRenderedLOD(int32 InLODIndex)
{
	//SetForcedLOD(InLODIndex);
}

int32 UTressFXComponent::GetForceStreamedLOD() const
{
	// Force streaming is not supported yet
	return INDEX_NONE;
}

int32 UTressFXComponent::GetForceRenderedLOD() const
{
	return GetForcedLOD();
}

void UTressFXComponent::SetWindDirection(const FVector3f& WindDirection)
{
	if (TressFXGroupInstance)
	{
		TressFXGroupInstance->Guides.WindDirection = WindDirection;
	}
}

void UTressFXComponent::SetWindForce(const float WindForce)
{
	if (TressFXGroupInstance)
	{
		TressFXGroupInstance->Guides.WindMagnitude = WindForce;
	}
}

void UTressFXComponent::SetGSR(const float GSR)
{
	if (TressFXGroupInstance)
	{
		TressFXGroupInstance->Guides.GlobalShapeEffectiveRange = GSR;
	}
}

void UTressFXComponent::SetGSS(const float GSS)
{
	if (TressFXGroupInstance)
	{
		TressFXGroupInstance->Guides.GlobalShapeStiffness = GSS;
	}
}


#if WITH_EDITORONLY_DATA
FTressFXComponentRecreateRenderStateContext::FTressFXComponentRecreateRenderStateContext(UTressFXAsset* TressFXAsset)
{
	if (!TressFXAsset)
	{
		return;
	}

	for (TObjectIterator<UTressFXComponent> TressFXComponentIt; TressFXComponentIt; ++TressFXComponentIt)
	{
		if (TressFXComponentIt->TressFXAsset == TressFXAsset ||
			TressFXComponentIt->TressFXAssetBeingLoaded == TressFXAsset) // A TressFXAsset was set on the component while it was still loading
		{
			if (TressFXComponentIt->IsRenderStateCreated())
			{
				TressFXComponentIt->DestroyRenderState_Concurrent();
				TressFXComponents.Add(*TressFXComponentIt);
			}
		}
	}

	// Flush the rendering commands generated by the detachments.
	FlushRenderingCommands();
}

FTressFXComponentRecreateRenderStateContext::~FTressFXComponentRecreateRenderStateContext()
{
	const int32 ComponentCount = TressFXComponents.Num();
	for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
	{
		UTressFXComponent* TressFXComponent = TressFXComponents[ComponentIndex];

		if (TressFXComponent->IsRegistered() && !TressFXComponent->IsRenderStateCreated())
		{
			if (TressFXComponent->TressFXAssetBeingLoaded && TressFXComponent->TressFXAssetBeingLoaded->IsValid())
			{
				// Re-set the assets on the component now that they are loaded
				TressFXComponent->SetTressFXAsset(TressFXComponent->TressFXAssetBeingLoaded);// , TressFXComponent->BindingAssetBeingLoaded);
			}
			else
			{
				//TressFXComponent->InitResources();
			}
			TressFXComponent->CreateRenderState_Concurrent(nullptr);
		}
	}
}
#endif
