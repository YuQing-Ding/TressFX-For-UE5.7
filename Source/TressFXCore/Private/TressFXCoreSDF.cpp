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
#include "TressFXMeshInstance.h"
#include "TressFXInterface.h"
#include "TressFXCommon.h"
#include "ShaderCompilerCore.h"
#include "SkeletalRenderPublic.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"


inline uint32 GetGroupSizePermutation(uint32 GroupSize);


static inline uint32 ComputeGroupSizeSDF()
{
	const uint32 GroupSize = 64;

	return GroupSize;
}

FIntVector ComputeDispatchCount(uint32 ItemCount, uint32 GroupSize);

class FSDFMeshBoxInitCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSDFMeshBoxInitCS);
	SHADER_USE_PARAMETER_STRUCT(FSDFMeshBoxInitCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FIntermediateCollisionMeshBox>, ICMBoxRW)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSDFMeshBoxInitCS, "/Engine/Private/TressFX/TressFXSimulationSDFMeshBox.usf", "SDFMeshBoxInit", SF_Compute);

void AddSDFMeshBoxInitPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	FTressFXMeshGroupInstance* Instance,
	const FRDGImportedBufferTFX& ICMBoxBuffer)
{

	FSDFMeshBoxInitCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSDFMeshBoxInitCS::FParameters>();

	Parameters->ICMBoxRW = ICMBoxBuffer.UAV;

	TShaderMapRef<FSDFMeshBoxInitCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SDFMeshBoxInit"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(1, 1, 1));

	GraphBuilder.SetBufferAccessFinal(ICMBoxBuffer.Buffer, ERHIAccess::SRVMask);

}

class FSDFMeshSkinningCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSDFMeshSkinningCS);
	SHADER_USE_PARAMETER_STRUCT(FSDFMeshSkinningCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(uint32, NumMeshVertices)
		SHADER_PARAMETER(uint32, PrimitiveId)

		SHADER_PARAMETER_SRV(StructuredBuffer<float4>, GPUScenePrimitiveSceneData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, BoneMatricesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTressFXMeshVertexData>, MeshRestVertexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBoneSkinningData>, BoneSkinningBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FBoneIndexData>, BoneIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FTressFXMeshVertexData>, MeshDeformedVertexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FIntermediateCollisionMeshBox>, ICMBox)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("TRESSFX_MAX_NUM_BONES"), TRESSFX_MAX_NUM_BONES);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		OutEnvironment.SetDefine(TEXT("USE_GLOBAL_GPU_SCENE_DATA"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSDFMeshSkinningCS, "/Engine/Private/TressFX/TressFXSimulationSDFMeshSkinning.usf", "SDFMeshSkinning", SF_Compute);

class FSDFMeshSkinningNoBoneCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSDFMeshSkinningNoBoneCS);
	SHADER_USE_PARAMETER_STRUCT(FSDFMeshSkinningNoBoneCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(uint32, NumMeshVertices)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTressFXMeshVertexData>, MeshRestVertexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FIntermediateCollisionMeshBox>, ICMBox)

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

IMPLEMENT_GLOBAL_SHADER(FSDFMeshSkinningNoBoneCS, "/Engine/Private/TressFX/TressFXSimulationSDFMeshSkinning.usf", "SDFMeshSkinningNoBone", SF_Compute);

void AddSDFMeshSkinningPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	TRefCountPtr<FRDGPooledBuffer>* GPUScenePrimitiveBuffer,
	FTressFXMeshGroupInstance* Instance,
	uint32 NumMeshVertices,
	const FRDGImportedBufferTFX& MeshDeformedVertexBuffer,
	const FRDGImportedBufferTFX& ICMBoxBuffer)
{

	if (Instance->Debug.SkeletalComponent)
	{

		if (!Instance->Debug.SkeletalComponent->MeshObject ||
			!Instance->Debug.SkeletalComponent->MeshObject->HaveValidDynamicData())
		{
			return;
		}

		TConstArrayView<FMatrix44f> ReferenceToLocalMatrices = Instance->Debug.SkeletalComponent->MeshObject->GetReferenceToLocalMatrices();
		TArray<FMatrix44f> BoneMatrices;
		BoneMatrices.Append(ReferenceToLocalMatrices.GetData(), ReferenceToLocalMatrices.Num());

		FRDGBufferSRVRef BoneMatricesBuffer = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("TressFXSkinnedBoneMatrices"), sizeof(float) * 4, BoneMatrices.Num() * 4, BoneMatrices.GetData(), sizeof(float) * 16 * BoneMatrices.Num()));
		const uint32 GroupSize = ComputeGroupSizeSDF();

		const uint32 DispatchCountX = FMath::DivideAndRoundUp(NumMeshVertices, GroupSize);


		FRDGImportedBufferTFX MeshRestVertexBuffer = Register(GraphBuilder,
			Instance->MeshRestResource->MeshRestVertexBuffer,
			ERDGImportedBufferFlagsTFX::CreateSRV);

		FRDGImportedBufferTFX MeshBoneSkinningBuffer = Register(GraphBuilder,
			Instance->MeshRestResource->MeshBoneSkinningBuffer,
			ERDGImportedBufferFlagsTFX::CreateSRV);

		FRDGImportedBufferTFX MeshBoneIndexBuffer = Register(GraphBuilder,
			Instance->MeshRestResource->MeshBoneIndexBuffer,
			ERDGImportedBufferFlagsTFX::CreateSRV);


		FSDFMeshSkinningCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSDFMeshSkinningCS::FParameters>();

		Parameters->PrimitiveId = Instance->PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetPersistentIndex().Index;
		Parameters->GPUScenePrimitiveSceneData = (*GPUScenePrimitiveBuffer)->GetSRV();
		Parameters->BoneMatricesBuffer = BoneMatricesBuffer;
		Parameters->BoneSkinningBuffer = MeshBoneSkinningBuffer.SRV;
		Parameters->BoneIndexBuffer = MeshBoneIndexBuffer.SRV;
		Parameters->MeshRestVertexBuffer = MeshRestVertexBuffer.SRV;
		Parameters->MeshDeformedVertexBuffer = MeshDeformedVertexBuffer.UAV;
		Parameters->ICMBox = ICMBoxBuffer.UAV;
		Parameters->NumMeshVertices = NumMeshVertices;


		TShaderMapRef<FSDFMeshSkinningCS> ComputeShader(ShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SDFMeshSkinning"),
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));

		GraphBuilder.SetBufferAccessFinal(MeshDeformedVertexBuffer.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(ICMBoxBuffer.Buffer, ERHIAccess::SRVMask);

	}
	else
	{
		const uint32 GroupSize = ComputeGroupSizeSDF();

		const uint32 DispatchCountX = FMath::DivideAndRoundUp(NumMeshVertices, GroupSize);


		FRDGImportedBufferTFX MeshRestVertexBuffer = Register(GraphBuilder,
			Instance->MeshRestResource->MeshRestVertexBuffer,
			ERDGImportedBufferFlagsTFX::CreateSRV);

		FSDFMeshSkinningNoBoneCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSDFMeshSkinningNoBoneCS::FParameters>();

		Parameters->MeshRestVertexBuffer = MeshRestVertexBuffer.SRV;
		Parameters->ICMBox = ICMBoxBuffer.UAV;
		Parameters->NumMeshVertices = NumMeshVertices;


		TShaderMapRef<FSDFMeshSkinningNoBoneCS> ComputeShader(ShaderMap);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SDFMeshSkinningNoBone"),
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));

		GraphBuilder.SetBufferAccessFinal(ICMBoxBuffer.Buffer, ERHIAccess::SRVMask);

	}

}

class FSDFMeshBoxConstructCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSDFMeshBoxConstructCS);
	SHADER_USE_PARAMETER_STRUCT(FSDFMeshBoxConstructCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(FIntVector, NumSDFCells)
		SHADER_PARAMETER(float, CollisionMeshBoxMargin)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FIntermediateCollisionMeshBox>, ICMBoxR)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FCollisionMeshBox>, CMBoxRW)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSDFMeshBoxConstructCS, "/Engine/Private/TressFX/TressFXSimulationSDFMeshBox.usf", "SDFMeshBoxConstruct", SF_Compute);

void AddSDFMeshBoxConstructPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	FTressFXMeshGroupInstance* Instance,
	const FRDGImportedBufferTFX& ICMBoxBuffer,
	const FRDGImportedBufferTFX& CMBoxBuffer)
{

	FSDFMeshBoxConstructCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSDFMeshBoxConstructCS::FParameters>();
	Parameters->NumSDFCells = Instance->NumSDFCells;
	Parameters->CollisionMeshBoxMargin = Instance->CollisionMeshBoxMargin;
	Parameters->ICMBoxR = ICMBoxBuffer.SRV;
	Parameters->CMBoxRW = CMBoxBuffer.UAV;

	TShaderMapRef<FSDFMeshBoxConstructCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SDFMeshBoxConstruct"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(1, 1, 1));

	GraphBuilder.SetBufferAccessFinal(CMBoxBuffer.Buffer, ERHIAccess::SRVMask);

}

class FSDFGenInitCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSDFGenInitCS);
	SHADER_USE_PARAMETER_STRUCT(FSDFGenInitCS, FGlobalShader);

	class FSDFVisualization : SHADER_PERMUTATION_INT("PERMUTATION_SDF_VISUALIZATION", 2);
	using FPermutationDomain = TShaderPermutationDomain<FSDFVisualization>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(int32, TotalNumSDFCells)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFBufferRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFDistanceCounterRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFDistanceStartOffsetBufferRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFDirectionCounterRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFDirectionStartOffsetBufferRW)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FSDFVisualization>() >= 2)
		{
			return false;
		}

		return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSDFGenInitCS, "/Engine/Private/TressFX/TressFXSimulationSDFGenInit.usf", "SDFGenInit", SF_Compute);

void AddSDFGenInitPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	FTressFXMeshGroupInstance* Instance,
	const FRDGImportedBufferTFX& SDFDistanceCounter,
	const FRDGImportedBufferTFX& SDFDistanceStartOffsetBuffer,
#if WITH_EDITOR
	const FRDGImportedBufferTFX& SDFDirectionCounter,
	const FRDGImportedBufferTFX& SDFDirectionStartOffsetBuffer,
#endif
	const FRDGImportedBufferTFX& SDFBuffer)
{
	uint32 TotalNumSDFCells = Instance->NumSDFCells.X * Instance->NumSDFCells.Y * Instance->NumSDFCells.Z;

	const uint32 GroupSize = ComputeGroupSizeSDF();

	const uint32 DispatchCountX = FMath::DivideAndRoundUp(TotalNumSDFCells, GroupSize);

	FSDFGenInitCS::FPermutationDomain PermutationVector;

	FSDFGenInitCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSDFGenInitCS::FParameters>();
	Parameters->TotalNumSDFCells = TotalNumSDFCells;
	Parameters->SDFBufferRW = SDFBuffer.UAV;

#if WITH_EDITOR

	if (Instance->Debug.bEnableVisualizeSDF)
	{
		Parameters->SDFDirectionCounterRW = SDFDirectionCounter.UAV;
		Parameters->SDFDirectionStartOffsetBufferRW = SDFDirectionStartOffsetBuffer.UAV;

		PermutationVector.Set<FSDFGenInitCS::FSDFVisualization>(1);

		TShaderMapRef<FSDFGenInitCS> ComputeShader(ShaderMap, PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SDFGenInit"),
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));

		GraphBuilder.SetBufferAccessFinal(SDFBuffer.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDirectionCounter.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDirectionStartOffsetBuffer.Buffer, ERHIAccess::SRVMask);

	}
	else
	{
		Parameters->SDFDistanceCounterRW = SDFDistanceCounter.UAV;
		Parameters->SDFDistanceStartOffsetBufferRW = SDFDistanceStartOffsetBuffer.UAV;

		PermutationVector.Set<FSDFGenInitCS::FSDFVisualization>(0);

		TShaderMapRef<FSDFGenInitCS> ComputeShader(ShaderMap, PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SDFGenInit"),
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));

		GraphBuilder.SetBufferAccessFinal(SDFBuffer.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDistanceCounter.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDistanceStartOffsetBuffer.Buffer, ERHIAccess::SRVMask);

	}
#else

	Parameters->SDFDistanceCounterRW = SDFDistanceCounter.UAV;
	Parameters->SDFDistanceStartOffsetBufferRW = SDFDistanceStartOffsetBuffer.UAV;

	PermutationVector.Set<FSDFGenInitCS::FSDFVisualization>(0);

	TShaderMapRef<FSDFGenInitCS> ComputeShader(ShaderMap, PermutationVector);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SDFGenInit"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(SDFBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(SDFDistanceCounter.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(SDFDistanceStartOffsetBuffer.Buffer, ERHIAccess::SRVMask);

#endif

}

class FSDFGenConstructCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSDFGenConstructCS);
	SHADER_USE_PARAMETER_STRUCT(FSDFGenConstructCS, FGlobalShader);

	class FSDFVisualization : SHADER_PERMUTATION_INT("PERMUTATION_SDF_VISUALIZATION", 2);
	using FPermutationDomain = TShaderPermutationDomain<FSDFVisualization>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(FIntVector, NumSDFCells)
		SHADER_PARAMETER(int32, TotalNumSDFCells)
		SHADER_PARAMETER(int32, NumGridOffset)
		SHADER_PARAMETER(uint32, NumTriangles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FCollisionMeshBox>, CMBoxR)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTressFXMeshVertexData>, MeshDeformedVertexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MeshRestIndicesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFBufferRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFDistanceCounterRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFDistanceStartOffsetBufferRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FDistanceAndCellIndex>, SDFDistanceBufferRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFDirectionCounterRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFDirectionStartOffsetBufferRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FDirectionAndCellIndex>, SDFDirectionBufferRW)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FSDFVisualization>() >= 2)
		{
			return false;
		}

		return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSDFGenConstructCS, "/Engine/Private/TressFX/TressFXSimulationSDFGenConstruct.usf", "SDFGenConstruct", SF_Compute);

void AddSDFGenConstructPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	FTressFXMeshGroupInstance* Instance,
	const FRDGImportedBufferTFX& CMBoxBuffer,
	const FRDGImportedBufferTFX& MeshDeformedVertexBuffer,
	const FRDGImportedBufferTFX& SDFDistanceCounter,
	const FRDGImportedBufferTFX& SDFDistanceStartOffsetBuffer,
	const FRDGImportedBufferTFX& SDFDistanceBuffer,
#if WITH_EDITOR
	const FRDGImportedBufferTFX& SDFDirectionCounter,
	const FRDGImportedBufferTFX& SDFDirectionStartOffsetBuffer,
	const FRDGImportedBufferTFX& SDFDirectionBuffer,
#endif
	const FRDGImportedBufferTFX& SDFBuffer)
{
	const int32 TotalNumSDFCells = Instance->NumSDFCells.X * Instance->NumSDFCells.Y * Instance->NumSDFCells.Z;

	const uint32 GroupSize = ComputeGroupSizeSDF();

	const uint32 NumTriangles = Instance->TFXMeshData->GetNumIndices() / 3;
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(NumTriangles, GroupSize);

	FRDGImportedBufferTFX MeshRestIndicesBuffer = Register(GraphBuilder,
		Instance->MeshRestResource->MeshRestIndicesBuffer,
		ERDGImportedBufferFlagsTFX::CreateSRV);

	FSDFGenConstructCS::FPermutationDomain PermutationVector;

	FSDFGenConstructCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSDFGenConstructCS::FParameters>();
	Parameters->NumSDFCells = Instance->NumSDFCells;
	Parameters->TotalNumSDFCells = TotalNumSDFCells;
	Parameters->NumGridOffset = Instance->NumGridOffset;
	Parameters->NumTriangles = NumTriangles;
	Parameters->SDFBufferRW = SDFBuffer.UAV;
	Parameters->CMBoxR = CMBoxBuffer.SRV;
	Parameters->MeshDeformedVertexBuffer = MeshDeformedVertexBuffer.SRV;
	Parameters->MeshRestIndicesBuffer = MeshRestIndicesBuffer.SRV;

#if WITH_EDITOR

	if (Instance->Debug.bEnableVisualizeSDF)
	{
		Parameters->SDFDirectionCounterRW = SDFDirectionCounter.UAV;
		Parameters->SDFDirectionStartOffsetBufferRW = SDFDirectionStartOffsetBuffer.UAV;
		Parameters->SDFDirectionBufferRW = SDFDirectionBuffer.UAV;

		PermutationVector.Set<FSDFGenConstructCS::FSDFVisualization>(1);

		TShaderMapRef<FSDFGenConstructCS> ComputeShader(ShaderMap, PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SDFGenConstruct"),
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));

		GraphBuilder.SetBufferAccessFinal(SDFBuffer.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDirectionCounter.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDirectionStartOffsetBuffer.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDirectionBuffer.Buffer, ERHIAccess::SRVMask);
	}
	else
	{
		Parameters->SDFDistanceCounterRW = SDFDistanceCounter.UAV;
		Parameters->SDFDistanceStartOffsetBufferRW = SDFDistanceStartOffsetBuffer.UAV;
		Parameters->SDFDistanceBufferRW = SDFDistanceBuffer.UAV;

		PermutationVector.Set<FSDFGenConstructCS::FSDFVisualization>(0);

		TShaderMapRef<FSDFGenConstructCS> ComputeShader(ShaderMap, PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SDFGenConstruct"),
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));

		GraphBuilder.SetBufferAccessFinal(SDFBuffer.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDistanceCounter.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDistanceStartOffsetBuffer.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDistanceBuffer.Buffer, ERHIAccess::SRVMask);
	}
#else

	Parameters->SDFDistanceCounterRW = SDFDistanceCounter.UAV;
	Parameters->SDFDistanceStartOffsetBufferRW = SDFDistanceStartOffsetBuffer.UAV;
	Parameters->SDFDistanceBufferRW = SDFDistanceBuffer.UAV;

	PermutationVector.Set<FSDFGenConstructCS::FSDFVisualization>(0);

	TShaderMapRef<FSDFGenConstructCS> ComputeShader(ShaderMap, PermutationVector);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SDFGenConstruct"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(SDFBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(SDFDistanceCounter.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(SDFDistanceStartOffsetBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(SDFDistanceBuffer.Buffer, ERHIAccess::SRVMask);

#endif

}

class FSDFGenFinalizeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSDFGenFinalizeCS);
	SHADER_USE_PARAMETER_STRUCT(FSDFGenFinalizeCS, FGlobalShader);

	class FSDFVisualization : SHADER_PERMUTATION_INT("PERMUTATION_SDF_VISUALIZATION", 2);
	using FPermutationDomain = TShaderPermutationDomain<FSDFVisualization>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(FIntVector, NumSDFCells)
		SHADER_PARAMETER(int32, TotalNumSDFCells)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFBufferRW)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFDistanceStartOffsetBufferRW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FDistanceAndCellIndex>, SDFDistanceBufferR)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, SDFDirectionStartOffsetBufferRW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FDirectionAndCellIndex>, SDFDirectionBufferR)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FSDFVisualization>() >= 2)
		{
			return false;
		}

		return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};

IMPLEMENT_GLOBAL_SHADER(FSDFGenFinalizeCS, "/Engine/Private/TressFX/TressFXSimulationSDFGenFinalize.usf", "SDFGenFinalize", SF_Compute);

void AddSDFGenFinalizePass(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	FTressFXMeshGroupInstance* Instance,
	const FRDGImportedBufferTFX& SDFDistanceStartOffsetBuffer,
	const FRDGImportedBufferTFX& SDFDistanceBuffer,
#if WITH_EDITOR
	const FRDGImportedBufferTFX& SDFDirectionStartOffsetBuffer,
	const FRDGImportedBufferTFX& SDFDirectionBuffer,
#endif
	const FRDGImportedBufferTFX& SDFBuffer)
{
	const uint32 TotalNumSDFCells = Instance->NumSDFCells.X * Instance->NumSDFCells.Y * Instance->NumSDFCells.Z;

	const uint32 GroupSize = ComputeGroupSizeSDF();

	const uint32 DispatchCountX = FMath::DivideAndRoundUp(TotalNumSDFCells, GroupSize);

	FSDFGenFinalizeCS::FPermutationDomain PermutationVector;

	FSDFGenFinalizeCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSDFGenFinalizeCS::FParameters>();
	Parameters->NumSDFCells = Instance->NumSDFCells;
	Parameters->TotalNumSDFCells = TotalNumSDFCells;
	Parameters->SDFBufferRW = SDFBuffer.UAV;

#if WITH_EDITOR

	if (Instance->Debug.bEnableVisualizeSDF)
	{
		Parameters->SDFDirectionStartOffsetBufferRW = SDFDirectionStartOffsetBuffer.UAV;
		Parameters->SDFDirectionBufferR = SDFDirectionBuffer.SRV;

		PermutationVector.Set<FSDFGenFinalizeCS::FSDFVisualization>(1);

		TShaderMapRef<FSDFGenFinalizeCS> ComputeShader(ShaderMap, PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SDFGenFinalize"),
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));

		GraphBuilder.SetBufferAccessFinal(SDFBuffer.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(SDFDirectionStartOffsetBuffer.Buffer, ERHIAccess::SRVMask);

	}
	else
	{
		Parameters->SDFDistanceStartOffsetBufferRW = SDFDistanceStartOffsetBuffer.UAV;
		Parameters->SDFDistanceBufferR = SDFDistanceBuffer.SRV;

		PermutationVector.Set<FSDFGenFinalizeCS::FSDFVisualization>(0);

		TShaderMapRef<FSDFGenFinalizeCS> ComputeShader(ShaderMap, PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SDFGenFinalize"),
			ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));

		GraphBuilder.SetBufferAccessFinal(SDFBuffer.Buffer, ERHIAccess::SRVMask);


	}
#else

	Parameters->SDFDistanceStartOffsetBufferRW = SDFDistanceStartOffsetBuffer.UAV;
	Parameters->SDFDistanceBufferR = SDFDistanceBuffer.SRV;

	PermutationVector.Set<FSDFGenFinalizeCS::FSDFVisualization>(0);

	TShaderMapRef<FSDFGenFinalizeCS> ComputeShader(ShaderMap, PermutationVector);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("SDFGenFinalize"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX, 1, 1));

	GraphBuilder.SetBufferAccessFinal(SDFBuffer.Buffer, ERHIAccess::SRVMask);

#endif

}

void ComputeTressFXSDFGen(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	TRefCountPtr<FRDGPooledBuffer>* GPUScenePrimitiveBuffer,
	struct FTressFXMeshGroupInstance* Instance)
{
	FRDGImportedBufferTFX ICMBoxBuffer = Register(GraphBuilder,
		Instance->MeshDeformedResource->IntermediateCollisionMeshBoxBuffer,
		ERDGImportedBufferFlagsTFX::CreateViews);

	FRDGImportedBufferTFX CMBoxBuffer = Register(GraphBuilder,
		Instance->MeshDeformedResource->CollisionMeshBoxBuffer,
		ERDGImportedBufferFlagsTFX::CreateViews);

	AddSDFMeshBoxInitPass(GraphBuilder,
		View,
		ShaderMap,
		Instance,
		ICMBoxBuffer);

	uint32 NumMeshVertices = Instance->TFXMeshData->GetNumVertices();

	FRDGImportedBufferTFX MeshDeformedVertexBuffer = Register(GraphBuilder,
		Instance->MeshDeformedResource->MeshDeformedVertexBuffer,
		ERDGImportedBufferFlagsTFX::CreateViews);

	AddSDFMeshSkinningPass(GraphBuilder,
		View,
		ShaderMap,
		GPUScenePrimitiveBuffer,
		Instance,
		NumMeshVertices,
		MeshDeformedVertexBuffer,
		ICMBoxBuffer);

	AddSDFMeshBoxConstructPass(GraphBuilder,
		View,
		ShaderMap,
		Instance,
		ICMBoxBuffer,
		CMBoxBuffer);


	FRDGImportedBufferTFX SDFBuffer = Register(GraphBuilder,
		Instance->MeshDeformedResource->SDFBuffer,
		ERDGImportedBufferFlagsTFX::CreateViews);

	FRDGImportedBufferTFX SDFDistanceCounter = Register(GraphBuilder,
		Instance->MeshDeformedResource->SDFDistanceCounter,
		ERDGImportedBufferFlagsTFX::CreateViews);

	FRDGImportedBufferTFX SDFDistanceStartOffsetBuffer = Register(GraphBuilder,
		Instance->MeshDeformedResource->SDFDistanceStartOffsetBuffer,
		ERDGImportedBufferFlagsTFX::CreateViews);

	FRDGImportedBufferTFX SDFDistanceBuffer = Register(GraphBuilder,
		Instance->MeshDeformedResource->SDFDistanceBuffer,
		ERDGImportedBufferFlagsTFX::CreateViews);

#if WITH_EDITOR

	FRDGImportedBufferTFX SDFDirectionCounter = Register(GraphBuilder,
		Instance->MeshDeformedResource->SDFDirectionCounter,
		ERDGImportedBufferFlagsTFX::CreateViews);

	FRDGImportedBufferTFX SDFDirectionStartOffsetBuffer = Register(GraphBuilder,
		Instance->MeshDeformedResource->SDFDirectionStartOffsetBuffer,
		ERDGImportedBufferFlagsTFX::CreateViews);

	FRDGImportedBufferTFX SDFDirectionBuffer = Register(GraphBuilder,
		Instance->MeshDeformedResource->SDFDirectionBuffer,
		ERDGImportedBufferFlagsTFX::CreateViews);

#endif

	AddSDFGenInitPass(GraphBuilder,
		View,
		ShaderMap,
		Instance,
		SDFDistanceCounter,
		SDFDistanceStartOffsetBuffer,
#if WITH_EDITOR
		SDFDirectionCounter,
		SDFDirectionStartOffsetBuffer,
#endif
		SDFBuffer);

	AddSDFGenConstructPass(GraphBuilder,
		View,
		ShaderMap,
		Instance,
		CMBoxBuffer,
		MeshDeformedVertexBuffer,
		SDFDistanceCounter,
		SDFDistanceStartOffsetBuffer,
		SDFDistanceBuffer,
#if WITH_EDITOR
		SDFDirectionCounter,
		SDFDirectionStartOffsetBuffer,
		SDFDirectionBuffer,
#endif
		SDFBuffer);

	AddSDFGenFinalizePass(GraphBuilder,
		View,
		ShaderMap,
		Instance,
		SDFDistanceStartOffsetBuffer,
		SDFDistanceBuffer,
#if WITH_EDITOR
		SDFDirectionStartOffsetBuffer,
		SDFDirectionBuffer,
#endif
		SDFBuffer);

}
