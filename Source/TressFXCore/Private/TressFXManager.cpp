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

#include "TressFXManager.h"
#include "TressFXInstance.h"
#include "TressFXMeshInstance.h"
#include "TressFXCoreRendering.h"
#include "TressFXSDFComponent.h"
#include "GPUSkinCache.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "RenderGraphEvent.h"


DEFINE_LOG_CATEGORY_STATIC(LogTressFXManager, Log, All);


static int32 GTressFX_InterpolationFrustumCullingEnable = 0;
static FAutoConsoleVariableRef CVarTressFX_InterpolationFrustumCullingEnable(TEXT("r.TressFX.Interoplation.FrustumCulling"), GTressFX_InterpolationFrustumCullingEnable, TEXT("Swap rendering buffer at the end of frame. This is an experimental toggle. Default:1"));

static int32 GTressFXMorphTargetMeshVisualizationEnable = 0;
static FAutoConsoleVariableRef CVarTressFXMorphTargetMeshVisualizationEnable(TEXT("r.TressFX.MorphTargetMeshVisualization"), GTressFXMorphTargetMeshVisualizationEnable, TEXT("Enable accurate transmittance pass for better rendering of small scale TressFX."));



struct FTressFXManager
{
	// #hair_todo: change this array to a queue update, in order make processing/update thread safe.
	TArray<struct FTressFXGroupInstance*> Instances;
	TArray<struct FTressFXMeshGroupInstance*> MeshInstances;

	FTressFXManager()
	{
		// Reserve a large a mount of object to avoid any potential memory reallocation, which 
		// could cause some thread safety issue. This is a workaround against the non-thread-safe array
		Instances.Reserve(64);
		MeshInstances.Reserve(64);
	}
};

FTressFXManager GTressFXManager;

#if WITH_EDITOR

void ProcessTressFXVisualizeTangent(
	FRDGBuilder& GraphBuilder,
	FTressFXVisualizeTangentParameters& VisualizeTangentParameters)
{
	for (int32 Index = 0; Index < GTressFXManager.Instances.Num();++Index)
	{
		if (!GTressFXManager.Instances[Index]->Debug.bEnableVisualizeTangents)
			continue;

		if (GTressFXManager.Instances[Index]->WorldType == VisualizeTangentParameters.WorldType
			&& GTressFXManager.Instances[Index]->WorldName == VisualizeTangentParameters.WorldName)
		{
			FTressFXGroupInstance* Instance = GTressFXManager.Instances[Index];
			
			VisualizeTangentParameters.VertexCount = Instance->Strands.StrandsResource->GetVertexCount();

			VisualizeTangentParameters.StrandsDeformedPositionBuffer
				= RegisterAsSRV(GraphBuilder, Instance->Strands.StrandsResource->GetBuffer(FTressFXStrandsResource::Current));
			VisualizeTangentParameters.StrandsTangentBuffer =
				RegisterAsSRV(GraphBuilder, Instance->Strands.StrandsResource->TangentBuffer);
			break;
		}
	}
}

void ProcessTressFXVisualizeMesh(
	FRDGBuilder& GraphBuilder,
	FTressFXVisualizeMeshParameters& VisualizeMeshParameters)
{
	for (int32 Index = 0; Index < GTressFXManager.MeshInstances.Num(); ++Index)
	{
		if (!GTressFXManager.MeshInstances[Index]->Debug.bEnableVisualizeMesh)
			continue;

		if (GTressFXManager.MeshInstances[Index]->WorldType == VisualizeMeshParameters.WorldType
			&& GTressFXManager.MeshInstances[Index]->WorldName == VisualizeMeshParameters.WorldName)
		{
			FTressFXMeshGroupInstance* Instance = GTressFXManager.MeshInstances[Index];

			{
				VisualizeMeshParameters.MeshVertexBuffer
					= RegisterAsSRV(GraphBuilder, Instance->MeshDeformedResource->MeshDeformedVertexBuffer);
			}
			

			VisualizeMeshParameters.IndicesBufferRHI = Instance->MeshRestResource->IndicesBuffer;

			VisualizeMeshParameters.IndicesCount = Instance->TFXMeshData->GetNumIndices();
			VisualizeMeshParameters.VertexCount = Instance->TFXMeshData->GetNumVertices();

			break;
		}
	}
}

void ProcessTressFXVisualizeMeshAABB(
	FRDGBuilder& GraphBuilder,
	FTressFXVisualizeMeshAABBParameters& VisualizeMeshAABBParameters)
{
	for (int32 Index = 0; Index < GTressFXManager.MeshInstances.Num(); ++Index)
	{
		if (!GTressFXManager.MeshInstances[Index]->Debug.bEnableVisualizeMeshAABB)
			continue;

		if (GTressFXManager.MeshInstances[Index]->WorldType == VisualizeMeshAABBParameters.WorldType
			&& GTressFXManager.MeshInstances[Index]->WorldName == VisualizeMeshAABBParameters.WorldName)
		{
			FTressFXMeshGroupInstance* Instance = GTressFXManager.MeshInstances[Index];

			{
				VisualizeMeshAABBParameters.MeshAABBBuffer
					= RegisterAsSRV(GraphBuilder, Instance->MeshDeformedResource->CollisionMeshBoxBuffer);
			}

			break;
		}
	}
}


void ProcessTressFXVisualizeSDF(
	FRDGBuilder& GraphBuilder,
	FTressFXVisualizeSDFParameters& VisualizeSDFParameters)
{
	for (int32 Index = 0; Index < GTressFXManager.MeshInstances.Num(); ++Index)
	{
		if (!GTressFXManager.MeshInstances[Index]->Debug.bEnableVisualizeSDF)
			continue;

		if (GTressFXManager.MeshInstances[Index]->WorldType == VisualizeSDFParameters.WorldType
			&& GTressFXManager.MeshInstances[Index]->WorldName == VisualizeSDFParameters.WorldName)
		{
			FTressFXMeshGroupInstance* Instance = GTressFXManager.MeshInstances[Index];

			{
				VisualizeSDFParameters.CMBoxBuffer
					= RegisterAsSRV(GraphBuilder, Instance->MeshDeformedResource->CollisionMeshBoxBuffer);
				VisualizeSDFParameters.SDFBuffer
					= RegisterAsSRV(GraphBuilder, Instance->MeshDeformedResource->SDFBuffer);
				VisualizeSDFParameters.SDFDirectionStartOffsetBuffer
					= RegisterAsSRV(GraphBuilder, Instance->MeshDeformedResource->SDFDirectionStartOffsetBuffer);
				VisualizeSDFParameters.SDFDirectionBuffer
					= RegisterAsSRV(GraphBuilder, Instance->MeshDeformedResource->SDFDirectionBuffer);
				VisualizeSDFParameters.NumSDFCells = Instance->NumSDFCells;
			}

			break;
		}
	}
}

void ProcessTressFXVisualizeMTMesh(
	FRDGBuilder& GraphBuilder,
	FTressFXVisualizeMTMeshParameters& VisualizeMeshParameters)
{
	if (0 == GTressFXMorphTargetMeshVisualizationEnable)
		return;

	for (int32 Index = 0; Index < GTressFXManager.Instances.Num(); ++Index)
	{
		if (!GTressFXManager.Instances[Index]->MTMeshResource)
			continue;

		if (GTressFXManager.Instances[Index]->WorldType == VisualizeMeshParameters.WorldType
			&& GTressFXManager.Instances[Index]->WorldName == VisualizeMeshParameters.WorldName)
		{
			FTressFXGroupInstance* Instance = GTressFXManager.Instances[Index];

			VisualizeMeshParameters.MeshVertexBuffer 
				= RegisterAsSRV(GraphBuilder, Instance->MTMeshResource->MTMeshDeformedPositionBuffer);
		
			VisualizeMeshParameters.IndicesBufferRHI = Instance->MTMeshIndicesBufferRHI;

			VisualizeMeshParameters.IndicesCount = Instance->MTMeshIndicesCount;
			VisualizeMeshParameters.VertexCount = Instance->MTMeshVertexCount;

			break;
		}
	}
}

#endif

void RegisterTressFXGroupInstance_RenderThread(FTressFXGroupInstance* InInstance)
{
	check(IsInRenderingThread());
	for (const FTressFXGroupInstance* Instance : GTressFXManager.Instances)
	{
		if (Instance->Debug.ComponentId == InInstance->Debug.ComponentId &&
			Instance->Debug.GroupIndex == InInstance->Debug.GroupIndex)
		{
			// Component already registered. This should not happen. 
			UE_LOG(LogTressFXManager, Warning, TEXT("Component already register. This should't happen. Please report this to a rendering engineer."))
				return;
		}
	}

	//check(InInstance->TressFXGroupPublicData);
	GTressFXManager.Instances.Add(InInstance);
}

void UnregisterTressFXGroupInstance_RenderThread(struct FTressFXGroupInstance* InInstance)
{
	check(IsInRenderingThread());
	for (int32 Index = 0;Index < GTressFXManager.Instances.Num();)
	{
		const FTressFXGroupInstance* Instance = GTressFXManager.Instances[Index];

		if (Instance == InInstance)
		{
			GTressFXManager.Instances[Index] = GTressFXManager.Instances[GTressFXManager.Instances.Num() - 1];
			GTressFXManager.Instances.SetNum(GTressFXManager.Instances.Num() - 1, false);
		}
		else
		{
			++Index;
		}
	}
}

void RegisterTressFXMeshGroupInstance_RenderThread(FTressFXMeshGroupInstance* InInstance)
{
	check(IsInRenderingThread());
	for (const FTressFXMeshGroupInstance* Instance : GTressFXManager.MeshInstances)
	{
		if (Instance->Debug.ComponentId == InInstance->Debug.ComponentId)
		{
			// Component already registered. This should not happen. 
			UE_LOG(LogTressFXManager, Warning, TEXT("Component already register. This should't happen. Please report this to a rendering engineer."))
				return;
		}
	}

	GTressFXManager.MeshInstances.Add(InInstance);
}

void UnregisterTressFXMeshGroupInstance_RenderThread(FTressFXMeshGroupInstance* InInstance)
{
	check(IsInRenderingThread());
	for (int32 Index = 0; Index < GTressFXManager.MeshInstances.Num();)
	{
		const FTressFXMeshGroupInstance* Instance = GTressFXManager.MeshInstances[Index];

		if (Instance == InInstance)
		{
			GTressFXManager.MeshInstances[Index] = GTressFXManager.MeshInstances[GTressFXManager.MeshInstances.Num() - 1];
			GTressFXManager.MeshInstances.SetNum(GTressFXManager.MeshInstances.Num() - 1, false);
		}
		else
		{
			++Index;
		}
	}
}


static bool IsInstanceVisible(
	const FSceneView* View,
	FTressFXGroupInstance* Instance)
{
	// Frustum culling for rendering strands. Update position only for visible/in camera frustum
	if (GTressFX_InterpolationFrustumCullingEnable > 0)
	{
		if (!Instance->ProxyBounds)
		{
			return false;
		}

		const FSphere InstanceBound = Instance->ProxyBounds->GetSphere();
		bool bFullyContained = false;
		if (!View->ViewFrustum.IntersectSphere(InstanceBound.Center, InstanceBound.W, bFullyContained))
		{
			return false;
		}
	}

	return true;
}

void RegisterClusterData(FTressFXGroupInstance* Instance, FTressFXClusterData* InClusterData)
{
	// Initialize group cluster data for culling by the renderer
	const int32 ClusterDataGroupIndex = InClusterData->HairGroups.Num();
	FTressFXClusterData::FHairGroup& HairGroupCluster = InClusterData->HairGroups.Emplace_GetRef();
	HairGroupCluster.ClusterCount = Instance->TressFXGroupPublicData->GetClusterCount();
	HairGroupCluster.VertexCount = Instance->TressFXGroupPublicData->GetGroupControlPointCount();// GetGroupInstanceVertexCount();
	HairGroupCluster.GroupAABBBuffer = &Instance->TressFXGroupPublicData->GetGroupAABBBuffer();
	HairGroupCluster.ClusterAABBBuffer = &Instance->TressFXGroupPublicData->GetClusterAABBBuffer();

	HairGroupCluster.NumFollowStrands = Instance->NumFollowStrands;
	HairGroupCluster.NumVerticesPerStrand = Instance->TFXData->NumVerticesPerStrand;

	HairGroupCluster.ClusterLODInfoBuffer = &Instance->Strands.StrandsResource->ClusterLODInfoBuffer;
	
	HairGroupCluster.TressFXGroupPublicPtr = Instance->TressFXGroupPublicData;
	HairGroupCluster.LODBias = Instance->TressFXGroupPublicData->GetLODBias();
	HairGroupCluster.LODIndex = Instance->TressFXGroupPublicData->GetLODIndex();
	HairGroupCluster.bVisible = Instance->TressFXGroupPublicData->GetLODVisibility();

	HairGroupCluster.TressFXGroupPublicPtr->ClusterDataIndex = ClusterDataGroupIndex;
}

static void RunTressFXGatherCluster(
	EWorldType::Type WorldType,
	FTressFXClusterData* ClusterData)
{
	for (FTressFXGroupInstance* Instance : GTressFXManager.Instances)
	{
		if (Instance->WorldType != WorldType || Instance->GeometryType != ETressFXGeometryType::Strands_)
			continue;

		if (Instance->IsValid())
		{
			RegisterClusterData(Instance, ClusterData);
		}
	}
}

static void RunInternalTressFXInterpolation(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	EWorldType::Type WorldType,
	//const FShaderDrawDebugData* ShaderDrawData,
	FGlobalShaderMap* ShaderMap,
	ETressFXInterpolationType Type,
	FTressFXClusterData* ClusterData)
{
	check(IsInRenderingThread());
	
	for (FTressFXGroupInstance* Instance : GTressFXManager.Instances)
	{
		// Frustum culling for rendering strands. Update position only for visible/in camera frustum
		if (Type == ETressFXInterpolationType::RenderStrands && !IsInstanceVisible(View, Instance))
		{
			continue;
		}

	}

	// Hair interpolation
	if (ETressFXInterpolationType::RenderStrands == Type)
	{
		for (FTressFXGroupInstance* Instance : GTressFXManager.Instances)
		{
			if (Instance->WorldType != WorldType || Instance->GeometryType == ETressFXGeometryType::NoneGeometry_)
				continue;

			// Frustum culling for rendering strands. Update position only for visible/in camera frustum
			if (!IsInstanceVisible(View, Instance))
			{
				continue;
			}

			ComputeTressFXInterpolation(
				GraphBuilder,
				ShaderMap,
				//ShaderDrawData,
				Instance,
				0,
				ClusterData);

			Instance->Strands.UniformBuffer.UpdateUniformBufferImmediate(GraphBuilder.RHICmdList, Instance->GetTressFXUniformShaderParameters());
		}
	}
}

static void RunTressFXInterpolation(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	EWorldType::Type WorldType,
	//const FShaderDrawDebugData* ShaderDrawData,
	FGlobalShaderMap* ShaderMap,
	FTressFXClusterData* ClusterData)
{
	check(IsInRenderingThread());

	DECLARE_GPU_STAT(TressFXInterpolation);
	RDG_EVENT_SCOPE(GraphBuilder, "TressFXInterpolation");
	RDG_GPU_STAT_SCOPE(GraphBuilder, TressFXInterpolation);

	RunInternalTressFXInterpolation(
		GraphBuilder,
		View,
		WorldType,
		//ShaderDrawData,
		ShaderMap,
		ETressFXInterpolationType::RenderStrands,
		ClusterData);
}

bool HasTressFXInstanceSimulationEnable(FTressFXGroupInstance* Instance, int32 MeshLODIndex);

static void RunInternalTressFXSimulation(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	EWorldType::Type WorldType,
	const FString& WorldName,
	const FGPUSkinCache* SkinCache,
	//const FShaderDrawDebugData* ShaderDrawData,
	FGlobalShaderMap* ShaderMap,
	TRefCountPtr<FRDGPooledBuffer>* GPUScenePrimitiveBuffer,
	ETressFXInterpolationType Type)
{
	check(IsInRenderingThread());

	if (ETressFXInterpolationType::SimulationStrands == Type)
	{
		for (FTressFXGroupInstance* Instance : GTressFXManager.Instances)
		{
			if (Instance->WorldType != WorldType || Instance->GeometryType == ETressFXGeometryType::NoneGeometry_)
				continue;

			// Frustum culling guide deformation if the instance does not have any simulation
			if (Instance->GeometryType == ETressFXGeometryType::Strands_ && !HasTressFXInstanceSimulationEnable(Instance, Instance->Debug.MeshLODIndex) && !IsInstanceVisible(View, Instance))
			{
				continue;
			}

			if (Instance->WorldName != WorldName)
				continue;

			if (!Instance->Guides.GuidesRestResource->IsInitialized())
				continue;
			
			ComputeTressFXSimulation(GraphBuilder, View, ShaderMap, GPUScenePrimitiveBuffer,
				SkinCache, Instance, Instance->Debug.MeshLODIndex, GTressFXManager.MeshInstances);

		}
	}

}

static void RunTressFXGuideSimulation(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	EWorldType::Type WorldType,
	const FString& WorldName,
	const FGPUSkinCache* SkinCache,
	//const FShaderDrawDebugData* ShaderDrawData,
	FGlobalShaderMap* ShaderMap,
	TRefCountPtr<FRDGPooledBuffer>* GPUScenePrimitiveBuffer)
{
	check(IsInRenderingThread());

	DECLARE_GPU_STAT(TressFXGuideSimulation);
	RDG_EVENT_SCOPE(GraphBuilder, "TressFXGuideSimulation");
	RDG_GPU_STAT_SCOPE(GraphBuilder, TressFXGuideSimulation);

	RunInternalTressFXSimulation(
		GraphBuilder,
		View,
		WorldType,
		WorldName,
		SkinCache,
		//ShaderDrawData,
		ShaderMap,
		GPUScenePrimitiveBuffer,
		ETressFXInterpolationType::SimulationStrands);
}

static void RunTressFXBufferSwap(EWorldType::Type WorldType, const TArray<const FSceneView*> Views)
{
	for (FTressFXGroupInstance* Instance : GTressFXManager.Instances)
	{
		int32 MeshLODIndex = -1;
		if (!Instance || Instance->WorldType != WorldType)
			continue;

		check(Instance->TressFXGroupPublicData);

		// 1. Swap current/previous buffer for all LODs
		const bool bIsPaused = Views[0]->Family->bWorldIsPaused;
		if (!bIsPaused)
		{
			//Instance->Guides.GuidesDeformedResource->bResetPositions = false;
			
			if(Instance->Strands.StrandsResource)
				Instance->Strands.StrandsResource->SwapBuffer();
		}
	}
}

static void RunTressFXPositionOffsetUpdate(EWorldType::Type WorldType, const TArray<const FSceneView*> Views)
{

	for (FTressFXGroupInstance* Instance : GTressFXManager.Instances)
	{
		int32 MeshLODIndex = -1;
		if (!Instance || Instance->WorldType != WorldType)
			continue;

		check(Instance->TressFXGroupPublicData);

		const bool bIsPaused = Views[0]->Family->bWorldIsPaused;
		
		if (Instance->ProxyBounds && !bIsPaused)
		{
			const FVector PositionOffset3d = Instance->ProxyBounds->GetSphere().Center;
			FVector3f PositionOffset(PositionOffset3d.X, PositionOffset3d.Y, PositionOffset3d.Z);
			//Instance->Guides.GuidesDeformedResource->GetPositionOffset(FTressFXGuidesDeformedResource::Current) = PositionOffset;
		}

	}
}

static void RunTressFXSDFGen(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	EWorldType::Type WorldType,
	const FString& WorldName,
	FGlobalShaderMap* ShaderMap,
	TRefCountPtr<FRDGPooledBuffer>* GPUScenePrimitiveBuffer)
{
	check(IsInRenderingThread());

	DECLARE_GPU_STAT(TressFXSDFGen);
	RDG_EVENT_SCOPE(GraphBuilder,"TressFXSDFGen");
	RDG_GPU_STAT_SCOPE(GraphBuilder, TressFXSDFGen);


	for (FTressFXMeshGroupInstance* Instance : GTressFXManager.MeshInstances)
	{
		if (Instance->WorldType != WorldType || Instance->WorldName != WorldName)
			continue;

		// TODO: Visibility Check and Cull
		if (!Instance->ParentSDFComp || !Instance->ParentSDFComp->EnableSDF)
			continue;

		ComputeTressFXSDFGen(GraphBuilder, View, ShaderMap, GPUScenePrimitiveBuffer, Instance);
	}
}

// Return the LOD which should be used for a given screen size and LOD bias value
static float GetTressFXInstanceLODIndex(const TArray<float>& InLODScreenSizes, float InScreenSize, float InLODBias)
{
	const uint32 LODCount = InLODScreenSizes.Num();
	check(LODCount > 0);

	float OutLOD = 0;
	if (LODCount > 1 && InScreenSize < InLODScreenSizes[0])
	{
		for (uint32 LODIt = 1; LODIt < LODCount; ++LODIt)
		{
			if (InScreenSize >= InLODScreenSizes[LODIt])
			{
				uint32 PrevLODIt = LODIt - 1;

				const float S_Delta = abs(InLODScreenSizes[PrevLODIt] - InLODScreenSizes[LODIt]);
				const float S = S_Delta > 0 ? FMath::Clamp(FMath::Abs(InScreenSize - InLODScreenSizes[LODIt]) / S_Delta, 0.f, 1.f) : 0;
				OutLOD = PrevLODIt + (1 - S);
				break;
			}
			else if (LODIt == LODCount - 1)
			{
				OutLOD = LODIt;
			}
		}
	}

	if (InLODBias != 0)
	{
		OutLOD = FMath::Clamp(OutLOD + InLODBias, 0.f, float(LODCount - 1));
	}
	return OutLOD;
}

static void RunTressFXLODSelection(EWorldType::Type WorldType, const TArray<const FSceneView*> Views)
{
	EShaderPlatform ShaderPlatform = EShaderPlatform::SP_NumPlatforms;
	if (Views.Num() > 0)
	{
		ShaderPlatform = Views[0]->GetShaderPlatform();
	}

	for (FTressFXGroupInstance* Instance : GTressFXManager.Instances)
	{
		int32 MeshLODIndex = -1;
		if (!Instance || Instance->WorldType != WorldType)
			continue;

		check(Instance->TressFXGroupPublicData);

		const float PrevLODIndex = Instance->TressFXGroupPublicData->GetLODIndex();
		float LODIndex = 0.f;
		float MinLOD = 0.f;
		if (Instance->bUseCPULODSelection)
		{
			const FSphere SphereBound = Instance->ProxyBounds ? Instance->ProxyBounds->GetSphere() : FSphere(0);
			for (const FSceneView* View : Views)
			{
				const float ScreenSize = FMath::Min(2.f,ComputeBoundsScreenSize(FVector4(SphereBound.Center, 1), SphereBound.W, *View));
				const float LODBias = 0;// Instance->Strands.Modifier.LODBias;
				
				Instance->TressFXGroupPublicData->SetCurrentLODScreenSize(ScreenSize);
				
				Instance->TressFXGroupPublicData->CurrentActiveStrandsCount = Instance->TFXData->NumStrandsToRender* FMath::Min(ScreenSize / 4.f, 1.f);
				if (EWorldType::EditorPreview == Instance->WorldType)
					Instance->TressFXGroupPublicData->CurrentActiveStrandsCount = Instance->TFXData->NumStrandsToRender * 0.5f;
				Instance->TressFXGroupPublicData->ContinuousLodRadiusScale = Instance->TFXData->NumStrandsToRender /
					float(Instance->TressFXGroupPublicData->CurrentActiveStrandsCount);
			}
		}

	}
}

void ProcessTressFXBookmark(
	FRDGBuilder& GraphBuilder,
	ETressFXBookmark Bookmark,
	FTressFXBookmarkParameters& Parameters)
{
	
	if (Bookmark == ETressFXBookmark::ProcessSDFGen)
	{
		RunTressFXSDFGen(
			GraphBuilder,
			Parameters.View,
			Parameters.WorldType,
			Parameters.WorldName,
			Parameters.ShaderMap,
			Parameters.GPUScenePrimitiveBuffer);

	}
	else if (Bookmark == ETressFXBookmark::ProcessGuideSimulation)
	{
		RunTressFXPositionOffsetUpdate(
			Parameters.WorldType,
			Parameters.AllViews);

		RunTressFXGuideSimulation(
			GraphBuilder,
			Parameters.View,
			Parameters.WorldType,
			Parameters.WorldName,
			Parameters.SkinCache,
			//Parameters.DebugShaderData,
			Parameters.ShaderMap,
			Parameters.GPUScenePrimitiveBuffer);
	}
	else if (Bookmark == ETressFXBookmark::ProcessGatherCluster)
	{
		RunTressFXGatherCluster(
			Parameters.WorldType,
			&Parameters.HairClusterData);
	}
	else if (Bookmark == ETressFXBookmark::ProcessStrandsInterpolation)
	{
		RunTressFXInterpolation(
			GraphBuilder,
			Parameters.View,
			Parameters.WorldType,
			//Parameters.DebugShaderData,
			Parameters.ShaderMap,
			&Parameters.HairClusterData);
	}
	else if (Bookmark == ETressFXBookmark::ProcessLODSelection)
	{
		RunTressFXLODSelection(
			Parameters.WorldType,
			Parameters.AllViews);
	}
	else if (Bookmark == ETressFXBookmark::ProcessEndOfFrame)
	{
		RunTressFXBufferSwap(
			Parameters.WorldType,
			Parameters.AllViews);
	}

}

void ProcessTressFXParameters(FTressFXBookmarkParameters& Parameters)
{
	Parameters.bHasElements = GTressFXManager.Instances.Num() > 0;
	Parameters.bHasMeshElements = GTressFXManager.MeshInstances.Num() > 0;
}
