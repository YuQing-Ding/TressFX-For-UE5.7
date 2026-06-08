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

#include "PrimitiveSceneInfo.h"
#include "TressFXCoreRendering.h"
#include "TressFXInstance.h"
#include "TressFXMeshInstance.h"
#include "TressFXInterface.h"
#include "TressFXCommon.h"
#include "TressFXGeometryCache.h"
#include "ShaderCompilerCore.h"
#include "GPUSkinCache.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"


inline uint32 GetGroupSizePermutation(uint32 GroupSize);

static inline uint32 ComputeGroupSizeSim()
{
	const uint32 GroupSize = 64;

	return GroupSize;
}

FIntVector ComputeDispatchCount(uint32 ItemCount, uint32 GroupSize);




bool HasTressFXInstanceSimulationEnable(FTressFXGroupInstance* Instance, int32 MeshLODIndex)
{
	return Instance &&
		Instance->Guides.bIsSimulationEnable &&
		Instance->Guides.GuidesRestResource &&
		Instance->Guides.GuidesDeformedResource;
}

class FIntegrationAndGlobalShapeConstraintsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FIntegrationAndGlobalShapeConstraintsCS);
	SHADER_USE_PARAMETER_STRUCT(FIntegrationAndGlobalShapeConstraintsCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, ResetPositions)
		SHADER_PARAMETER(float, GravityMagnitude)
		SHADER_PARAMETER(float, DampingCoeff)
		SHADER_PARAMETER(float, EnableVSP)
		SHADER_PARAMETER(float, TimeStep)
		SHADER_PARAMETER(float, GlobalShapeStiffness)
		SHADER_PARAMETER(float, GlobalShapeEffectiveRange)
		SHADER_PARAMETER(uint32, DispatchCountX)
		SHADER_PARAMETER(uint32, NumOfStrandsPerThreadGroup)
		SHADER_PARAMETER(uint32, NumVerticesFromRootNoSimulation)
		SHADER_PARAMETER(uint32, PrimitiveId)

		SHADER_PARAMETER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, BoneMatricesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesRestPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBoneSkinningData>, GuidesBoneSkinningBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBoneIndexData>, GuidesBoneIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPrevPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPrevPrevPositionBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TRESSFX_MAX_NUM_BONES"), TRESSFX_MAX_NUM_BONES);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FIntegrationAndGlobalShapeConstraintsCS, "/Engine/Private/TressFX/TressFXSimulationIntegration.usf", "IntegrationAndGlobalShapeConstraints", SF_Compute);


void AddGuideIntegrationAndGlobalShapeConstraintsPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	TRefCountPtr<FRDGPooledBuffer>* GPUScenePrimitiveBuffer,
	const uint32 InPrimitiveId,
	const uint32 VertexCount,
	const float TimeStep,
	const FRDGImportedBufferTFX& OutGuidesDeformedPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPrevPositionBuffer,
	const uint32 NumVerticesPerStrand,
	const FRDGBufferSRVRef GuidesRestPositionBuffer,
	const FRDGBufferSRVRef GuidesBoneSkinningBuffer,
	const FRDGBufferSRVRef GuidesBoneIndexBuffer,
	const FRDGBufferSRVRef BoneMatricesBuffer,
	const FTressFXGroupInstance::FGuides& Guides)
{

	const uint32 GroupSize = ComputeGroupSizeSim();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(VertexCount, GroupSize);


	FIntegrationAndGlobalShapeConstraintsCS::FParameters* Parameters = GraphBuilder.AllocParameters<FIntegrationAndGlobalShapeConstraintsCS::FParameters>();

	Parameters->GPUScenePrimitiveSceneData = (*GPUScenePrimitiveBuffer)->GetSRV(); // Scene->GPUScene.PrimitiveBuffer.SRV;
	Parameters->PrimitiveId = InPrimitiveId;
	Parameters->GuidesRestPositionBuffer = GuidesRestPositionBuffer;
	Parameters->GuidesBoneSkinningBuffer = GuidesBoneSkinningBuffer;
	Parameters->GuidesBoneIndexBuffer = GuidesBoneIndexBuffer;
	Parameters->GuidesDeformedPositionBuffer = OutGuidesDeformedPositionBuffer.UAV;
	Parameters->GuidesDeformedPrevPositionBuffer = OutGuidesDeformedPrevPositionBuffer.UAV;
	Parameters->GuidesDeformedPrevPrevPositionBuffer = OutGuidesDeformedPrevPrevPositionBuffer.UAV;
	Parameters->ResetPositions = Guides.GuidesDeformedResource->bResetPositions;
	Parameters->GravityMagnitude = Guides.GravityMagnitude;
	Parameters->EnableVSP = true;
	Parameters->NumVerticesFromRootNoSimulation = Guides.NumVerticesFromRootNoSimulation;
	Parameters->DampingCoeff = Guides.Damping;
	Parameters->GlobalShapeStiffness = Guides.GlobalShapeStiffness;
	Parameters->GlobalShapeEffectiveRange = Guides.GlobalShapeEffectiveRange;
	Parameters->TimeStep = TimeStep > 0.016f ? 0.016f : TimeStep;
	Parameters->NumOfStrandsPerThreadGroup = TRESSFX_SIM_THREAD_GROUP_SIZE / NumVerticesPerStrand;
	Parameters->BoneMatricesBuffer = BoneMatricesBuffer;

	TShaderMapRef<FIntegrationAndGlobalShapeConstraintsCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("IntegrationAndGlobalShapeConstraints"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPositionBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPrevPositionBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPrevPrevPositionBuffer.Buffer, ERHIAccess::SRVMask);

}

class FIntegrationAndGlobalShapeConstraintsMTCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FIntegrationAndGlobalShapeConstraintsMTCS);
	SHADER_USE_PARAMETER_STRUCT(FIntegrationAndGlobalShapeConstraintsMTCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, ResetPositions)
		SHADER_PARAMETER(float, GravityMagnitude)
		SHADER_PARAMETER(float, DampingCoeff)
		SHADER_PARAMETER(float, TimeStep)
		SHADER_PARAMETER(float, EnableVSP)
		SHADER_PARAMETER(float, GlobalShapeStiffness)
		SHADER_PARAMETER(float, GlobalShapeEffectiveRange)
		SHADER_PARAMETER(uint32, NumOfStrandsPerThreadGroup)
		SHADER_PARAMETER(uint32, NumVerticesFromRootNoSimulation)
		SHADER_PARAMETER(FMatrix44f, CompToWorld)
		SHADER_PARAMETER(uint32, PrimitiveId)

		SHADER_PARAMETER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, BoneMatricesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesRestPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBoneSkinningData>, GuidesBoneSkinningBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBoneIndexData>, GuidesBoneIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPrevPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPrevPrevPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGuideRootTriangle>, GuidesRootBindingTrianglesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, GuidesLocalToGlobalVertexIndexMapBuffer)
		SHADER_PARAMETER_SRV(Buffer<float>, MeshPositionRestBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, MeshPositionDeformedBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TRESSFX_MAX_NUM_BONES"), TRESSFX_MAX_NUM_BONES);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FIntegrationAndGlobalShapeConstraintsMTCS, "/Engine/Private/TressFX/TressFXSimulationIntegration.usf", "IntegrationAndGlobalShapeConstraintsMT", SF_Compute);


void AddGuideIntegrationAndGlobalShapeConstraintsMTPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	TRefCountPtr<FRDGPooledBuffer>* GPUScenePrimitiveBuffer,
	const uint32 InPrimitiveId,
	const uint32 VertexCount,
	const float MaxRestLength,
	const float TimeStep,
	const FRDGImportedBufferTFX& OutGuidesDeformedPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPrevPositionBuffer,
	const uint32 NumVerticesPerStrand,
	const FRDGBufferSRVRef GuidesRestPositionBuffer,
	const FRDGBufferSRVRef GuidesBoneSkinningBuffer,
	const FRDGBufferSRVRef GuidesBoneIndexBuffer,
	const FRDGBufferSRVRef BoneMatricesBuffer,
	const FRDGBufferSRVRef GuidesRootBindingTrianglesBuffer,
	const FRDGBufferSRVRef GuidesLocalToGlobalVertexIndexMapBuffer,
	FRHIShaderResourceView* MeshPositionRestBuffer,
	FRDGBufferSRVRef MeshPositionDeformedBuffer,
	const FMatrix44f& CompToWorld,
	const FTressFXGroupInstance::FGuides& Guides)
{

	const uint32 GroupSize = ComputeGroupSizeSim();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(VertexCount, GroupSize);


	FIntegrationAndGlobalShapeConstraintsMTCS::FParameters* Parameters = GraphBuilder.AllocParameters<FIntegrationAndGlobalShapeConstraintsMTCS::FParameters>();

	Parameters->GPUScenePrimitiveSceneData = (*GPUScenePrimitiveBuffer)->GetSRV(); // Scene->GPUScene.PrimitiveBuffer.SRV;
	Parameters->PrimitiveId = InPrimitiveId;
	Parameters->GuidesRestPositionBuffer = GuidesRestPositionBuffer;
	Parameters->GuidesBoneSkinningBuffer = GuidesBoneSkinningBuffer;
	Parameters->GuidesBoneIndexBuffer = GuidesBoneIndexBuffer;
	Parameters->GuidesRootBindingTrianglesBuffer = GuidesRootBindingTrianglesBuffer;
	Parameters->GuidesLocalToGlobalVertexIndexMapBuffer = GuidesLocalToGlobalVertexIndexMapBuffer;
	Parameters->GuidesDeformedPositionBuffer = OutGuidesDeformedPositionBuffer.UAV;
	Parameters->GuidesDeformedPrevPositionBuffer = OutGuidesDeformedPrevPositionBuffer.UAV;
	Parameters->GuidesDeformedPrevPrevPositionBuffer = OutGuidesDeformedPrevPrevPositionBuffer.UAV;
	Parameters->ResetPositions = Guides.GuidesDeformedResource->bResetPositions;
	Parameters->GravityMagnitude = Guides.GravityMagnitude;
	Parameters->DampingCoeff = Guides.Damping;
	Parameters->EnableVSP = true;
	Parameters->NumVerticesFromRootNoSimulation = Guides.NumVerticesFromRootNoSimulation;
	Parameters->GlobalShapeStiffness = Guides.GlobalShapeStiffness;
	Parameters->GlobalShapeEffectiveRange = Guides.GlobalShapeEffectiveRange;
	Parameters->TimeStep = TimeStep > 0.016f ? 0.016f : TimeStep;
	Parameters->NumOfStrandsPerThreadGroup = TRESSFX_SIM_THREAD_GROUP_SIZE / NumVerticesPerStrand;
	Parameters->CompToWorld = CompToWorld;

	Parameters->BoneMatricesBuffer = BoneMatricesBuffer;
	Parameters->MeshPositionRestBuffer = MeshPositionRestBuffer;
	Parameters->MeshPositionDeformedBuffer = MeshPositionDeformedBuffer;

	TShaderMapRef<FIntegrationAndGlobalShapeConstraintsMTCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("IntegrationAndGlobalShapeConstraintsMT"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPositionBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPrevPositionBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPrevPrevPositionBuffer.Buffer, ERHIAccess::SRVMask);

}

class FCalculateStrandLevelDataCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateStrandLevelDataCS);
	SHADER_USE_PARAMETER_STRUCT(FCalculateStrandLevelDataCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumOfStrandsPerThreadGroup)
		SHADER_PARAMETER(FVector3f, VelocityShockPropagation)
		SHADER_PARAMETER(uint32, PrimitiveId)

		SHADER_PARAMETER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, BoneMatricesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBoneSkinningData>, GuidesBoneSkinningBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBoneIndexData>, GuidesBoneIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesDeformedPrevPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesDeformedPrevPrevPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FStrandLevelData>, GuidesDeformedStrandLevelDataBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TRESSFX_MAX_NUM_BONES"), TRESSFX_MAX_NUM_BONES);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FCalculateStrandLevelDataCS, "/Engine/Private/TressFX/TressFXSimulationStrandLevel.usf", "CalculateStrandLevelData", SF_Compute);

void AddGuideCalculateStrandLevelDataPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	TRefCountPtr<FRDGPooledBuffer>* GPUScenePrimitiveBuffer,
	const uint32 InPrimitiveId,
	const uint32 VertexCount,
	const FRDGImportedBufferTFX& OutGuidesDeformedPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPrevPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedCalculateStrandLevelDataBuffer,
	const uint32 NumVerticesPerStrand,
	const FRDGBufferSRVRef GuidesBoneSkinningBuffer,
	const FRDGBufferSRVRef GuidesBoneIndexBuffer,
	const FRDGBufferSRVRef BoneMatricesBuffer,
	const FTressFXGroupInstance::FGuides& Guides)
{
	const uint32 GroupSize = ComputeGroupSizeSim();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(VertexCount / NumVerticesPerStrand, GroupSize);


	FCalculateStrandLevelDataCS::FParameters* Parameters = GraphBuilder.AllocParameters<FCalculateStrandLevelDataCS::FParameters>();

	Parameters->GPUScenePrimitiveSceneData = (*GPUScenePrimitiveBuffer)->GetSRV(); // Scene->GPUScene.PrimitiveBuffer.SRV;
	Parameters->PrimitiveId = InPrimitiveId;
	Parameters->GuidesBoneSkinningBuffer = GuidesBoneSkinningBuffer;
	Parameters->GuidesBoneIndexBuffer = GuidesBoneIndexBuffer;
	Parameters->BoneMatricesBuffer = BoneMatricesBuffer;
	Parameters->GuidesDeformedPositionBuffer = OutGuidesDeformedPositionBuffer.SRV;
	Parameters->GuidesDeformedPrevPositionBuffer = OutGuidesDeformedPrevPositionBuffer.SRV;
	Parameters->GuidesDeformedPrevPrevPositionBuffer = OutGuidesDeformedPrevPrevPositionBuffer.SRV;
	Parameters->NumOfStrandsPerThreadGroup = TRESSFX_SIM_THREAD_GROUP_SIZE / NumVerticesPerStrand;
	Parameters->GuidesDeformedStrandLevelDataBuffer = OutGuidesDeformedCalculateStrandLevelDataBuffer.UAV;
	Parameters->VelocityShockPropagation = Guides.VelocityShockPropagation;

	TShaderMapRef<FCalculateStrandLevelDataCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("CalculateStrandLevelData"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedCalculateStrandLevelDataBuffer.Buffer, ERHIAccess::SRVMask);

}

class FCalculateStrandLevelDataMTCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCalculateStrandLevelDataMTCS);
	SHADER_USE_PARAMETER_STRUCT(FCalculateStrandLevelDataMTCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumOfStrandsPerThreadGroup)
		SHADER_PARAMETER(FVector3f, VelocityShockPropagation)
		SHADER_PARAMETER(FMatrix44f, CompToWorld)
		SHADER_PARAMETER(uint32, PrimitiveId)

		SHADER_PARAMETER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FGuideRootTriangle>, GuidesRootBindingTrianglesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, GuidesLocalToGlobalVertexIndexMapBuffer)
		SHADER_PARAMETER_SRV(Buffer<float>, MeshPositionRestBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float>, MeshPositionDeformedBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, BoneMatricesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBoneSkinningData>, GuidesBoneSkinningBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBoneIndexData>, GuidesBoneIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesDeformedPrevPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesDeformedPrevPrevPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FStrandLevelData>, GuidesDeformedStrandLevelDataBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TRESSFX_MAX_NUM_BONES"), TRESSFX_MAX_NUM_BONES);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FCalculateStrandLevelDataMTCS, "/Engine/Private/TressFX/TressFXSimulationStrandLevel.usf", "CalculateStrandLevelDataMT", SF_Compute);

void AddGuideCalculateStrandLevelDataMTPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	TRefCountPtr<FRDGPooledBuffer>* GPUScenePrimitiveBuffer,
	const uint32 InPrimitiveId,
	const uint32 VertexCount,
	const FRDGImportedBufferTFX& OutGuidesDeformedPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPrevPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedCalculateStrandLevelDataBuffer,
	const uint32 NumVerticesPerStrand,
	const FRDGBufferSRVRef GuidesBoneSkinningBuffer,
	const FRDGBufferSRVRef GuidesBoneIndexBuffer,
	const FRDGBufferSRVRef BoneMatricesBuffer,
	const FRDGBufferSRVRef GuidesRootBindingTrianglesBuffer,
	const FRDGBufferSRVRef GuidesLocalToGlobalVertexIndexMapBuffer,
	FRHIShaderResourceView* MeshPositionRestBuffer,
	FRDGBufferSRVRef MeshPositionDeformedBuffer,
	const FMatrix44f& CompToWorld,
	const FTressFXGroupInstance::FGuides& Guides)
{
	const uint32 GroupSize = ComputeGroupSizeSim();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(VertexCount / NumVerticesPerStrand, GroupSize);


	FCalculateStrandLevelDataMTCS::FParameters* Parameters = GraphBuilder.AllocParameters<FCalculateStrandLevelDataMTCS::FParameters>();

	Parameters->GPUScenePrimitiveSceneData = (*GPUScenePrimitiveBuffer)->GetSRV(); // Scene->GPUScene.PrimitiveBuffer.SRV;
	Parameters->PrimitiveId = InPrimitiveId;
	Parameters->GuidesBoneSkinningBuffer = GuidesBoneSkinningBuffer;
	Parameters->GuidesBoneIndexBuffer = GuidesBoneIndexBuffer;
	Parameters->BoneMatricesBuffer = BoneMatricesBuffer;
	Parameters->GuidesDeformedPositionBuffer = OutGuidesDeformedPositionBuffer.SRV;
	Parameters->GuidesDeformedPrevPositionBuffer = OutGuidesDeformedPrevPositionBuffer.SRV;
	Parameters->GuidesDeformedPrevPrevPositionBuffer = OutGuidesDeformedPrevPrevPositionBuffer.SRV;
	Parameters->NumOfStrandsPerThreadGroup = TRESSFX_SIM_THREAD_GROUP_SIZE / NumVerticesPerStrand;
	Parameters->GuidesDeformedStrandLevelDataBuffer = OutGuidesDeformedCalculateStrandLevelDataBuffer.UAV;
	Parameters->VelocityShockPropagation = Guides.VelocityShockPropagation;
	Parameters->GuidesRootBindingTrianglesBuffer = GuidesRootBindingTrianglesBuffer;
	Parameters->GuidesLocalToGlobalVertexIndexMapBuffer = GuidesLocalToGlobalVertexIndexMapBuffer;
	Parameters->MeshPositionRestBuffer = MeshPositionRestBuffer;
	Parameters->MeshPositionDeformedBuffer = MeshPositionDeformedBuffer;
	Parameters->CompToWorld = CompToWorld;

	TShaderMapRef<FCalculateStrandLevelDataMTCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("CalculateStrandLevelDataMT"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedCalculateStrandLevelDataBuffer.Buffer, ERHIAccess::SRVMask);

}

class FVelocityShockPropagationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVelocityShockPropagationCS);
	SHADER_USE_PARAMETER_STRUCT(FVelocityShockPropagationCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumOfStrandsPerThreadGroup)
		SHADER_PARAMETER(int32, NumVerticesFromRootNoSimulation)
		SHADER_PARAMETER_SCALAR_ARRAY(float, VSPStiffness, [64])

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPrevPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FStrandLevelData>, GuidesDeformedStrandLevelDataBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TRESSFX_MAX_NUM_BONES"), TRESSFX_MAX_NUM_BONES);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVelocityShockPropagationCS, "/Engine/Private/TressFX/TressFXSimulationVSP.usf", "VelocityShockPropagation", SF_Compute);

void AddGuideVelocityShockPropagationPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	const uint32 VertexCount,
	const FRDGImportedBufferTFX& OutGuidesDeformedPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedCalculateStrandLevelDataBuffer,
	const uint32 NumVerticesPerStrand,
	const FTressFXGroupInstance::FGuides& Guides)
{
	const uint32 GroupSize = ComputeGroupSizeSim();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(VertexCount, GroupSize);


	FVelocityShockPropagationCS::FParameters* Parameters = GraphBuilder.AllocParameters<FVelocityShockPropagationCS::FParameters>();
	Parameters->GuidesDeformedPositionBuffer = OutGuidesDeformedPositionBuffer.UAV;
	Parameters->GuidesDeformedPrevPositionBuffer = OutGuidesDeformedPrevPositionBuffer.UAV;
	Parameters->NumOfStrandsPerThreadGroup = TRESSFX_SIM_THREAD_GROUP_SIZE / NumVerticesPerStrand;
	Parameters->GuidesDeformedStrandLevelDataBuffer = OutGuidesDeformedCalculateStrandLevelDataBuffer.SRV;

	Parameters->NumVerticesFromRootNoSimulation = Guides.NumVerticesFromRootNoSimulation;
	for (uint32 i = 0; i < NumVerticesPerStrand; ++i)
	{
		GET_SCALAR_ARRAY_ELEMENT(Parameters->VSPStiffness, i) = Guides.VSPStiffness[i];
	}

	TShaderMapRef<FVelocityShockPropagationCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("VelocityShockPropagation"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPositionBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPrevPositionBuffer.Buffer, ERHIAccess::SRVMask);

}

class FLocalShapeConstraintsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalShapeConstraintsCS);
	SHADER_USE_PARAMETER_STRUCT(FLocalShapeConstraintsCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumOfStrandsPerThreadGroup)
		SHADER_PARAMETER_SCALAR_ARRAY(float, Stiffness, [32])
		SHADER_PARAMETER(uint32, NumVerticesFromRootNoSimulation)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesRestPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FStrandLevelData>, GuidesDeformedStrandLevelDataBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TRESSFX_MAX_NUM_BONES"), TRESSFX_MAX_NUM_BONES);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalShapeConstraintsCS, "/Engine/Private/TressFX/TressFXSimulationLocalShapeConstraints.usf", "LocalShapeConstraints", SF_Compute);

void AddGuideLocalShapeConstraintsPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	const uint32 VertexCount,
	const FRDGImportedBufferTFX& OutGuidesDeformedPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedCalculateStrandLevelDataBuffer,
	const uint32 NumVerticesPerStrand,
	const FRDGBufferSRVRef GuidesRestPositionBuffer,
	const FTressFXGroupInstance::FGuides& Guides)
{
	const uint32 GroupSize = ComputeGroupSizeSim();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(VertexCount / NumVerticesPerStrand, GroupSize);


	FLocalShapeConstraintsCS::FParameters* Parameters = GraphBuilder.AllocParameters<FLocalShapeConstraintsCS::FParameters>();
	Parameters->GuidesDeformedPositionBuffer = OutGuidesDeformedPositionBuffer.UAV;
	Parameters->GuidesRestPositionBuffer = GuidesRestPositionBuffer;
	Parameters->NumOfStrandsPerThreadGroup = TRESSFX_SIM_THREAD_GROUP_SIZE / NumVerticesPerStrand;
	Parameters->GuidesDeformedStrandLevelDataBuffer = OutGuidesDeformedCalculateStrandLevelDataBuffer.SRV;
	Parameters->NumVerticesFromRootNoSimulation = Guides.NumVerticesFromRootNoSimulation;

	for (uint32 i = 0; i < NumVerticesPerStrand; ++i)
	{
		GET_SCALAR_ARRAY_ELEMENT(Parameters->Stiffness, i) = Guides.LocalShapeStiffness[i];
	}


	TShaderMapRef<FLocalShapeConstraintsCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("LocalShapeConstraints"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPositionBuffer.Buffer, ERHIAccess::SRVMask);

}

class FLengthConstraintsWindAndCollisionCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLengthConstraintsWindAndCollisionCS);
	SHADER_USE_PARAMETER_STRUCT(FLengthConstraintsWindAndCollisionCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumOfStrandsPerThreadGroup)
		SHADER_PARAMETER(uint32, LengthConstraintsIterations)
		SHADER_PARAMETER(int32, NumVerticesFromRootNoSimulation)
		SHADER_PARAMETER(float, TimeStep)
		SHADER_PARAMETER(FVector4f, Wind0)
		SHADER_PARAMETER(FVector4f, Wind1)
		SHADER_PARAMETER(FVector4f, Wind2)
		SHADER_PARAMETER(FVector4f, Wind3)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, GuidesRestLengthBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPrevPositionBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TRESSFX_MAX_NUM_BONES"), TRESSFX_MAX_NUM_BONES);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLengthConstraintsWindAndCollisionCS, "/Engine/Private/TressFX/TressFXSimulationLengthConstraintsWind.usf", "LengthConstraintsWindAndCollision", SF_Compute);

static void SetWindCorner(const FQuat4f& rotFromXAxisToWindDir, const FVector4f& rotAxis,
	float angleToWideWindCone, float wM, FVector4f& outVec)
{
	static const FVector4f XAxis(1.0f, 0, 0);
	FQuat4f rot(rotAxis, angleToWideWindCone);
	FVector4f newWindDir = rotFromXAxisToWindDir * rot * XAxis;
	outVec.X = newWindDir.X * wM;
	outVec.Y = newWindDir.Y * wM;
	outVec.Z = newWindDir.Z * wM;
	outVec.W = 0;  // unused.
}

const float MATH_PI2 = 3.14159265359f;
#define DEG_TO_RAD2(d) (d * MATH_PI2 / 180)


static void SetWind(const FVector4f& windDir, float windMag, int32 frame,
	FVector4f& Wind0, FVector4f& Wind1, FVector4f& Wind2, FVector4f& Wind3)
{

	float wM = windMag * (pow(sin(frame * 0.001f), 2.0f) + 0.5f);

	FVector4f windDirN = windDir.GetUnsafeNormal3();

	FVector4f XAxis(1.0f, 0, 0);
	FVector4f xCrossW = FVector3f::CrossProduct(XAxis, windDirN);

	FQuat4f rotFromXAxisToWindDir;
	rotFromXAxisToWindDir = FQuat4f::Identity;

	float angle = asin(xCrossW.Size3());

	if (angle > 0.001)
		rotFromXAxisToWindDir = FQuat4f(xCrossW.GetUnsafeNormal3(), angle);

	float angleToWideWindCone = DEG_TO_RAD2(40.f);

	FVector4f rotAxis = FVector4f(0, 1.0, 0);

	SetWindCorner(rotFromXAxisToWindDir,
		rotAxis,
		angleToWideWindCone,
		wM,
		Wind0);

	rotAxis.Y = -1.0f;
	SetWindCorner(rotFromXAxisToWindDir,
		rotAxis,
		angleToWideWindCone,
		wM,
		Wind1);

	rotAxis.Y = 0;
	rotAxis.Z = 1.0f;
	SetWindCorner(rotFromXAxisToWindDir,
		rotAxis,
		angleToWideWindCone,
		wM,
		Wind2);

	rotAxis.Z = -1.0f;
	SetWindCorner(rotFromXAxisToWindDir,
		rotAxis,
		angleToWideWindCone,
		wM,
		Wind3);

	// fourth component unused. (used to store frame number, but no longer used).
}

void AddGuideLengthConstraintsWindAndCollisionPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	const uint32 VertexCount,
	const FRDGBufferSRVRef& GuidesRestLengthBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedCalculateStrandLevelDataBuffer,
	const uint32 NumVerticesPerStrand,
	const float TimeStep,
	const FTressFXGroupInstance::FGuides& Guides)
{
	const uint32 GroupSize = ComputeGroupSizeSim();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(VertexCount, GroupSize);


	FLengthConstraintsWindAndCollisionCS::FParameters* Parameters = GraphBuilder.AllocParameters<FLengthConstraintsWindAndCollisionCS::FParameters>();
	Parameters->GuidesRestLengthBuffer = GuidesRestLengthBuffer;
	Parameters->GuidesDeformedPositionBuffer = OutGuidesDeformedPositionBuffer.UAV;
	Parameters->GuidesDeformedPrevPositionBuffer = OutGuidesDeformedPrevPositionBuffer.UAV;
	Parameters->NumOfStrandsPerThreadGroup = TRESSFX_SIM_THREAD_GROUP_SIZE / NumVerticesPerStrand;
	Parameters->LengthConstraintsIterations = Guides.LengthConstraintsIterations;
	Parameters->NumVerticesFromRootNoSimulation = Guides.NumVerticesFromRootNoSimulation;
	Parameters->TimeStep = TimeStep;
	
	FVector4f Wind[4];
	SetWind(Guides.WindDirection, Guides.WindMagnitude * (0.3f + FMath::FRand()), Guides.Frame,
		Wind[0], Wind[1], Wind[2], Wind[3]);
	Parameters->Wind0 = Wind[0];
	Parameters->Wind1 = Wind[1];
	Parameters->Wind2 = Wind[2];
	Parameters->Wind3 = Wind[3];

	TShaderMapRef<FLengthConstraintsWindAndCollisionCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("LengthConstraintsWindAndCollision"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPositionBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPrevPositionBuffer.Buffer, ERHIAccess::SRVMask);

}

class FCollideStrandsVerticesWithSDFCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCollideStrandsVerticesWithSDFCS);
	SHADER_USE_PARAMETER_STRUCT(FCollideStrandsVerticesWithSDFCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int32, NumTotalVertices)
		SHADER_PARAMETER(int32, NumVerticesPerStrand)
		SHADER_PARAMETER(int32, NumVerticesFromRootBeforeSDFCollision)
		SHADER_PARAMETER(FIntVector, NumSDFCells)
		SHADER_PARAMETER(float, CollisionMargin)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPrevPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FCollisionMeshBox>, CMBoxR)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, SDFBufferR)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TRESSFX_MAX_NUM_BONES"), TRESSFX_MAX_NUM_BONES);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FCollideStrandsVerticesWithSDFCS, "/Engine/Private/TressFX/TressFXSimulationCollideStrandsVerticesWithSDF.usf", "CollideStrandsVerticesWithSDF_Forward", SF_Compute);

void AddCollideStrandsVerticesWithSDFPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	const uint32 VertexCount,
	const FRDGImportedBufferTFX& OutGuidesDeformedPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPrevPositionBuffer,
	const uint32 NumVerticesPerStrand,
	const FTressFXGroupInstance::FGuides& Guides,
	const FTressFXMeshGroupInstance* MeshInstance)
{
	const uint32 GroupSize = ComputeGroupSizeSim();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(VertexCount, GroupSize);

	FRDGBufferSRVRef CMBoxBuffer = RegisterAsSRV(GraphBuilder, MeshInstance->MeshDeformedResource->CollisionMeshBoxBuffer);
	FRDGBufferSRVRef SDFBuffer = RegisterAsSRV(GraphBuilder, MeshInstance->MeshDeformedResource->SDFBuffer);

	FCollideStrandsVerticesWithSDFCS::FParameters* Parameters = GraphBuilder.AllocParameters<FCollideStrandsVerticesWithSDFCS::FParameters>();
	Parameters->GuidesDeformedPositionBuffer = OutGuidesDeformedPositionBuffer.UAV;
	Parameters->GuidesDeformedPrevPositionBuffer = OutGuidesDeformedPrevPositionBuffer.UAV;
	Parameters->CMBoxR = CMBoxBuffer;
	Parameters->SDFBufferR = SDFBuffer;
	Parameters->NumSDFCells = MeshInstance->NumSDFCells;
	Parameters->NumVerticesPerStrand = NumVerticesPerStrand;
	Parameters->NumTotalVertices = VertexCount;
	Parameters->CollisionMargin = MeshInstance->SDFCollisionMargin;
	Parameters->NumVerticesFromRootBeforeSDFCollision = Guides.NumVerticesFromRootNoSimulation;

	TShaderMapRef<FCollideStrandsVerticesWithSDFCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("CollideStrandsVerticesWithSDF"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPositionBuffer.Buffer, ERHIAccess::SRVMask);

}

class FGuidesRestToDeformedCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGuidesRestToDeformedCS);
	SHADER_USE_PARAMETER_STRUCT(FGuidesRestToDeformedCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(uint32, NumOfStrandsPerThreadGroup)
		SHADER_PARAMETER(FMatrix44f, ComponentToWorld)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesRestPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, GuidesDeformedPositionBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGuidesRestToDeformedCS, "/Engine/Private/TressFX/TressFXGuidesRestToDeformed.usf", "GuidesRestToDeformed", SF_Compute);

void AddGuidesRestToDeformedPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	const uint32 VertexCount,
	const uint32 NumVerticesPerStrand,
	const FRDGBufferSRVRef& GuidesRestPositionBuffer,
	const FRDGImportedBufferTFX& OutGuidesDeformedPositionBuffer,
	const FMatrix44f& ComponentToWorld)
{
	const uint32 GroupSize = ComputeGroupSizeSim();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(VertexCount, GroupSize);

	FGuidesRestToDeformedCS::FParameters* Parameters = GraphBuilder.AllocParameters<FGuidesRestToDeformedCS::FParameters>();
	Parameters->GuidesDeformedPositionBuffer = OutGuidesDeformedPositionBuffer.UAV;
	Parameters->GuidesRestPositionBuffer = GuidesRestPositionBuffer;
	Parameters->ComponentToWorld = ComponentToWorld;
	Parameters->NumOfStrandsPerThreadGroup = TRESSFX_SIM_THREAD_GROUP_SIZE / NumVerticesPerStrand;

	TShaderMapRef<FGuidesRestToDeformedCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GuidesRestToDeformed"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutGuidesDeformedPositionBuffer.Buffer, ERHIAccess::SRVMask);

}

void ComputeTressFXSimulation(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	TRefCountPtr<FRDGPooledBuffer>* GPUScenePrimitiveBuffer,
	const FGPUSkinCache* SkinCache,
	FTressFXGroupInstance* Instance,
	int32 MeshLODIndex,
	const TArray<FTressFXMeshGroupInstance*>& MeshInstances)
{
	++Instance->Guides.Frame;

	float TimeStep = Instance->TimeStep;
	/*if (TimeStep >= 0.0167f)
		Instance->AccTimeStep += TimeStep;
	else
		Instance->AccTimeStep = 0.0167f;*/

	Instance->TimeStep = 0.0125f;

	FRDGBufferSRVRef GuidesRestPositionBuffer = RegisterAsSRV(GraphBuilder, Instance->Guides.GuidesRestResource->GuidesRestPositionBuffer);
	FRDGImportedBufferTFX GuidesDeformedPositionBuffer = Register(GraphBuilder,
		Instance->Guides.GuidesDeformedResource->GetBuffer(FTressFXGuidesDeformedResource::Current),
		ERDGImportedBufferFlagsTFX::CreateViews);

	const FMatrix ComponentToWorldTransform44d = Instance->LocalToWorld.ToMatrixWithScale();

	FMatrix44f ComponentToWorldTransform;
	for (int32 i = 0; i < 4; ++i)
		for (int32 j = 0; j < 4; ++j)
			ComponentToWorldTransform.M[i][j] = ComponentToWorldTransform44d.M[i][j];


	if (!HasTressFXInstanceSimulationEnable(Instance, MeshLODIndex))
	{

		AddGuidesRestToDeformedPass(
			GraphBuilder,
			View,
			ShaderMap,
			Instance->Guides.GuidesRestResource->GetVertexCount(),
			Instance->TFXData->NumVerticesPerStrand,
			GuidesRestPositionBuffer,
			GuidesDeformedPositionBuffer,
			ComponentToWorldTransform
		);

		return;
	}

	FRDGBufferSRVRef GuidesBoneSkinningBuffer = RegisterAsSRV(GraphBuilder, Instance->Guides.GuidesRestResource->GuidesBoneSkinningBuffer);
	FRDGBufferSRVRef GuidesBoneIndexBuffer = RegisterAsSRV(GraphBuilder, Instance->Guides.GuidesRestResource->GuidesBoneIndexBuffer);

	TArray<FMatrix44f> BoneMatrices;
	if (Instance->SkeletalComponent && Instance->SkeletalComponent->MeshObject && Instance->SkeletalComponent->MeshObject->HaveValidDynamicData())
	{
		TConstArrayView<FMatrix44f> ReferenceToLocalMatrices = Instance->SkeletalComponent->MeshObject->GetReferenceToLocalMatrices();
		BoneMatrices.Append(ReferenceToLocalMatrices.GetData(), ReferenceToLocalMatrices.Num());
	}
	else
	{
		BoneMatrices.Add(FMatrix44f::Identity);
	}

	FRDGBufferSRVRef BoneMatricesBuffer = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("TressFXSkinnedBoneMatrices"), sizeof(float) * 4, BoneMatrices.Num() * 4, BoneMatrices.GetData(), sizeof(float) * 16 * BoneMatrices.Num()));

#if WITH_EDITOR

	FRDGImportedBufferTFX GuidesDeformedPositionBufferDebug = Register(GraphBuilder,
		Instance->Guides.GuidesDeformedResource->GuidesDeformedPositionBufferDebug,
		ERDGImportedBufferFlagsTFX::CreateViews);

#endif

	FRDGImportedBufferTFX GuidesDeformedPrevPositionBuffer = Register(GraphBuilder,
		Instance->Guides.GuidesDeformedResource->GetBuffer(FTressFXGuidesDeformedResource::Previous),
		ERDGImportedBufferFlagsTFX::CreateViews);

	FRDGImportedBufferTFX GuidesDeformedPrevPrevPositionBuffer = Register(GraphBuilder,
		Instance->Guides.GuidesDeformedResource->GetBuffer(FTressFXGuidesDeformedResource::PreviousPrevious),
		ERDGImportedBufferFlagsTFX::CreateViews);

	FRDGImportedBufferTFX GuidesDeformedStrandLevelDataBuffer = Register(GraphBuilder,
		Instance->Guides.GuidesDeformedResource->GuidesDeformedStrandLevelDataBuffer,
		ERDGImportedBufferFlagsTFX::CreateViews);

	FRDGBufferSRVRef GuidesRestLengthBuffer = RegisterAsSRV(GraphBuilder, Instance->Guides.GuidesRestResource->GuidesRestLengthBuffer);

	int32 NumLoops = 0;
	while (TimeStep > 0.f && NumLoops < 2)
	{
		if (nullptr == Instance->Guides.GuidesBindingResource)
		{
			int32 PrimitiveId = Instance->PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetPersistentIndex().Index;

			AddGuideIntegrationAndGlobalShapeConstraintsPass(
				GraphBuilder,
				View,
				ShaderMap,
				GPUScenePrimitiveBuffer,
				PrimitiveId,
				Instance->Guides.GuidesRestResource->GetVertexCount(),
				Instance->TimeStep,
				GuidesDeformedPositionBuffer,
				GuidesDeformedPrevPositionBuffer,
				GuidesDeformedPrevPrevPositionBuffer,
				Instance->TFXData->NumVerticesPerStrand,
				GuidesRestPositionBuffer,
				GuidesBoneSkinningBuffer,
				GuidesBoneIndexBuffer,
				BoneMatricesBuffer,
				Instance->Guides);

			AddGuideCalculateStrandLevelDataPass(
				GraphBuilder,
				View,
				ShaderMap,
				GPUScenePrimitiveBuffer,
				PrimitiveId,
				Instance->Guides.GuidesRestResource->GetVertexCount(),
				GuidesDeformedPositionBuffer,
				GuidesDeformedPrevPositionBuffer,
				GuidesDeformedPrevPrevPositionBuffer,
				GuidesDeformedStrandLevelDataBuffer,
				Instance->TFXData->NumVerticesPerStrand,
				GuidesBoneSkinningBuffer,
				GuidesBoneIndexBuffer,
				BoneMatricesBuffer,
				Instance->Guides);

		}
		else
		{
			FRDGBufferSRVRef GuidesRootBindingTrianglesBuffer = RegisterAsSRV(GraphBuilder, Instance->Guides.GuidesBindingResource->GuidesRootBindingBuffer);

			FRDGBufferSRVRef GuidesLocalToGlobalVertexIndexMapBuffer = RegisterAsSRV(GraphBuilder, Instance->Guides.GuidesBindingResource->GuidesLocalToGlobalVertexIndexMapBuffer);

			FCachedGeometry CachedGeometry;
			/* // Deprecated Skin Cache, you can re-implement this part if your project enabled Skin Cache
			if (SkinCache)
			{
				CachedGeometry = SkinCache->GetCachedGeometry(Instance->SkeletalComponent->ComponentId.PrimIDValue);
			}

			if (CachedGeometry.Sections.Num() == 0)*/
			{
				BuildCacheGeometry(GraphBuilder, View, ShaderMap, Instance, CachedGeometry, GuidesLocalToGlobalVertexIndexMapBuffer);

				if (CachedGeometry.Sections.Num() == 0)
					return;
			}

			const FMatrix CompToWorld44d = Instance->SkeletalComponent->GetComponentToWorld().ToMatrixWithScale();
			FMatrix44f CompToWorld;
			for (int32 i = 0; i < 4; ++i)
				for (int32 j = 0; j < 4; ++j)
					CompToWorld.M[i][j] = CompToWorld44d.M[i][j];

			FSkeletalMeshRenderData* RenderData = Instance->SkeletalComponent->GetSkinnedAsset()->GetResourceForRendering();
			const uint32 LODIndex = Instance->SkeletalComponent->GetPredictedLODLevel();// RenderData->PendingFirstLODIdx;
			FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];

			int32 PrimitiveId = Instance->PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetPersistentIndex().Index;

			AddGuideIntegrationAndGlobalShapeConstraintsMTPass(
				GraphBuilder,
				View,
				ShaderMap,
				GPUScenePrimitiveBuffer,
				PrimitiveId,
				Instance->Guides.GuidesRestResource->GetVertexCount(),
				Instance->TFXData->MaxRestLength,
				Instance->TimeStep,
				GuidesDeformedPositionBuffer,
				GuidesDeformedPrevPositionBuffer,
				GuidesDeformedPrevPrevPositionBuffer,
				Instance->TFXData->NumVerticesPerStrand,
				GuidesRestPositionBuffer,
				GuidesBoneSkinningBuffer,
				GuidesBoneIndexBuffer,
				BoneMatricesBuffer,
				GuidesRootBindingTrianglesBuffer,
				GuidesLocalToGlobalVertexIndexMapBuffer,
				LODData.StaticVertexBuffers.PositionVertexBuffer.GetSRV(),
				CachedGeometry.Sections[0].RDGPositionBuffer,
				CompToWorld,
				Instance->Guides);

			AddGuideCalculateStrandLevelDataMTPass(
				GraphBuilder,
				View,
				ShaderMap,
				GPUScenePrimitiveBuffer,
				PrimitiveId,
				Instance->Guides.GuidesRestResource->GetVertexCount(),
				GuidesDeformedPositionBuffer,
				GuidesDeformedPrevPositionBuffer,
				GuidesDeformedPrevPrevPositionBuffer,
				GuidesDeformedStrandLevelDataBuffer,
				Instance->TFXData->NumVerticesPerStrand,
				GuidesBoneSkinningBuffer,
				GuidesBoneIndexBuffer,
				BoneMatricesBuffer,
				GuidesRootBindingTrianglesBuffer,
				GuidesLocalToGlobalVertexIndexMapBuffer,
				LODData.StaticVertexBuffers.PositionVertexBuffer.GetSRV(),
				CachedGeometry.Sections[0].RDGPositionBuffer,
				CompToWorld,
				Instance->Guides);
		}

		Instance->Guides.GuidesDeformedResource->bResetPositions = false;

		if (Instance->Guides.EnableVelocityShockPropagation)
		{
			AddGuideVelocityShockPropagationPass(
				GraphBuilder,
				View,
				ShaderMap,
				Instance->Guides.GuidesRestResource->GetVertexCount(),
				GuidesDeformedPositionBuffer,
				GuidesDeformedPrevPositionBuffer,
				GuidesDeformedStrandLevelDataBuffer,
				Instance->TFXData->NumVerticesPerStrand,
				Instance->Guides);
		}

		if (Instance->Guides.EnableLocalShapeConstraint)
		{
			AddGuideLocalShapeConstraintsPass(
				GraphBuilder,
				View,
				ShaderMap,
				Instance->Guides.GuidesRestResource->GetVertexCount(),
				GuidesDeformedPositionBuffer,
				GuidesDeformedStrandLevelDataBuffer,
				Instance->TFXData->NumVerticesPerStrand,
				GuidesRestPositionBuffer,
				Instance->Guides);
		}

		if (Instance->Guides.EnableLengthConstraint)
		{
			AddGuideLengthConstraintsWindAndCollisionPass(
				GraphBuilder,
				View,
				ShaderMap,
				Instance->Guides.GuidesRestResource->GetVertexCount(),
				GuidesRestLengthBuffer,
				GuidesDeformedPositionBuffer,
				GuidesDeformedPrevPositionBuffer,
				GuidesDeformedPrevPrevPositionBuffer,
				Instance->TFXData->NumVerticesPerStrand,
				Instance->TimeStep,
				Instance->Guides);
		}

		{
			// Find the Corresponding MeshInstance
			FTressFXMeshGroupInstance* MeshInstance = nullptr;

			if (Instance->LocalSDFIdRef != 0xFFFFFFFF)
			{
				for (int32 i = 0; i < MeshInstances.Num(); ++i)
				{
					if (Instance->SkeletalComponent
						&& Instance->WorldType == MeshInstances[i]->WorldType
						&& Instance->SkeletalComponent == MeshInstances[i]->Debug.SkeletalComponent)
					{
						if (Instance->LocalSDFIdRef == MeshInstances[i]->LocalSDFId)
						{
							MeshInstance = MeshInstances[i];
							break;
						}
					}
				}
			}

			if (MeshInstance)
			{
				AddCollideStrandsVerticesWithSDFPass(
					GraphBuilder,
					View,
					ShaderMap,
					Instance->Guides.GuidesRestResource->GetVertexCount(),
					GuidesDeformedPositionBuffer,
					GuidesDeformedPrevPositionBuffer,
					Instance->TFXData->NumVerticesPerStrand,
					Instance->Guides,
					MeshInstance);
			}
		}

		TimeStep -= 0.0167f;
		++NumLoops;
	}
}
