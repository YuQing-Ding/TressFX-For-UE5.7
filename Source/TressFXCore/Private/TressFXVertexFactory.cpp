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

#include "TressFXVertexFactory.h"
#include "SceneView.h"
#include "MeshBatch.h"
#include "ShaderParameterUtils.h"
#include "Rendering/ColorVertexBuffer.h"
#include "MeshMaterialShader.h"
#include "TressFXInterface.h"
#include "TressFXInstance.h"
#include "MaterialDomain.h"
#include "MeshDrawShaderBindings.h"
#include "DataDrivenShaderPlatformInfo.h"


template<typename T> inline void VFS_BindParam(FMeshDrawSingleShaderBindings& ShaderBindings, const FShaderResourceParameter& Param, T* Value) { if (Param.IsBound() && Value) ShaderBindings.Add(Param, Value); }
template<typename T> inline void VFS_BindParam(FMeshDrawSingleShaderBindings& ShaderBindings, const FShaderParameter& Param, const T& Value) { if (Param.IsBound()) ShaderBindings.Add(Param, Value); }

FTressFXGroupPublicData::FVertexFactoryInput ComputeTressFXVertexInputData(const FTressFXGroupInstance* Instance);


inline FRHIShaderResourceView* GetPositionSRV(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.PositionBuffer; };
inline FRHIShaderResourceView* GetPreviousPositionSRV(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.PrevPositionBuffer; }
inline FRHIShaderResourceView* GetLengthSRV(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.PositionBuffer; };
inline FRHIShaderResourceView* GetTangentSRV(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.TangentBuffer; }
inline FRHIShaderResourceView* GetRootUVSRV(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.RootUVBuffer; }
inline FRHIShaderResourceView* GetStrandsIDSRV(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.StrandsIDBuffer; }

inline bool  UseStableRasterization(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.bUseStableRasterization; };
inline bool  UseScatterSceneLighting(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.bScatterSceneLighting; };
inline float GetMaxStrandRadius(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.HairRadius; };
inline float GetMaxStrandLength(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.MaxLength; };
inline float GetHairDensity(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.HairDensity; };
inline float GetTranslucencyDensity(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.TranslucencyDensity; };
inline uint32 GetNumVerticesPerStrand(const FTressFXGroupPublicData::FVertexFactoryInput& VFInput) { return VFInput.Strands.NumVerticesPerStrand; };

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FTressFXVertexFactoryUniformShaderParameters, "TressFXVF");

FTressFXVertexFactoryUniformShaderParameters FTressFXGroupInstance::GetTressFXUniformShaderParameters() const
{
	const FTressFXGroupPublicData::FVertexFactoryInput VFInput = ComputeTressFXVertexInputData(this);
	FTressFXVertexFactoryUniformShaderParameters TressFXVFUSP;
	TressFXVFUSP.PositionBuffer = GetPositionSRV(VFInput);
	TressFXVFUSP.PreviousPositionBuffer = GetPreviousPositionSRV(VFInput);
	TressFXVFUSP.LengthBuffer = GetLengthSRV(VFInput);
	TressFXVFUSP.TangentBuffer = GetTangentSRV(VFInput);
	TressFXVFUSP.Radius = GetMaxStrandRadius(VFInput);
	TressFXVFUSP.Density = GetHairDensity(VFInput);
	TressFXVFUSP.TranslucencyDensity = GetTranslucencyDensity(VFInput);
	TressFXVFUSP.StableRasterization = UseStableRasterization(VFInput) ? 1u : 0u;
	TressFXVFUSP.ScatterSceneLighing = UseScatterSceneLighting(VFInput) ? 1u : 0u;
	TressFXVFUSP.RootUVBuffer = GetRootUVSRV(VFInput);
	TressFXVFUSP.StrandsIDBuffer = GetStrandsIDSRV(VFInput);
	TressFXVFUSP.NumVerticesPerStrand = GetNumVerticesPerStrand(VFInput);

	/*VFS_BindParam(ShaderBindings, PositionBuffer, GetPositionSRV(VFInput));
	VFS_BindParam(ShaderBindings, PreviousPositionBuffer, GetPreviousPositionSRV(VFInput));
	VFS_BindParam(ShaderBindings, LengthBuffer, GetLengthSRV(VFInput));
	VFS_BindParam(ShaderBindings, TangentBuffer, GetTangentSRV(VFInput));
	VFS_BindParam(ShaderBindings, Radius, GetMaxStrandRadius(VFInput));
	VFS_BindParam(ShaderBindings, Density, GetHairDensity(VFInput));
	VFS_BindParam(ShaderBindings, TranslucencyDensity, GetTranslucencyDensity(VFInput));
	VFS_BindParam(ShaderBindings, StableRasterization, UseStableRasterization(VFInput) ? 1u : 0u);
	VFS_BindParam(ShaderBindings, ScatterSceneLighing, UseScatterSceneLighting(VFInput) ? 1u : 0u);
	VFS_BindParam(ShaderBindings, RootUVBuffer, GetRootUVSRV(VFInput));
	VFS_BindParam(ShaderBindings, StrandsIDBuffer, GetStrandsIDSRV(VFInput));
	VFS_BindParam(ShaderBindings, NumVerticesPerStrand, GetNumVerticesPerStrand(VFInput));
	*/
	return TressFXVFUSP;
}

class FTressFXVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FTressFXVertexFactoryShaderParameters, NonVirtual);
public:
	/*
	LAYOUT_FIELD(FShaderParameter, Radius);
	LAYOUT_FIELD(FShaderParameter, RadiusAtDepth1_Primary);	// unused
	LAYOUT_FIELD(FShaderParameter, RadiusAtDepth1_Velocity);	// unused
	LAYOUT_FIELD(FShaderParameter, Density);
	LAYOUT_FIELD(FShaderParameter, TranslucencyDensity);
	LAYOUT_FIELD(FShaderParameter, Culling);
	LAYOUT_FIELD(FShaderParameter, NumVerticesPerStrand);
	LAYOUT_FIELD(FShaderParameter, StableRasterization);
	LAYOUT_FIELD(FShaderParameter, ScatterSceneLighing);

	LAYOUT_FIELD(FShaderResourceParameter, PositionBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, PreviousPositionBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, LengthBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, TangentBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, RootUVBuffer);
	LAYOUT_FIELD(FShaderResourceParameter, StrandsIDBuffer);
	*/

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		/*Radius.Bind(ParameterMap, TEXT("TressFXVF_Radius"));
		Density.Bind(ParameterMap, TEXT("TressFXVF_Density"));
		TranslucencyDensity.Bind(ParameterMap, TEXT("TressFXVF_TranslucencyDensity"));
		Culling.Bind(ParameterMap, TEXT("TressFXVF_CullingEnable"));
		NumVerticesPerStrand.Bind(ParameterMap, TEXT("TressFXVF_NumVerticesPerStrand"));
		StableRasterization.Bind(ParameterMap, TEXT("TressFXVF_bUseStableRasterization"));
		ScatterSceneLighing.Bind(ParameterMap, TEXT("TressFXVF_bScatterSceneLighing"));

		PositionBuffer.Bind(ParameterMap, TEXT("TressFXVF_PositionBuffer"));
		PreviousPositionBuffer.Bind(ParameterMap, TEXT("TressFXVF_PreviousPositionBuffer"));
		LengthBuffer.Bind(ParameterMap, TEXT("TressFXVF_LengthBuffer"));
		TangentBuffer.Bind(ParameterMap, TEXT("TressFXVF_TangentBuffer"));
		RootUVBuffer.Bind(ParameterMap, TEXT("TressFXVF_RootUVBuffer"));
		StrandsIDBuffer.Bind(ParameterMap, TEXT("TressFXVF_StrandsIDBuffer"));
		*/
	}

	void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
	) const
	{
		const FTressFXVertexFactory* VF = static_cast<const FTressFXVertexFactory*>(VertexFactory);

		//const FTressFXGroupPublicData* GroupPublicData = reinterpret_cast<const FTressFXGroupPublicData*>(BatchElement.VertexFactoryUserData);
		//check(GroupPublicData);
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FTressFXVertexFactoryUniformShaderParameters>(), VF->Data.Instance->Strands.UniformBuffer);

	}
};

IMPLEMENT_TYPE_LAYOUT(FTressFXVertexFactoryShaderParameters);

bool FTressFXVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	return Parameters.MaterialParameters.MaterialDomain == MD_Surface
		&& Parameters.MaterialParameters.ShadingModels.HasShadingModel(MSM_Hair)
		&& IsTressFXSupported(ETressFXShaderType::Strands, Parameters.Platform);
}

void FTressFXVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	const bool bUseGPUSceneAndPrimitiveIdStream = Parameters.VertexFactoryType->SupportsPrimitiveIdStream() && UseGPUScene(Parameters.Platform, GetMaxSupportedFeatureLevel(Parameters.Platform));
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), bUseGPUSceneAndPrimitiveIdStream);
	OutEnvironment.SetDefine(TEXT("VF_STRAND_TRESSFX"), TEXT("1"));
	//OutEnvironment.SetDefine(TEXT("VF_GPU_SCENE_TEXTURE"), bUseGPUSceneAndPrimitiveIdStream&& GPUSceneUseTexture2D(Parameters.Platform));
}

void FTressFXVertexFactory::ValidateCompiledResult(const FVertexFactoryType* Type, EShaderPlatform Platform, const FShaderParameterMap& ParameterMap, TArray<FString>& OutErrors)
{
	if (Type->SupportsPrimitiveIdStream()
		&& UseGPUScene(Platform, GetMaxSupportedFeatureLevel(Platform))
		&& !IsMobilePlatform(Platform) // On mobile VS may use PrimtiveUB while GPUScene is enabled
		&& ParameterMap.ContainsParameterAllocation(FPrimitiveUniformShaderParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName()))
	{
		OutErrors.AddUnique(*FString::Printf(TEXT("Shader attempted to bind the Primitive uniform buffer even though Vertex Factory %s computes a PrimitiveId per-instance.  This will break auto-instancing.  Shaders should use GetPrimitiveData(PrimitiveId).Member instead of Primitive.Member."), Type->GetName()));
	}
}

void FTressFXVertexFactory::SetData(const FDataType& InData, FRHICommandListBase& RHICmdList)
{
	check(IsInRenderingThread());
	Data = InData;
	UpdateRHI(RHICmdList);
}

void FTressFXVertexFactory::InitRHI(FRHICommandListBase& RHICmdList)
{
	bNeedsDeclaration = false;
	//bSupportsManualVertexFetch = true;

	// We create different streams based on feature level
	check(HasValidFeatureLevel());

	// VertexFactory needs to be able to support max possible shader platform and feature level
	// in case if we switch feature level at runtime.
	const bool bCanUseGPUScene = UseGPUScene(GMaxRHIShaderPlatform, GMaxRHIFeatureLevel);

	FVertexDeclarationElementList Elements;
	SetPrimitiveIdStreamIndex(GetFeatureLevel(),EVertexInputStreamType::Default, -1);
	if (GetType()->SupportsPrimitiveIdStream() && bCanUseGPUScene)
	{
		// When the VF is used for rendering in normal mesh passes, this vertex buffer and offset will be overridden
		Elements.Add(AccessStreamComponent(FVertexStreamComponent(&GPrimitiveIdDummy, 0, 0, sizeof(uint32), VET_UInt, EVertexStreamUsage::Instancing), 13));
		SetPrimitiveIdStreamIndex(GetFeatureLevel(),EVertexInputStreamType::Default, Elements.Last().StreamIndex);
		bNeedsDeclaration = true;
	}

	check(Streams.Num() > 0);

	InitDeclaration(Elements);
	//check(IsValidRef(GetDeclaration()));

	// create the buffer
	FTressFXVertexFactoryUniformShaderParameters Parameters = Data.Instance->GetTressFXUniformShaderParameters();
	Data.Instance->Strands.UniformBuffer = FTressFXUniformBuffer::CreateUniformBufferImmediate(Parameters, UniformBuffer_MultiFrame);

}

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FTressFXVertexFactory, SF_Vertex, FTressFXVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FTressFXVertexFactory, SF_Pixel, FTressFXVertexFactoryShaderParameters);


void FTressFXVertexFactory::ReleaseRHI()
{
	FVertexFactory::ReleaseRHI();
}


//IMPLEMENT_VERTEX_FACTORY_TYPE(FTressFXVertexFactory, TEXT("/Engine/Private/TressFX/TressFXVertexFactory.ush"), true, false, true, true, true, true, true);

IMPLEMENT_VERTEX_FACTORY_TYPE(FTressFXVertexFactory, "/Engine/Private/TressFX/TressFXVertexFactory.ush",
	EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsCachingMeshDrawCommands
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsRayTracing
);
