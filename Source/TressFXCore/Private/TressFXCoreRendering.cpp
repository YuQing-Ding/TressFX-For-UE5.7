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
#include "TressFXCoreRendering.h"
#include "PrimitiveSceneInfo.h"
#include "TressFXInstance.h"
#include "TressFXInterface.h"
#include "TressFXCommon.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"


inline uint32 GetGroupSizePermutation(uint32 GroupSize)
{
	return GroupSize == 64 ? 0 : (GroupSize == 32 ? 1 : 2);
}

static inline uint32 ComputeGroupSize()
{
	const uint32 GroupSize = 64;
	
	return GroupSize;
}

static FIntVector ComputeDispatchCount(uint32 ItemCount, uint32 GroupSize)
{
	const uint32 BatchCount = FMath::DivideAndRoundUp(ItemCount, GroupSize);
	const uint32 DispatchCountX = FMath::FloorToInt(FMath::Sqrt((float)BatchCount));
	const uint32 DispatchCountY = DispatchCountX + FMath::DivideAndRoundUp(BatchCount - DispatchCountX * DispatchCountX, DispatchCountX);

	check(DispatchCountX <= 65535);
	check(DispatchCountY <= 65535);
	check(BatchCount <= DispatchCountX * DispatchCountY);
	return FIntVector(DispatchCountX, DispatchCountY, 1);
}
class FClearGroupAABBTFXCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearGroupAABBTFXCS);
	SHADER_USE_PARAMETER_STRUCT(FClearGroupAABBTFXCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OutGroupAABBBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CLEARGROUPAABB"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearGroupAABBTFXCS, "/Engine/Private/TressFX/TressFXGroupCulling.usf", "MainClearGroupAABBCS", SF_Compute);

static void AddClearGroupAABBPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	uint32 ClusterCount,
	FRDGImportedBufferTFX& OutClusterAABBBuffer,
	FRDGImportedBufferTFX& OutGroupAABBBuffer)
{
	check(OutClusterAABBBuffer.Buffer);

	FClearGroupAABBTFXCS::FParameters* Parameters = GraphBuilder.AllocParameters<FClearGroupAABBTFXCS::FParameters>();
	Parameters->OutGroupAABBBuffer = OutGroupAABBBuffer.UAV;

	TShaderMapRef<FClearGroupAABBTFXCS> ComputeShader(ShaderMap);

	const FIntVector DispatchCount = DispatchCount.DivideAndRoundUp(FIntVector(ClusterCount * 6, 1, 1), FIntVector(64, 1, 1));
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TressFXClearGroupAABB"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		DispatchCount);

	GraphBuilder.SetBufferAccessFinal(OutGroupAABBBuffer.Buffer, ERHIAccess::SRVMask);
}


class FTressFXFollowStrandsOffsetCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXFollowStrandsOffsetCS);
	SHADER_USE_PARAMETER_STRUCT(FTressFXFollowStrandsOffsetCS, FGlobalShader);


	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, StrandsCount)
		SHADER_PARAMETER(uint32, NumOfStrandsPerThreadGroup)
		SHADER_PARAMETER(uint32, NumFollowStrands)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float2>, FollowRootRandomBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, FollowRootOffsetBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, GuidesDeformedPositionBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTressFXFollowStrandsOffsetCS, "/Engine/Private/TressFX/TressFXFollowStrandsOffset.usf", "MainCS", SF_Compute);

static void AddTressFXFollowStrandsOffsetPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	//const FShaderDrawDebugData* ShaderDrawData,
	FTressFXGroupInstance* Instance,
	const uint32 StrandsCount,
	const uint32 NumVerticesPerStrand,
	const FRDGBufferSRVRef& GuidesDeformedPositionBuffer,
	FRDGImportedBufferTFX& FollowRootOffsetBuffer)
{
	const uint32 GroupSize = ComputeGroupSize();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(StrandsCount, GroupSize);

	FRDGBufferSRVRef FollowRootRandomBuffer = GraphBuilder.CreateSRV(CreateStructuredBuffer(GraphBuilder, TEXT("TressFXFollowRootRandomBuffer"), 
		sizeof(FVector2D), 
		Instance->Strands.StrandsResource->FollowRootRandoms.Num(),
		Instance->Strands.StrandsResource->FollowRootRandoms.GetData(),
		Instance->Strands.StrandsResource->FollowRootRandoms.Num()*sizeof(FVector2D)));


	FTressFXFollowStrandsOffsetCS::FParameters* Parameters = GraphBuilder.AllocParameters<FTressFXFollowStrandsOffsetCS::FParameters>();
	Parameters->GuidesDeformedPositionBuffer = GuidesDeformedPositionBuffer;

	Parameters->NumOfStrandsPerThreadGroup = TRESSFX_SIM_THREAD_GROUP_SIZE / NumVerticesPerStrand;
	Parameters->StrandsCount = StrandsCount;
	Parameters->NumFollowStrands = Instance->NumFollowStrands;
	Parameters->FollowRootOffsetBuffer = FollowRootOffsetBuffer.UAV;
	Parameters->FollowRootRandomBuffer = FollowRootRandomBuffer;

	TShaderMapRef<FTressFXFollowStrandsOffsetCS> ComputeShader(ShaderMap);

	{
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TressFXFollowStrandsOffset"),
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));
	}

	GraphBuilder.SetBufferAccessFinal(FollowRootOffsetBuffer.Buffer, ERHIAccess::SRVMask);

}


class FTressFXInterpolationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXInterpolationCS);
	SHADER_USE_PARAMETER_STRUCT(FTressFXInterpolationCS, FGlobalShader);

	
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VertexCount)
		SHADER_PARAMETER(uint32, NumOfStrandsPerThreadGroup)
		SHADER_PARAMETER(float, TipSeparationFactor)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint32>, RandomCurveIndexArrBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, GuidesDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, StrandsDeformedPositionBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_TRESSFXINTERPOLATION"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTressFXInterpolationCS, "/Engine/Private/TressFX/TressFXInterpolation.usf", "MainCS", SF_Compute);



static void AddTressFXInterpolationPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	//const FShaderDrawDebugData* ShaderDrawData,
	FTressFXGroupInstance* Instance,
	const uint32 VertexCount,
	const uint32 NumVerticesPerStrand,
	const FRDGBufferSRVRef& GuidesDeformedPositionBuffer,
	FRDGImportedBufferTFX& OutStrandsDeformedPositionBuffer,
	const FRDGBufferSRVRef& RandomCurveIndexArrBuffer)
{	
	const uint32 GroupSize = ComputeGroupSize();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(VertexCount, GroupSize);

	FTressFXInterpolationCS::FParameters* Parameters = GraphBuilder.AllocParameters<FTressFXInterpolationCS::FParameters>();
	Parameters->GuidesDeformedPositionBuffer = GuidesDeformedPositionBuffer;
	Parameters->StrandsDeformedPositionBuffer = OutStrandsDeformedPositionBuffer.UAV;
	
	Parameters->NumOfStrandsPerThreadGroup = TRESSFX_SIM_THREAD_GROUP_SIZE / NumVerticesPerStrand;
	Parameters->VertexCount = VertexCount;
	Parameters->TipSeparationFactor = Instance->TipSeparationFactor;

	Parameters->RandomCurveIndexArrBuffer = RandomCurveIndexArrBuffer;

	TShaderMapRef<FTressFXInterpolationCS> ComputeShader(ShaderMap);

	{
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TressFXInterpolation"),
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));
	}

	GraphBuilder.SetBufferAccessFinal(OutStrandsDeformedPositionBuffer.Buffer, ERHIAccess::SRVMask);

}

class FTressFXStrandsIDCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXStrandsIDCS);
	SHADER_USE_PARAMETER_STRUCT(FTressFXStrandsIDCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumStrandsToRender)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint32>, RandomCurveIndexArrBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, StrandsIDBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTressFXStrandsIDCS, "/Engine/Private/TressFX/TressFXInterpolation.usf", "MainStrandsIDCS", SF_Compute);



static void AddTressFXStrandsIDPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	//const FShaderDrawDebugData* ShaderDrawData,
	FTressFXGroupInstance* Instance,
	const uint32 VertexCount,
	const uint32 NumVerticesPerStrand,
	FRDGImportedBufferTFX& StrandsIDBuffer,
	const FRDGBufferSRVRef& RandomCurveIndexArrBuffer)
{
	uint32 StrandsCount = VertexCount / NumVerticesPerStrand;

	const uint32 GroupSize = ComputeGroupSize();
	const uint32 DispatchCountX = FMath::DivideAndRoundUp(StrandsCount, GroupSize);

	FTressFXStrandsIDCS::FParameters* Parameters = GraphBuilder.AllocParameters<FTressFXStrandsIDCS::FParameters>();
	Parameters->NumStrandsToRender = StrandsCount;

	Parameters->RandomCurveIndexArrBuffer = RandomCurveIndexArrBuffer;
	Parameters->StrandsIDBuffer = StrandsIDBuffer.UAV;

	TShaderMapRef<FTressFXStrandsIDCS> ComputeShader(ShaderMap);

	{
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TressFXStrandsID"),
			ComputeShader,
			Parameters,
			FIntVector(DispatchCountX, 1, 1));
	}

	GraphBuilder.SetBufferAccessFinal(StrandsIDBuffer.Buffer, ERHIAccess::SRVMask);

}

class FGroupAABBInitCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGroupAABBInitCS);
	SHADER_USE_PARAMETER_STRUCT(FGroupAABBInitCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<int>, OutGroupAABBBuffer)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
		OutEnvironment.SetDefine(TEXT("SHADER_GROUPAABB"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGroupAABBInitCS, "/Engine/Private/TressFX/TressFXInterpolation.usf", "GroupAABBInitCS", SF_Compute);

void AddTressFXGroupAABBInitPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const FRDGImportedBufferTFX& GroupAABBBuffer)
{

	FGroupAABBInitCS::FParameters* Parameters = GraphBuilder.AllocParameters<FGroupAABBInitCS::FParameters>();

	Parameters->OutGroupAABBBuffer = GroupAABBBuffer.UAV;

	TShaderMapRef<FGroupAABBInitCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("GroupAABBInit"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		ComputeShader,
		Parameters,
		FIntVector(1, 1, 1));

	GraphBuilder.SetBufferAccessFinal(GroupAABBBuffer.Buffer, ERHIAccess::SRVMask);

}

class FTressFXGroupAABBCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXGroupAABBCS);
	SHADER_USE_PARAMETER_STRUCT(FTressFXGroupAABBCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, DispatchCountX)
		SHADER_PARAMETER(uint32, ClusterCount)
		SHADER_PARAMETER(uint32, NumGuidesVertices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, GuidesDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, ClusterVertexIdBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, ClusterIdBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, ClusterIndexOffsetBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, ClusterIndexCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TressFXVF_CullingIndirectBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OutClusterAABBBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OutGroupAABBBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_GROUPAABB"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FTressFXGroupAABBCS, "/Engine/Private/TressFX/TressFXInterpolation.usf", "GroupAABBEvaluationCS", SF_Compute);

struct FRDGTressFXCullingData
{
	FRDGImportedBufferTFX TressFXVF_CullingIndirectBuffer;

	uint32 ClusterCount = 0;
	FRDGImportedBufferTFX ClusterAABBBuffer;
	FRDGImportedBufferTFX GroupAABBBuffer;
};

static FIntVector ComputeDispatchGroupCount2D(uint32 GroupCount)
{
	const uint32 DispatchCountX = FMath::FloorToInt(FMath::Sqrt((float)GroupCount));
	const uint32 DispatchCountY = DispatchCountX + FMath::DivideAndRoundUp(GroupCount - DispatchCountX * DispatchCountX, DispatchCountX);

	check(DispatchCountX <= 65535);
	check(DispatchCountY <= 65535);
	check(GroupCount <= DispatchCountX * DispatchCountY);
	return FIntVector(DispatchCountX, DispatchCountY, 1);
}

static void AddTressFXGroupAABBPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const uint32 NumGuidesVerticesToRender,
	//const FVector3f& OutHairWorldOffset,
	FTressFXClusterData::FHairGroup& ClusterData,
	FRDGTressFXCullingData& ClusterAABBData,
	FRDGImportedBufferTFX& GuidesDeformedPositionBuffer,
	FRDGImportedBufferTFX& DrawIndirectRasterComputeBuffer)
{
	const uint32 GroupSize = ComputeGroupSize();

	const uint32 DispatchCountX = FMath::DivideAndRoundUp(NumGuidesVerticesToRender, GroupSize) - 1;


	FTressFXGroupAABBCS::FParameters* Parameters = GraphBuilder.AllocParameters<FTressFXGroupAABBCS::FParameters>();
	Parameters->NumGuidesVertices = NumGuidesVerticesToRender;
	Parameters->ClusterCount = ClusterData.ClusterCount;
	Parameters->GuidesDeformedPositionBuffer = GuidesDeformedPositionBuffer.SRV;

	Parameters->TressFXVF_CullingIndirectBuffer = DrawIndirectRasterComputeBuffer.SRV; // Used for checking max vertex count
	Parameters->OutClusterAABBBuffer = ClusterAABBData.ClusterAABBBuffer.UAV;
	Parameters->OutGroupAABBBuffer = ClusterAABBData.GroupAABBBuffer.UAV;

	TShaderMapRef<FTressFXGroupAABBCS> ComputeShader(ShaderMap);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("TressFXGroupAABB"),
		ComputeShader,
		Parameters,
		FIntVector(DispatchCountX,1,1));

	GraphBuilder.SetBufferAccessFinal(ClusterAABBData.GroupAABBBuffer.Buffer, ERHIAccess::SRVMask);
}

class FTressFXTangentCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTressFXTangentCS);
	SHADER_USE_PARAMETER_STRUCT(FTressFXTangentCS, FGlobalShader);

	class FCulling : SHADER_PERMUTATION_INT("PERMUTATION_CULLING", 2);
	using FPermutationDomain = TShaderPermutationDomain<FCulling>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VertexCount)
		SHADER_PARAMETER(uint32, DispatchCountX)
		SHADER_PARAMETER(uint32, TressFXVF_bIsCullingEnable)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, StrandsDeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TressFXVF_CullingIndirectBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TressFXVF_CullingIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutputTangentBuffer)
		//SHADER_PARAMETER_RDG_BUFFER(Buffer, IndirectBufferArgs)
		END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::Strands, Parameters.Platform); }
};

IMPLEMENT_GLOBAL_SHADER(FTressFXTangentCS, "/Engine/Private/TressFX/TressFXTangent.usf", "MainCS", SF_Compute);


static void AddTressFXTangentPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	uint32 VertexCount,
	FTressFXGroupPublicData* TressFXGroupPublicData,
	FRDGBufferSRVRef StrandsDeformedPositionBuffer,
	FRDGImportedBufferTFX OutTangentBuffer)
{
	const uint32 GroupSize = ComputeGroupSize();
	const FIntVector DispatchCount = ComputeDispatchCount(VertexCount, GroupSize);
	const bool bCullingEnable = false;// TressFXGroupPublicData->GetCullingResultAvailable();

	FTressFXTangentCS::FParameters* Parameters = GraphBuilder.AllocParameters<FTressFXTangentCS::FParameters>();
	Parameters->StrandsDeformedPositionBuffer = StrandsDeformedPositionBuffer;
	Parameters->OutputTangentBuffer = OutTangentBuffer.UAV;
	Parameters->VertexCount = VertexCount;
	Parameters->DispatchCountX = DispatchCount.X;
	Parameters->TressFXVF_bIsCullingEnable = bCullingEnable ? 1 : 0;

	FTressFXTangentCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FTressFXTangentCS::FCulling>(bCullingEnable ? 1 : 0);

	TShaderMapRef<FTressFXTangentCS> ComputeShader(ShaderMap, PermutationVector);
	
	{
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TressFXTangent(culling=off)"),
			ComputeShader,
			Parameters,
			DispatchCount);
	}

	GraphBuilder.SetBufferAccessFinal(OutTangentBuffer.Buffer, ERHIAccess::SRVMask);
}

FRDGTressFXCullingData ImportCullingData(FRDGBuilder& GraphBuilder, FTressFXGroupPublicData* In)
{
	FRDGTressFXCullingData Out;
	Out.TressFXVF_CullingIndirectBuffer = Register(GraphBuilder, In->GetDrawIndirectRasterComputeBuffer(), ERDGImportedBufferFlagsTFX::CreateViews);
	
	Out.ClusterCount = In->GetClusterCount();
	Out.ClusterAABBBuffer = Register(GraphBuilder, In->GetClusterAABBBuffer(), ERDGImportedBufferFlagsTFX::CreateViews);
	Out.GroupAABBBuffer = Register(GraphBuilder, In->GetGroupAABBBuffer(), ERDGImportedBufferFlagsTFX::CreateViews);

	return Out;
}

void ComputeTressFXInterpolation(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	//const FShaderDrawDebugData* ShaderDrawData,
	struct FTressFXGroupInstance* Instance,
	int32 MeshLODIndex,
	FTressFXClusterData* InClusterData)
{
	const ETressFXGeometryType InstanceGeometryType = Instance->GeometryType;

	if (InstanceGeometryType == ETressFXGeometryType::Strands_)
	{
		FRDGTressFXCullingData CullingData = ImportCullingData(GraphBuilder, Instance->TressFXGroupPublicData);

		{
			AddClearGroupAABBPass(
				GraphBuilder,
				ShaderMap,
				CullingData.ClusterCount,
				CullingData.ClusterAABBBuffer,
				CullingData.GroupAABBBuffer);
		}

		FRDGImportedBufferTFX Strands_DeformedPositionBuffer = Register(GraphBuilder, Instance->Strands.StrandsResource->GetBuffer(FTressFXStrandsResource::Current), ERDGImportedBufferFlagsTFX::CreateViews);
		FRDGImportedBufferTFX Strands_DeformedTangentBuffer = Register(GraphBuilder, Instance->Strands.StrandsResource->TangentBuffer, ERDGImportedBufferFlagsTFX::CreateViews);

		FRDGImportedBufferTFX Strands_StrandsIDBuffer = Register(GraphBuilder, Instance->Strands.StrandsResource->StrandsIDBuffer, ERDGImportedBufferFlagsTFX::CreateViews);
		{
			uint32 VertexCount = Instance->Guides.GuidesRestResource->TressFXData.GetNumPointsToRender();

			AddTressFXInterpolationPass(
					GraphBuilder,
					ShaderMap,
					//nullptr,
					Instance,
					VertexCount,
					Instance->TFXData->NumVerticesPerStrand,
					RegisterAsSRV(GraphBuilder, Instance->Guides.GuidesDeformedResource->GetBuffer(FTressFXGuidesDeformedResource::Current)),
					Strands_DeformedPositionBuffer,
					RegisterAsSRV(GraphBuilder, Instance->Guides.GuidesRestResource->RandomCurveIndexArrBuffer)
				);

			AddTressFXStrandsIDPass(
				GraphBuilder,
				ShaderMap,
				//nullptr,
				Instance,
				VertexCount,
				Instance->TFXData->NumVerticesPerStrand,
				Strands_StrandsIDBuffer,
				RegisterAsSRV(GraphBuilder, Instance->Guides.GuidesRestResource->RandomCurveIndexArrBuffer)
			);
		}

		{
			FTressFXClusterData::FHairGroup& TressFXGroupCluster = InClusterData->HairGroups[Instance->TressFXGroupPublicData->ClusterDataIndex];
			if (TressFXGroupCluster.bVisible)
			{
				const bool bNeedAABB = Instance->bUseCPULODSelection || Instance->bCastShadow;

				if (bNeedAABB)
				{
					AddTressFXGroupAABBInitPass(
					GraphBuilder,
					ShaderMap,
					CullingData.ClusterAABBBuffer);

					uint32 VertexCount = Instance->TFXData->GetNumPointsToRender();
					if (0 == VertexCount)
						VertexCount = Instance->TFXData->GetNumPoints();

					FRDGImportedBufferTFX Strands_CulledVertexCount = Register(GraphBuilder, Instance->TressFXGroupPublicData->GetDrawIndirectRasterComputeBuffer(), ERDGImportedBufferFlagsTFX::CreateSRV);
					AddTressFXGroupAABBPass(
						GraphBuilder,
						ShaderMap,
						VertexCount,
						TressFXGroupCluster,
						CullingData,
						Strands_DeformedPositionBuffer,
						Strands_CulledVertexCount);
				}
			}
		}

		if (Instance->Strands.StrandsResource->NeedsToUpdateTangent())
		{
			uint32 VertexCount = Instance->Strands.StrandsResource->GetVertexCount();
			if (0 == VertexCount)
				VertexCount = Instance->Strands.StrandsResource->TressFXData.GetNumPoints();

			AddTressFXTangentPass(
				GraphBuilder,
				ShaderMap,
				VertexCount,
				Instance->TressFXGroupPublicData,
				Strands_DeformedPositionBuffer.SRV,
				Strands_DeformedTangentBuffer);
		}
	}
}


struct FTressFXScaleAndClipDesc
{
	float InHairLength = 0;
	float InHairRadius = 0;
	float OutHairRadius = 0;
	float MaxOutHairRadius = 0;
	float HairRadiusRootScale = 1;
	float HairRadiusTipScale = 1;
	float HairLengthClip = 1;
	bool  bEnable = true;

	bool IsEnable() const
	{
		return
			bEnable && (
				InHairRadius != OutHairRadius ||
				HairRadiusRootScale != 1 ||
				HairRadiusTipScale != 1 ||
				HairLengthClip < 1);
	}
};

static int32 GTressFXDebugStrandsMode = 0;
static FAutoConsoleVariableRef CVarDebugPhysicsStrand(TEXT("r.TressFX.StrandsMode"), GTressFXDebugStrandsMode, TEXT("Render debug mode for TressFX. 0:off, 1:simulation strands, 2:render strands with colored simulation strands influence, 3:hair UV, 4:hair root UV, 5: hair seed, 6: dimensions"));

ETressFXDebugMode GetTressFXDebugStrandsMode()
{
	switch (GTressFXDebugStrandsMode)
	{
	case  0:  return ETressFXDebugMode::NoneDebug;
	case  1:  return ETressFXDebugMode::SimTressFX;
	case  2:  return ETressFXDebugMode::RenderTressFX;
	case  3:  return ETressFXDebugMode::RenderHairRootUV;
	case  4:  return ETressFXDebugMode::RenderHairRootUDIM;
	case  5:  return ETressFXDebugMode::RenderHairUV;
	case  6:  return ETressFXDebugMode::RenderHairSeed;
	case  7:  return ETressFXDebugMode::RenderHairDimension;
	case  8:  return ETressFXDebugMode::RenderHairRadiusVariation;
	case  9:  return ETressFXDebugMode::RenderHairBaseColor;
	case 10:  return ETressFXDebugMode::RenderHairRoughness;
	case 11:  return ETressFXDebugMode::RenderVisCluster;
	default:  return ETressFXDebugMode::NoneDebug;
	};
}

ETressFXDebugMode GetTressFXGeometryDebugMode(const FTressFXGroupInstance* Instance)
{
	return Instance->Debug.DebugMode != ETressFXDebugMode::NoneDebug ? Instance->Debug.DebugMode : GetTressFXDebugStrandsMode();
}

FTressFXScaleAndClipDesc ComputeTressFXScaleAndClipDesc(const FTressFXGroupInstance* Instance)
{
	FTressFXScaleAndClipDesc Out;
	Out.bEnable = true;
	//Out.InHairLength = Instance->TFXData->StrandsCurves.MaxLength;
	Out.OutHairRadius = Instance->StrandsWidth * 0.5f;
	Out.MaxOutHairRadius = Instance->StrandsWidth * 0.5f;
	
	return Out;
}


bool NeedsPatchAttributeBuffer(ETressFXDebugMode DebugMode)
{
	return DebugMode == ETressFXDebugMode::RenderTressFX || DebugMode == ETressFXDebugMode::RenderVisCluster;
}

FTressFXGroupPublicData::FVertexFactoryInput ComputeTressFXVertexInputData(const FTressFXGroupInstance* Instance)
{
	FTressFXGroupPublicData::FVertexFactoryInput OutVFInput;
	if (!Instance || Instance->GeometryType != ETressFXGeometryType::Strands_)
		return OutVFInput;

	const FTressFXScaleAndClipDesc ScaleAndClipDesc = ComputeTressFXScaleAndClipDesc(Instance);

	const ETressFXDebugMode DebugMode = GetTressFXGeometryDebugMode(Instance);
	const bool bDebugModePatchedAttributeBuffer = NeedsPatchAttributeBuffer(DebugMode);

	
	{
		OutVFInput.Strands.PositionBuffer = Instance->Strands.StrandsResource->GetBuffer(FTressFXStrandsResource::EFrameType::Current).SRV;
		OutVFInput.Strands.PrevPositionBuffer = Instance->Strands.StrandsResource->GetBuffer(FTressFXStrandsResource::EFrameType::Previous).SRV;
		OutVFInput.Strands.LengthBuffer = Instance->Guides.GuidesRestResource->GuidesRestLengthBuffer.SRV;
		OutVFInput.Strands.TangentBuffer = Instance->Strands.StrandsResource->TangentBuffer.SRV;
		
		OutVFInput.Strands.RootUVBuffer = Instance->Guides.GuidesRestResource->GuidesRootUVBuffer.SRV;
		OutVFInput.Strands.StrandsIDBuffer = Instance->Strands.StrandsResource->StrandsIDBuffer.SRV;

		const uint32 LODVertexCount = Instance->TressFXGroupPublicData->CurrentActiveStrandsCount * Instance->TFXData->NumVerticesPerStrand;
		

		OutVFInput.Strands.VertexCount = LODVertexCount;
		OutVFInput.Strands.NumVerticesPerStrand = Instance->TFXData->NumVerticesPerStrand;
		OutVFInput.Strands.HairRadius = ScaleAndClipDesc.MaxOutHairRadius;
		OutVFInput.Strands.MaxLength = 1.f;
		OutVFInput.Strands.HairDensity = FMath::Max(1.f,Instance->TressFXGroupPublicData->ContinuousLodRadiusScale);
		OutVFInput.Strands.TranslucencyDensity = Instance->TranslucencyDensity * Instance->TressFXGroupPublicData->ContinuousLodRadiusScale;
		OutVFInput.Strands.bUseStableRasterization = true;
		OutVFInput.Strands.bScatterSceneLighting = true;

	}

	return OutVFInput;
}

