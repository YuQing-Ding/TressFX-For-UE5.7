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
#include "TressFXGeometryCache.h"
#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "TressFXInterface.h"
#include "RenderGraphResources.h"
#include "RenderGraphBuilder.h"
#include "Shader.h"
#include "GPUSkinCache.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "CommonRenderResources.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshLODRenderData.h"




static void BuildBoneMatrices(USkeletalMeshComponent* SkeletalMeshComponent, const FSkeletalMeshLODRenderData& LODData,
	const uint32 LODIndex, TArray<uint32>& MatrixOffsets, TArray<FVector4f>& BoneMatrices)
{
	TArray<FMatrix44f> BoneTransforms;
	//SkeletalMeshComponent->GetCurrentRefToLocalMatrices(BoneTransforms, LODIndex);
	BoneTransforms = SkeletalMeshComponent->MeshObject->GetReferenceToLocalMatrices();

	MatrixOffsets.SetNum(LODData.GetNumVertices());
	uint32 BonesOffset = 0;
	for (int32 SectionIdx = 0; SectionIdx < LODData.RenderSections.Num(); ++SectionIdx)
	{
		const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIdx];
		for (uint32 SectionVertex = 0; SectionVertex < Section.NumVertices; ++SectionVertex)
		{
			MatrixOffsets[Section.BaseVertexIndex + SectionVertex] = BonesOffset;
		}
		BonesOffset += Section.BoneMap.Num();
	}
	BoneMatrices.SetNum(BonesOffset * 3);
	BonesOffset = 0;
	for (int32 SectionIdx = 0; SectionIdx < LODData.RenderSections.Num(); ++SectionIdx)
	{
		const FSkelMeshRenderSection& Section = LODData.RenderSections[SectionIdx];
		for (int32 BoneIdx = 0; BoneIdx < Section.BoneMap.Num(); ++BoneIdx, ++BonesOffset)
		{
			BoneTransforms[Section.BoneMap[BoneIdx]].To3x4MatrixTranspose(&BoneMatrices[3 * BonesOffset].X);
		}
	}
}

class FSkinUpdateTFXCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSkinUpdateTFXCS);
	SHADER_USE_PARAMETER_STRUCT(FSkinUpdateTFXCS, FGlobalShader);

	class FUnlimitedBoneInfluence : SHADER_PERMUTATION_INT("GPUSKIN_UNLIMITED_BONE_INFLUENCE", 2);
	class FUseExtraInfluence : SHADER_PERMUTATION_INT("GPUSKIN_USE_EXTRA_INFLUENCES", 2);
	class FIndexUint16 : SHADER_PERMUTATION_INT("GPUSKIN_BONE_INDEX_UINT16", 2);
	class FUpdateExternalDeformedBuffer : SHADER_PERMUTATION_INT("UPDATE_EXTERNAL_DEFORMED_BUFFER", 2);
	using FPermutationDomain = TShaderPermutationDomain<FUnlimitedBoneInfluence, FUseExtraInfluence, FIndexUint16, FUpdateExternalDeformedBuffer>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, IndexSize)
		SHADER_PARAMETER(uint32, NumVertices)
		SHADER_PARAMETER(uint32, WeightStride)
		SHADER_PARAMETER(FMatrix44f, CompToWorld)
		SHADER_PARAMETER(uint32, MorphBufferOffset)
		SHADER_PARAMETER_UAV(RWBuffer<float>, MorphBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, GuidesLocalToGlobalVertexIndexMapBuffer)
		SHADER_PARAMETER_SRV(Buffer<uint>, WeightLookup)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, BoneMatrices)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MatrixOffsets)
		SHADER_PARAMETER_SRV(Buffer<uint>, VertexWeights)
		SHADER_PARAMETER_SRV(Buffer<float>, RestPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, DeformedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float3>, ExternalDeformedPositions)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsTressFXSupported(ETressFXShaderType::All, Parameters.Platform); }
};

IMPLEMENT_GLOBAL_SHADER(FSkinUpdateTFXCS, "/Engine/Private/TressFX/TressFXSkinUpdate.usf", "UpdateSkinPositionCS", SF_Compute);

void AddSkinUpdatePass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FTressFXGroupInstance* Instance,
	FSkinWeightVertexBuffer* SkinWeight,
	FSkeletalMeshLODRenderData& RenderData,
	FRHIUnorderedAccessView* MorphBufferUAV,
	FRDGBufferRef BoneMatrices,
	FRDGBufferRef MatrixOffsets,
	const FMatrix44f& CompToWorld,
	FRDGBufferRef OutDeformedPosition,
#if WITH_EDITOR
	FRDGImportedBufferTFX& OutExternalDeformedPosition,
#endif
	const FRDGBufferSRVRef InGuidesLocalToGlobalVertexIndexMapBuffer)
{
	FSkinUpdateTFXCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSkinUpdateTFXCS::FParameters>();

	Parameters->IndexSize = SkinWeight->GetBoneIndexByteSize();
	Parameters->NumVertices = Instance->Guides.GuidesBindingResource->GetBindingVertexCount();// RenderData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
	Parameters->WeightStride = SkinWeight->GetConstantInfluencesVertexStride();
	Parameters->CompToWorld = CompToWorld;
	Parameters->MorphBuffer = MorphBufferUAV;
	Parameters->GuidesLocalToGlobalVertexIndexMapBuffer = InGuidesLocalToGlobalVertexIndexMapBuffer;
	Parameters->WeightLookup = SkinWeight->GetLookupVertexBuffer()->GetSRV();
	Parameters->BoneMatrices = GraphBuilder.CreateSRV(BoneMatrices);//BoneBuffer.VertexBufferSRV;
	Parameters->MatrixOffsets = GraphBuilder.CreateSRV(MatrixOffsets);//BoneBuffer.VertexBufferSRV;
	Parameters->VertexWeights = SkinWeight->GetDataVertexBuffer()->GetSRV();
	Parameters->RestPositions = RenderData.StaticVertexBuffers.PositionVertexBuffer.GetSRV();
	Parameters->DeformedPositions = GraphBuilder.CreateUAV(OutDeformedPosition, PF_R32_FLOAT);
#if WITH_EDITOR
	Parameters->ExternalDeformedPositions = OutExternalDeformedPosition.UAV;
#endif

	FSkinUpdateTFXCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FSkinUpdateTFXCS::FUnlimitedBoneInfluence>(SkinWeight->GetBoneInfluenceType() == GPUSkinBoneInfluenceType::UnlimitedBoneInfluence);
	PermutationVector.Set<FSkinUpdateTFXCS::FUseExtraInfluence>(SkinWeight->GetMaxBoneInfluences() > MAX_INFLUENCES_PER_STREAM);
	PermutationVector.Set<FSkinUpdateTFXCS::FIndexUint16>(SkinWeight->Use16BitBoneIndex());
#if WITH_EDITOR
	PermutationVector.Set<FSkinUpdateTFXCS::FUpdateExternalDeformedBuffer>(1);
#else
	PermutationVector.Set<FSkinUpdateTFXCS::FUpdateExternalDeformedBuffer>(0);
#endif

	const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(Instance->Guides.GuidesBindingResource->GetBindingVertexCount(), 64);
	check(DispatchGroupCount.X < 65536);
	TShaderMapRef<FSkinUpdateTFXCS> ComputeShader(ShaderMap, PermutationVector);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("UpdateSkinPosition"),
		ComputeShader,
		Parameters,
		DispatchGroupCount);
}


void BuildCacheGeometry(
	FRDGBuilder& GraphBuilder,
	const FSceneView* View,
	FGlobalShaderMap* ShaderMap,
	FTressFXGroupInstance* Instance,
	FCachedGeometry& CachedGeometry,
	const FRDGBufferSRVRef GuidesLocalToGlobalVertexIndexMapBuffer)
{

	if (Instance->SkeletalComponent)
	{
		if (Instance->Guides.GuidesBindingResource->GetBindingVertexCount() == 0)
			return;

		FSkeletalMeshRenderData* RenderData = Instance->SkeletalComponent->GetSkinnedAsset()->GetResourceForRendering();

		const uint32 LODIndex = Instance->SkeletalComponent->GetPredictedLODLevel();// RenderData->PendingFirstLODIdx;
		FSkeletalMeshLODRenderData& LODData = RenderData->LODRenderData[LODIndex];

		TArray<uint32> MatrixOffsets;
		TArray<FVector4f> BoneMatrices;
		BuildBoneMatrices(Instance->SkeletalComponent, LODData, LODIndex, MatrixOffsets, BoneMatrices);

		FRDGBufferRef DeformedPositionsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), Instance->Guides.GuidesBindingResource->GetBindingVertexCount() * 3), TEXT("TressFX.BindingDeformedPositions"));
		FRDGBufferRef BoneMatricesBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("TressFX.SkinnedBoneMatrices"), sizeof(float) * 4, BoneMatrices.Num(), BoneMatrices.GetData(), sizeof(float) * 4 * BoneMatrices.Num());
		FRDGBufferRef MatrixOffsetsBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("TressFX.SkinnedMatrixOffsets"), sizeof(uint32), MatrixOffsets.Num(), MatrixOffsets.GetData(), sizeof(uint32) * MatrixOffsets.Num());

#if WITH_EDITOR
		FRDGImportedBufferTFX ExternalDeformedPositionBuffer = Register(GraphBuilder,
			Instance->MTMeshResource->MTMeshDeformedPositionBuffer,
			ERDGImportedBufferFlagsTFX::CreateViews);
#endif

		const FMatrix CompToWorld4d = Instance->SkeletalComponent->GetComponentToWorld().ToMatrixWithScale();
		FMatrix44f CompToWorld;
		for (uint32 i = 0; i < 4; ++i)
			for (uint32 j = 0; j < 4; ++j)
				CompToWorld.M[i][j] = CompToWorld4d.M[i][j];

		FRHIUnorderedAccessView* MorphBufferUAV = nullptr;

		if (!MorphBufferUAV)
			return;

		AddSkinUpdatePass(GraphBuilder, ShaderMap,
			Instance,
			Instance->SkeletalComponent->GetSkinWeightBuffer(LODIndex),
			LODData,
			MorphBufferUAV,
			BoneMatricesBuffer,
			MatrixOffsetsBuffer, 
			CompToWorld,
			DeformedPositionsBuffer, 
#if WITH_EDITOR
			ExternalDeformedPositionBuffer,
#endif
			GuidesLocalToGlobalVertexIndexMapBuffer);

		//CachedGeometry.DeformedPositionBuffer = DeformedPositionsBuffer;
		FRDGBufferSRVRef DeformedPositionSRV = GraphBuilder.CreateSRV(DeformedPositionsBuffer, PF_R32_FLOAT);

		//for (int32 SectionIdx = 0; SectionIdx < LODData.RenderSections.Num(); ++SectionIdx)
		{
			FCachedGeometry::Section CachedSection;

			CachedSection.RDGPositionBuffer = DeformedPositionSRV;
			
			CachedGeometry.Sections.Add(CachedSection);
		}

#if WITH_EDITOR

		Instance->MTMeshVertexCount = LODData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();

		Instance->MTMeshIndicesBufferRHI = LODData.MultiSizeIndexContainer.GetIndexBuffer()->IndexBufferRHI;
		Instance->MTMeshIndicesCount = Instance->MTMeshIndicesBufferRHI->GetSize() / Instance->MTMeshIndicesBufferRHI->GetStride();
#endif
	}

}
