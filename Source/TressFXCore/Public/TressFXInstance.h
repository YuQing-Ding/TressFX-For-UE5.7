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
#include "TressFXCommon.h"
#include "TressFXInterface.h"
#include "TressFXResources.h"
#include "RenderGraphDefinitions.h"
#include "ShaderParameterMacros.h"


BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FTressFXVertexFactoryUniformShaderParameters, TRESSFXCORE_API)
	SHADER_PARAMETER(FVector3f, PositionOffset)
	SHADER_PARAMETER(FVector3f, PreviousPositionOffset)
	SHADER_PARAMETER(float, Density)
	SHADER_PARAMETER(float, Radius)
	SHADER_PARAMETER(float, TranslucencyDensity)
	SHADER_PARAMETER(uint32, CullingEnable)
	SHADER_PARAMETER(uint32, StableRasterization)
	SHADER_PARAMETER(uint32, ScatterSceneLighing)
	SHADER_PARAMETER(uint32, NumVerticesPerStrand)
	SHADER_PARAMETER_SRV(StructuredBuffer<float4>, PositionBuffer)
	SHADER_PARAMETER_SRV(StructuredBuffer<float4>, PreviousPositionBuffer)
	SHADER_PARAMETER_SRV(StructuredBuffer<float>, LengthBuffer)
	SHADER_PARAMETER_SRV(StructuredBuffer<float3>, TangentBuffer)
	SHADER_PARAMETER_SRV(StructuredBuffer<float2>, RootUVBuffer)
	SHADER_PARAMETER_SRV(StructuredBuffer<uint>, StrandsIDBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FTressFXVertexFactoryUniformShaderParameters> FTressFXUniformBuffer;



struct TRESSFXCORE_API FTressFXGroupInstance
{
	FTressFXDatas* TFXData = nullptr;

	bool IsValid() const { return TFXData != nullptr && TFXData->GetNumPoints() > 0; }

	struct FGuides
	{
		int32 Frame = 0;
		bool bIsSimulationEnable = false;
		bool bHasGlobalInterpolation = false;
		float GravityMagnitude = 10.f;
		float Damping = 0.035f;
		float GlobalShapeStiffness = 0.f;
		float GlobalShapeEffectiveRange = 0.f;
		bool EnableVelocityShockPropagation = true;
		bool EnableLengthConstraint = true;
		bool EnableLocalShapeConstraint = true;
		FVector3f VelocityShockPropagation;
		FVector3f WindDirection;
		float WindMagnitude = 0.f;
		uint32 LengthConstraintsIterations = 1;
		TArray<float> LocalShapeStiffness;
		TArray<float> VSPStiffness;
		int32 NumVerticesFromRootNoSimulation = 2;

		FTressFXGuidesRestResource*			GuidesRestResource = nullptr;
		FTressFXGuidesBindingResource*		GuidesBindingResource = nullptr;
		FTressFXGuidesDeformedResource*		GuidesDeformedResource = nullptr;
	} Guides;

	// Strands
	struct FStrands
	{
		FTressFXStrandsResource* StrandsResource;

		FTressFXUniformBuffer UniformBuffer;
		// Resources - Raytracing data when enabling (expensive) raytracing method
#if RHI_RAYTRACING
		//FTressFXRaytracingResource* RenRaytracingResource = nullptr;
#endif

		UMaterialInterface* Material = nullptr;
	} Strands;

	struct FDebug
	{
		// Data
		ETressFXDebugMode		DebugMode = ETressFXDebugMode::NoneDebug;
		uint32					ComponentId = ~0;
		uint32					GroupIndex = ~0;
		uint32					GroupCount = 0;
		FString					TressFXAssetName;
		uint32					LastFrameIndex = ~0;

		int32					MeshLODIndex = ~0;

		bool					bEnableVisualizeTangents = false;
	} Debug;

	FTressFXVertexFactoryUniformShaderParameters GetTressFXUniformShaderParameters() const;

	FString						WorldName;

	FPrimitiveSceneProxy*		PrimitiveSceneProxy = nullptr;
	USceneComponent*			ParentComponent = nullptr;
	USkeletalMeshComponent*		SkeletalComponent = nullptr;

	float						StrandsWidth = 0.0065f;
	uint32						NumFollowStrands = 0;
	float						TipSeparationFactor = 0.05f;
	float						MaxRadiusAroundGuide = 0.1f;
	float						TranslucencyDensity = 1.f;
	FTransform					LocalToWorld = FTransform::Identity;
	EWorldType::Type			WorldType;
	ETressFXGeometryType		GeometryType = ETressFXGeometryType::NoneGeometry_;
	FTressFXGroupPublicData*	TressFXGroupPublicData = nullptr;
	const FBoxSphereBounds*		ProxyBounds = nullptr;
	const FBoxSphereBounds*		ProxyLocalBounds = nullptr;
	bool						LODChanged = false;
	bool						bUseCPULODSelection = true;
	bool						bForceCards = false;
	bool						bUpdatePositionOffset = false;
	bool						bCastShadow = true;
	float						TimeStep = 0.0167f;
	float						AccTimeStep = 0.0f;
	uint32						LocalSDFIdRef = 0xFFFFFFFF;

#if WITH_EDITOR
	FTressFXMorphTargetMeshResource*		MTMeshResource = nullptr;
	FBufferRHIRef							MTMeshIndicesBufferRHI = nullptr;
	uint32									MTMeshIndicesCount = 0;
	uint32									MTMeshVertexCount = 0;
#endif
};