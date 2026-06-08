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

#include "TressFXAsset.h"
#include "TressFXTFXFileHeader.h"
#include "Engine/SkeletalMesh.h"
#include "TressFXBuilder.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/AnimObjectVersion.h"
#include "TressFXComponent.h"


static void GetTangentVectors(const FVector4f& n, FVector4f& t0, FVector4f& t1)
{
	if (fabsf(n[2]) > 0.707f)
	{
		float a = n[1] * n[1] + n[2] * n[2];
		float k = 1.0f / sqrtf(a);
		t0[0] = 0;
		t0[1] = -n[2] * k;
		t0[2] = n[1] * k;

		t1[0] = a * k;
		t1[1] = -n[0] * t0[2];
		t1[2] = n[0] * t0[1];
	}
	else
	{
		float a = n[0] * n[0] + n[1] * n[1];
		float k = 1.0f / sqrtf(a);
		t0[0] = -n[1] * k;
		t0[1] = n[0] * k;
		t0[2] = 0;

		t1[0] = -n[2] * t0[1];
		t1[1] = n[2] * t0[0];
		t1[2] = a * k;
	}
}

static float GetRandom(float Min, float Max);

FTressFXAsset::FTressFXAsset()
{
	numTotalStrands = 0;
	numTotalVertices = 0;
	numVerticesPerStrand = 0;
	numGuideStrands = 0;
	numGuideVertices = 0;
	numFollowStrandsPerGuide = 0;
}

FTressFXAsset::~FTressFXAsset()
{
	Clear();
}

void FTressFXAsset::Clear()
{
	numTotalStrands = 0;
	numTotalVertices = 0;
	numVerticesPerStrand = 0;
	numGuideStrands = 0;
	numGuideVertices = 0;
	numFollowStrandsPerGuide = 0;

	positions.SetNum(0);
	strandUV.SetNum(0);
	tangents.SetNum(0);
	followRootOffsets.SetNum(0);
	strandTypes.SetNum(0);
	thicknessCoeffs.SetNum(0);
	restLengths.SetNum(0);
	triangleIndices.SetNum(0);
	boneSkinningData.SetNum(0);
}

// This generates follow hairs around loaded guide hairs procedually with random distribution within the max radius input. 
// Calling this is optional. 
bool FTressFXAsset::GenerateFollowHairs(int numFollowHairsPerGuideHair, float tipSeparationFactor, float maxRadiusAroundGuideHair)
{
	TRESSFX_ASSERT(numFollowHairsPerGuideHair >= 0);

	// Basic check for reasonable values, otherwise we can crash with out of memory error.

	const int numPossibleTotalStrands = numGuideStrands * (numFollowHairsPerGuideHair + 1);
	const int numPossibleTotalVerts = numPossibleTotalStrands * numVerticesPerStrand;

	const int maxPossibleVertices = 32 * 1000000;

	if (numPossibleTotalVerts > maxPossibleVertices)
	{
		UE_LOG(LogTemp, Error, TEXT("TressFXAsset: Too many possible strands/verts resulted (%d/%d)."), numPossibleTotalStrands, numPossibleTotalVerts);

		numFollowStrandsPerGuide = 0;
		return false;
	}

	numFollowStrandsPerGuide = numFollowHairsPerGuideHair;

	// Nothing to do, just exit. 
	if (numFollowHairsPerGuideHair == 0)
		return false;

	// Recompute total number of hair strands and vertices with considering number of follow hairs per a guide hair. 
	numTotalStrands = numGuideStrands * (numFollowStrandsPerGuide + 1);
	numTotalVertices = numTotalStrands * numVerticesPerStrand;

	// keep the old buffers until the end of this function. 
	TArray<FVector4f> positionsGuide = positions;
	TArray<FVector2f> strandUVGuide = strandUV;

	// re-allocate all buffers
	positions.SetNumUninitialized(numTotalVertices);
	strandUV.SetNumUninitialized(numTotalStrands);
	followRootOffsets.SetNumUninitialized(numTotalStrands);

	// Generate follow hairs
	for (int i = 0; i < numGuideStrands; i++)
	{
		int indexGuideStrand = i * (numFollowStrandsPerGuide + 1);
		int indexRootVertMaster = indexGuideStrand * numVerticesPerStrand;

		memcpy(&positions[indexRootVertMaster], &positionsGuide[i * numVerticesPerStrand], sizeof(FVector4) * numVerticesPerStrand);
		strandUV[indexGuideStrand] = strandUVGuide[i];

		followRootOffsets[indexGuideStrand].Set(0, 0, 0, (float)indexGuideStrand);
		FVector4f v01 = positions[indexRootVertMaster + 1] - positions[indexRootVertMaster];
		v01 = v01.GetUnsafeNormal3();

		// Find two orthogonal unit tangent vectors to v01
		FVector4f t0, t1;
		GetTangentVectors(v01, t0, t1);

		for (int j = 0; j < numFollowStrandsPerGuide; j++)
		{
			int indexStrandFollow = indexGuideStrand + j + 1;
			int indexRootVertFollow = indexStrandFollow * numVerticesPerStrand;

			strandUV[indexStrandFollow] = strandUV[indexGuideStrand];

			// offset vector from the guide strand's root vertex position
			FVector4f offset = GetRandom(-maxRadiusAroundGuideHair, maxRadiusAroundGuideHair) * t0 + GetRandom(-maxRadiusAroundGuideHair, maxRadiusAroundGuideHair) * t1;
			followRootOffsets[indexStrandFollow] = offset;
			followRootOffsets[indexStrandFollow].W = (float)indexGuideStrand;

			for (int k = 0; k < numVerticesPerStrand; k++)
			{
				const FVector4f& guideVert = positions[indexRootVertMaster + k];
				FVector4f& followVert = positions[indexRootVertFollow + k];

				float factor = tipSeparationFactor * ((float)k / ((float)numVerticesPerStrand)) + 1.0f;
				followVert = guideVert + offset * factor;
			}
		}
	}

	return true;
}

bool FTressFXAsset::LoadHairData(FMemoryReader ioObject)
{
	// Clear all data before loading an asset. 
	Clear();

	TressFXTFXFileHeader header;

	// read the header
	ioObject.Seek(0); // make sure the stream pos is at the beginning. 
	ioObject.Serialize((void*)&header, sizeof(TressFXTFXFileHeader));

	// If the tfx version is lower than the current major version, exit. 
	if (header.version < TRESSFX_MAJOR_VERSION)
	{
		return false;
	}

	numStrandsInFile = header.numHairStrands;

	// We make the number of strands be multiple of TRESSFX_SIM_THREAD_GROUP_SIZE. 
	numGuideStrands = (numStrandsInFile - numStrandsInFile % TRESSFX_SIM_THREAD_GROUP_SIZE) + TRESSFX_SIM_THREAD_GROUP_SIZE;

	numVerticesPerStrand = header.numVerticesPerStrand;

	// Make sure number of vertices per strand is greater than two and less than or equal to
	// thread group size (64). Also thread group size should be a mulitple of number of
	// vertices per strand. So possible number is 4, 8, 16, 32 and 64.
	TRESSFX_ASSERT(numVerticesPerStrand > 2 && numVerticesPerStrand <= TRESSFX_SIM_THREAD_GROUP_SIZE && TRESSFX_SIM_THREAD_GROUP_SIZE % numVerticesPerStrand == 0);

	numFollowStrandsPerGuide = 0;
	numTotalStrands = numGuideStrands; // Until we call GenerateFollowHairs, the number of total strands is equal to the number of guide strands. 
	numGuideVertices = numGuideStrands * numVerticesPerStrand;
	numTotalVertices = numGuideVertices; // Again, the total number of vertices is equal to the number of guide vertices here. 

	TRESSFX_ASSERT(numTotalVertices % TRESSFX_SIM_THREAD_GROUP_SIZE == 0); // number of total vertices should be multiple of thread group size. 
																		   // This assert is actually redundant because we already made numGuideStrands
																		   // and numTotalStrands are multiple of thread group size. 
																		   // Just demonstrating the requirement for number of vertices here in case 
																		   // you are to make your own loader. 

	positions.SetNumUninitialized(numTotalVertices);

	// Read position data from the io stream. 
	ioObject.Seek(header.offsetVertexPosition);
	ioObject.Serialize((void*)positions.GetData(), numStrandsInFile * numVerticesPerStrand * sizeof(float4));	// to generate follow hairs, positions will be re-allocated. 
	
	// We need to make up some strands to fill up the buffer because the number of strands from stream is not necessarily multile of thread size. 
	int32 numStrandsToMakeUp = numGuideStrands - numStrandsInFile;

	for (int32 i = 0; i < numStrandsToMakeUp; ++i)
	{
		for (int32 j = 0; j < numVerticesPerStrand; ++j)
		{
			int32 indexLastVertex = (numStrandsInFile - 1) * numVerticesPerStrand + j;
			int32 indexVertex = (numStrandsInFile + i) * numVerticesPerStrand + j;
			positions[indexVertex] = positions[indexLastVertex];
		}
	}

	// Read strand UVs
	ioObject.Seek(header.offsetStrandUV);
	strandUV.SetNumUninitialized(numTotalStrands);

	ioObject.Serialize((void*)strandUV.GetData(), numStrandsInFile * sizeof(float2));

	// Fill up the last empty space
	int32 indexLastStrand = (numStrandsInFile - 1);

	for (int32 i = 0; i < numStrandsToMakeUp; ++i)
	{
		int32 indexStrand = (numStrandsInFile + i);
		strandUV[indexStrand] = strandUV[indexLastStrand];
	}

	// fills root offsets with zeros
	followRootOffsets.SetNumZeroed(numTotalStrands);
	return true;
}

bool FTressFXAsset::ProcessAsset()
{
	strandTypes.SetNumUninitialized(numTotalStrands);
	tangents.SetNumUninitialized(numTotalVertices);
	restLengths.SetNumUninitialized(numTotalVertices);
	thicknessCoeffs.SetNumUninitialized(numTotalVertices);
	triangleIndices.SetNumUninitialized(GetNumHairTriangleIndices());

	// compute tangent vectors
	ComputeStrandTangent();

	// compute thickness coefficients
	ComputeThicknessCoeffs();

	// compute rest lengths
	ComputeRestLengths();

	// triangle index
	FillTriangleIndexArray();

	for (int i = 0; i < numTotalStrands; i++)
		strandTypes[i] = 0;

	return true;
}

void FTressFXAsset::FillTriangleIndexArray()
{
	TRESSFX_ASSERT(numTotalVertices == numTotalStrands * numVerticesPerStrand);
	TRESSFX_ASSERT(triangleIndices.Num() != 0);

	uint32 id = 0;
	int iCount = 0;

	for (int i = 0; i < numTotalStrands; i++)
	{
		for (int j = 0; j < numVerticesPerStrand - 1; j++)
		{
			int strandIndex = id / numVerticesPerStrand;
			int indexInStrand = id % numVerticesPerStrand;
			strandIndex = (strandIndex % numGuideStrands) * (numFollowStrandsPerGuide + 1) + strandIndex / numGuideStrands;
			int vertexId = strandIndex * numVerticesPerStrand + indexInStrand;

			triangleIndices[iCount++] = 2 * vertexId;
			triangleIndices[iCount++] = 2 * vertexId + 1;
			triangleIndices[iCount++] = 2 * vertexId + 2;
			triangleIndices[iCount++] = 2 * vertexId + 2;
			triangleIndices[iCount++] = 2 * vertexId + 1;
			triangleIndices[iCount++] = 2 * vertexId + 3;

			id++;
		}

		id++;
	}

	TRESSFX_ASSERT(iCount == 6 * numTotalStrands * (numVerticesPerStrand - 1)); // iCount == GetNumHairTriangleIndices()
	TRESSFX_ASSERT(id == numTotalStrands * numVerticesPerStrand);
}

void FTressFXAsset::ComputeStrandTangent()
{

	for (int iStrand = 0; iStrand < numTotalStrands; ++iStrand)
	{
		int indexRootVertMaster = iStrand * numVerticesPerStrand;
		// vertex 0
		{
			FVector4f& vert_0 = positions[indexRootVertMaster];
			FVector4f& vert_1 = positions[indexRootVertMaster + 1];

			FVector4f tangent = vert_1 - vert_0;
			tangents[indexRootVertMaster] = tangent.GetSafeNormal();
		}

		// vertex 1 through n-2
		for (int i = 1; i < (int)numVerticesPerStrand - 1; i++)
		{
			FVector4f& vert_i_minus_1 = positions[indexRootVertMaster + i - 1];
			FVector4f& vert_i = positions[indexRootVertMaster + i];
			FVector4f& vert_i_plus_1 = positions[indexRootVertMaster + i + 1];

			FVector4f tangent_pre = vert_i - vert_i_minus_1;
			FVector4f tangent_next = vert_i_plus_1 - vert_i;

			FVector4f tangent = tangent_pre.GetSafeNormal() + tangent_next.GetSafeNormal();
			tangents[indexRootVertMaster + i] = tangent.GetSafeNormal();
		}
		// vertex n-1
		{
			FVector4f& vert_0 = positions[indexRootVertMaster + numVerticesPerStrand - 2];
			FVector4f& vert_1 = positions[indexRootVertMaster + numVerticesPerStrand - 1];

			FVector4f tangent = vert_1 - vert_0;
			tangents[indexRootVertMaster + numVerticesPerStrand - 1] = tangent.GetSafeNormal();
		}
	}
}

void FTressFXAsset::ComputeThicknessCoeffs()
{
	int index = 0;

	for (int iStrand = 0; iStrand < numTotalStrands; ++iStrand)
	{
		int   indexRootVertMaster = iStrand * numVerticesPerStrand;
		float strandLength = 0;
		float tVal = 0;

		// vertex 1 through n
		for (int i = 1; i < (int)numVerticesPerStrand; ++i)
		{
			FVector4f& vert_i_minus_1 = positions[indexRootVertMaster + i - 1];
			FVector4f& vert_i = positions[indexRootVertMaster + i];

			FVector4f vec = vert_i - vert_i_minus_1;
			float        disSeg = vec.Size3();

			tVal += disSeg;
			strandLength += disSeg;
		}

		for (int i = 0; i < (int)numVerticesPerStrand; ++i)
		{
			tVal /= strandLength;
			thicknessCoeffs[index++] = sqrt(1.f - tVal * tVal);
		}
	}
}

void FTressFXAsset::ComputeRestLengths()
{

	int index = 0;

	// Calculate rest lengths
	for (int i = 0; i < numTotalStrands; i++)
	{
		int indexRootVert = i * numVerticesPerStrand;

		for (int j = 0; j < numVerticesPerStrand - 1; ++j,++index)
		{
			restLengths[index] = (positions[indexRootVert + j] - positions[indexRootVert + j + 1]).Size3();
			maxRestLength = restLengths[index] > maxRestLength ? restLengths[index] : maxRestLength;
		}

		// Since number of edges are one less than number of vertices in hair strand, below
		// line acts as a placeholder.
		restLengths[index++] = 0;
	}
}

bool FTressFXAsset::LoadBoneData(const TressFXSkeletonInterface& skeletonData, FMemoryReader ioObject)
{
	return true;
}


bool FTressFXAsset::LoadBoneData(const USkeletalMesh& skeletonData, FMemoryReader ioObject)
{
	//AMD_SAFE_FREE(boneSkinningData);

	int32 numOfBones = 0;
	ioObject.Seek(0);
	ioObject.Serialize((void*)&numOfBones, sizeof(int32));
	if (numOfBones < 0)
	{
		return false;
	}

	size_t startOfBoneNames = (sizeof(int32) * numOfBones);
	size_t currentNamePosition = startOfBoneNames;
	size_t currentOffSetPosition = 0;

	TArray<FString> boneNames;
	boneNames.Init(FString(), numOfBones);// Reserve(numOfBones);

	for (int i = 0; i < numOfBones; i++)
	{
		int32 index = 0;
		ioObject.Serialize((char*)&index, sizeof(int32));

		int32 charLen = 0;
		ioObject.Serialize((char*)&charLen, sizeof(int32)); // character length includes null termination already.
		if (charLen <= 0 || ioObject.TotalSize() - ioObject.Tell() < charLen)
		{
			return false;
		}

		char* tmp = static_cast<char*>(FMemory::Malloc(charLen * sizeof(char)));
		ioObject.Serialize(tmp, charLen);

		boneNames[i] = FString(tmp);
		FMemory::Free(tmp);

		//EI_Read(ANSI_TO_TCHAR(*boneNames[i]), sizeof(char) * charLen, ioObject);
	}

	// Reading the number of strands 
	int32 numOfStrandsInStream = 0;
	ioObject.Serialize((char*)&numOfStrandsInStream, sizeof(int32));
	if (numOfStrandsInStream < 0)
	{
		return false;
	}

	// If the number of strands from the input stream (tfxbone) is bigger than what we already know from tfx, something is wrong. 
	if (numGuideStrands < numOfStrandsInStream)
	    return 0;

	boneIndexData.SetNumZeroed(numGuideStrands);

	for (int32 i = 0; i < numOfStrandsInStream; ++i)
	{
		int32 index = 0; // Well, we don't really use this here. 
		ioObject.Serialize((char*)&index, sizeof(int32));

		int32 count = 0; // Well, we don't really use this here. 
		ioObject.Serialize((char*)&count, sizeof(int32));

		FTressFXBoneIndexData idxData;
		idxData.StartIdx = boneSkinningData.Num();

		for (int32 j = 0; j < count; ++j)
		{
			FTressFXBoneSkinningData skinData;

			int32 boneIndex;
			ioObject.Serialize((char*)&boneIndex, sizeof(int32));
			skinData.IndexBone = boneIndex;

			int32 BoneIndex = INDEX_NONE;
			FName BoneName = TCHAR_TO_ANSI(*boneNames[(int)skinData.IndexBone]);
			if (BoneName != NAME_None && IsValid(&skeletonData))
			{
				BoneIndex = skeletonData.GetRefSkeleton().FindBoneIndex(BoneName);
			}
			skinData.IndexBone = BoneIndex; // Change the joint index to be what the engine wants

			ioObject.Serialize((char*)&skinData.Weight, sizeof(float));


			// If bone index is -1, then it means that there is no bone associated to this. In this case we simply replace it with zero.
			// This is safe because the corresonding weight should be zero anyway.
			if (-1 == skinData.IndexBone || skinData.Weight == 0)
				continue;
			
			idxData.BoneCount++;
			boneSkinningData.Add(skinData);
		}

		boneIndexData[i] = idxData;
	}
	
	return true;
}


UTressFXAsset::UTressFXAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsInitialized = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Strands_DefaultMaterialRef(TEXT("/TressFX/Materials/HairDefaultMaterial.HairDefaultMaterial"));
	Strands_DefaultMaterial = Strands_DefaultMaterialRef.Object;

}

int32 UTressFXAsset::GetNumTressFXGroups() const
{
	return TressFXGroupsData.Num();
}

FArchive& operator<<(FArchive& Ar, FTressFXGroupData& GroupData)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);

	FTressFXGroupData NoStrandsData;
	if (!Ar.IsCooking())// || !GroupData.bIsCookedOut)
	{
		GroupData.TFXData.Serialize(Ar);
		GroupData.ClusterCullingData.Serialize(Ar);

	}
	else
	{
		GroupData.TFXData.Serialize(Ar);
		GroupData.ClusterCullingData.Serialize(Ar);
	}

	return Ar;
}

template<typename T>
inline void InternalReleaseResource(T*& Resource)
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

void UTressFXAsset::InitGuideResources()
{
	for (FTressFXGroupData& GroupData : TressFXGroupsData)
	{
		if (GroupData.HasValidData())
		{
			GroupData.GuidesRestResource = new FTressFXGuidesRestResource(GroupData.TFXData);
			BeginInitResource(GroupData.GuidesRestResource);
		}
	}
}

void UTressFXAsset::InitStrandsResources()
{

}

void UTressFXAsset::InitResources()
{
	InitGuideResources();
	InitStrandsResources();

	bIsInitialized = true;
}

void UTressFXAsset::ReleaseResource()
{
	bIsInitialized = false;
	for (uint32 GroupIndex = 0, GroupCount = GetNumTressFXGroups(); GroupIndex < GroupCount; ++GroupIndex)
	{
		FTressFXGroupData& GroupData = TressFXGroupsData[GroupIndex];

		if (GroupData.HasValidData())
		{
			InternalReleaseResource(GroupData.GuidesRestResource);
		}

	}

}

void UTressFXAsset::BeginDestroy()
{
	ReleaseResource();
	Super::BeginDestroy();
}

#if WITH_EDITOR

void UTressFXAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	{
		FTressFXComponentRecreateRenderStateContext Context(this);
		TressFXGroupsRendering.NumStrands = (1 + TressFXGroupsRendering.NumFollowStrands) * TressFXGroupsRendering.NumGuides;
		TressFXGroupsRendering.NumStrandsVertices = TressFXGroupsRendering.NumStrands* TressFXGroupsRendering.NumVerticesPerStrand;
	}

}

#endif

bool UTressFXAsset::CacheStrandsData(uint32 GroupIndex, FProcessedTressFXDescription& ProcessedHairDescription, FString& OutDerivedDataKey)
{
	{
		if (!IsInGameThread())
		{
			// Strands build might actually be thread safe, but retry on the game thread to be safe
			bRetryLoadFromGameThread = true;
			return false;
		}

		bool bSuccess = true;
		FTressFXGroupData& TressFXGroupData = TressFXGroupsData[GroupIndex];
		if (bSuccess)
		{
			// Build cluster data
			FTressFXBuilder::BuildClusterData(this, ProcessedHairDescription, GroupIndex);

		}
	}

	return true;
}

bool UTressFXAsset::CacheDerivedData(uint32 GroupIndex, FProcessedTressFXDescription& ProcessedHairDescription)
{

	const uint32 GroupCount = 1;// TressFXGroupsInterpolation.Num();
	check(GroupIndex < GroupCount);
	if (GroupIndex >= GroupCount)
	{
		return false;
	}

	FString DerivedDataKey;
	bool bSuccess = CacheStrandsData(GroupIndex, ProcessedHairDescription, DerivedDataKey);


	return true;
}

bool UTressFXAsset::CacheDerivedDatas()
{
	const bool bIsGameThread = IsInGameThread();

	FProcessedTressFXDescription ProcessedHairDescription;
	const uint32 GroupCount = 1;// TressFXGroupsInterpolation.Num();
	for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		bool bSucceed = CacheDerivedData(GroupIndex, ProcessedHairDescription);
		if (!bSucceed)
			return false;
	}

	if (bIsGameThread)
	{
		InitResources();
	}

	return true;
}

void UTressFXAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	//
	{
		// Old format serialized the computed tressfx data directly
		Ar << TressFXGroupsData;
	}
}

void UTressFXAsset::PostLoad()
{
	//LLM_SCOPE(ELLMTag::Meshes) // This should be a Groom LLM tag, but there is no LLM tag bit left

	Super::PostLoad();

	CacheDerivedDatas();


}

int32 UTressFXAsset::GetMaterialIndex(FName MaterialSlotName) const
{
	const int32 SlotCount = 1;// TressFXGroupsMaterials.Num();
	for (int32 SlotIt = 0; SlotIt < SlotCount; ++SlotIt)
	{
		if (TressFXGroupsMaterials.SlotName == MaterialSlotName)
		{
			return SlotIt;
		}
	}

	return INDEX_NONE;
}

bool UTressFXAsset::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	return GetMaterialIndex(MaterialSlotName) != INDEX_NONE;
}

TArray<FName> UTressFXAsset::GetMaterialSlotNames() const
{
	TArray<FName> MaterialNames;
	//for (const FTressFXGroupsMaterial& Material : TressFXGroupsMaterials)
	{
		MaterialNames.Add(TressFXGroupsMaterials.SlotName);
	}

	return MaterialNames;
}

template<typename T>
bool InternalIsMaterialUsed(const TArray<T>& Groups, const FName& MaterialSlotName)
{
	bool bNeedSaving = false;
	for (const T& Group : Groups)
	{
		if (Group.MaterialSlotName == MaterialSlotName)
		{
			return true;
		}
	}

	return false;
}

template<typename T>
bool InternalIsMaterialUsed(const T& Group, const FName& MaterialSlotName)
{
	bool bNeedSaving = false;
	//for (const T& Group : Groups)
	{
		if (Group.MaterialSlotName == MaterialSlotName)
		{
			return true;
		}
	}

	return false;
}

bool UTressFXAsset::IsMaterialUsed(int32 MaterialIndex) const
{
	if (MaterialIndex < 0)// || MaterialIndex >= TressFXGroupsMaterials.Num())
		return false;

	const FName MaterialSlotName = TressFXGroupsMaterials.SlotName;
	return
		InternalIsMaterialUsed(TressFXGroupsRendering, MaterialSlotName);
}
