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
#include "TressFXImporter.h"
#include "TressFXImportOptions.h"
#include "TressFXMeshImportOptions.h"
#include "TressFXAsset.h"
#include "TressFXMeshAsset.h"
#include "Misc/FileHelper.h"


DEFINE_LOG_CATEGORY_STATIC(LogTressFXImporter, Log, All);

FTressFXImportContext::FTressFXImportContext(UTressFXImportOptions* InImportOptions, UObject* InParent, UClass* InClass, FName InName, EObjectFlags InFlags)
	: ImportOptions(InImportOptions)
	, Parent(InParent)
	, Class(InClass)
	, Name(InName)
	, Flags(InFlags)
{
}

FTressFXMeshImportContext::FTressFXMeshImportContext(UTressFXMeshImportOptions* InImportOptions, UObject* InParent, UClass* InClass, FName InName, EObjectFlags InFlags)
	: ImportOptions(InImportOptions)
	, Parent(InParent)
	, Class(InClass)
	, Name(InName)
	, Flags(InFlags)
{
}

UTressFXAsset* FTressFXImporter::ImportTFX(const FTressFXImportContext& ImportContext, FTressFXDescription& HairDescription, const FString& FilePath, UTressFXAsset* ExistingHair)
{
	FTressFXAsset* Asset = new FTressFXAsset();
	bool AssetLoaded = false;

	TArray<uint8> TheBinaryArray;
	if (FFileHelper::LoadFileToArray(TheBinaryArray, *FilePath))
	{
		if (TheBinaryArray.Num())
		{
			FMemoryReader FromBinary = FMemoryReader(TheBinaryArray, true);
			AssetLoaded = Asset->LoadHairData(FromBinary);
			if (AssetLoaded)
			{
				Asset->ProcessAsset();
			}
		}
	}

	FString BoneFilePath;
	BoneFilePath = FilePath + TEXT("bone");
	TArray<uint8> TheBoneBinaryArray;
	bool bBoneDataLoaded = false;
	if (FFileHelper::LoadFileToArray(TheBoneBinaryArray, *BoneFilePath))
	{
		if (TheBoneBinaryArray.Num())
		{
			if (ImportContext.ImportOptions && ImportContext.ImportOptions->Skeleton)
			{
				FMemoryReader FromBinary = FMemoryReader(TheBoneBinaryArray, true);
				bBoneDataLoaded = Asset->LoadBoneData(*ImportContext.ImportOptions->Skeleton, FromBinary);
			}
			else
			{
				UE_LOG(LogTressFXImporter, Warning, TEXT("Found '%s', but no skeleton was selected. Importing '%s' without bone skinning data."), *BoneFilePath, *FilePath);
			}
		}
	}
	else
	{
		UE_LOG(LogTressFXImporter, Display, TEXT("No TressFX bone file found at '%s'. Importing '%s' without bone skinning data."), *BoneFilePath, *FilePath);
	}

	UTressFXAsset* OutTressFXAsset = nullptr;

	if (ExistingHair)
		OutTressFXAsset = ExistingHair;
	else
		OutTressFXAsset = NewObject<UTressFXAsset>(ImportContext.Parent, ImportContext.Class, ImportContext.Name, ImportContext.Flags);

	OutTressFXAsset->TressFXGroupsData.SetNum(1);
	FTressFXDatas& TressFXDatas = OutTressFXAsset->TressFXGroupsData[0].TFXData;
	TressFXDatas.StrandsPoints.SetNum(Asset->numGuideVertices);
	TressFXDatas.StrandsCurves.SetNum(Asset->numGuideStrands);

	TressFXDatas.NumVerticesPerStrand = Asset->numVerticesPerStrand;
	TressFXDatas.NumStrandsToRender = Asset->numStrandsInFile;

	uint32 NumCurves = Asset->numGuideStrands;
	uint32 NumPoints = Asset->numGuideVertices;
	int32 NumCurvesNoPadding = Asset->numStrandsInFile;

	TArray<uint32> RandomCurveIndexArr;
	RandomCurveIndexArr.SetNum(NumCurvesNoPadding);

	for (int32 i = 0; i < NumCurvesNoPadding; ++i)
		RandomCurveIndexArr[i] = i;

	for (int32 i = 0; i < NumCurvesNoPadding; ++i)
	{
		int32 RandCurveIndex = FMath::Rand32() % NumCurvesNoPadding;
		int32 CurIndex = RandomCurveIndexArr[i];
		RandomCurveIndexArr[i] = RandCurveIndex;
		RandomCurveIndexArr[RandCurveIndex] = CurIndex;
	}

	TressFXDatas.RandomCurveIndexArr = RandomCurveIndexArr;

	float MaxRestStrandLength = 0.f;
	float TempRestStrandLength = 0.f;
	FVector3f BoxMin(FLT_MAX,FLT_MAX,FLT_MAX), BoxMax(-FLT_MAX,-FLT_MAX,-FLT_MAX);
	for (int32 i = 0; i < Asset->numGuideVertices; ++i)
	{
		if (i % Asset->numVerticesPerStrand == 0)
			TempRestStrandLength = 0;
		int32 SequenceCurveIndex = i / Asset->numVerticesPerStrand;
		int32 VertexIndexInCurve = i % Asset->numVerticesPerStrand;
		int32 CurVertexIndex = SequenceCurveIndex * Asset->numVerticesPerStrand + VertexIndexInCurve;


		TressFXDatas.StrandsPoints.PointsPosition[i] 
			= FVector4f(Asset->positions[CurVertexIndex].X,Asset->positions[CurVertexIndex].Y,Asset->positions[CurVertexIndex].Z, (i % Asset->numVerticesPerStrand < 2) ? 0.f : 1.f);
		
		TressFXDatas.StrandsPoints.PointsLength[i] = Asset->restLengths[CurVertexIndex];
		TempRestStrandLength += Asset->restLengths[CurVertexIndex];

		const FVector4f& PointPosition = TressFXDatas.StrandsPoints.PointsPosition[i];

		BoxMin = FVector3f(FMath::Min(BoxMin.X, PointPosition.X), FMath::Min(BoxMin.Y, PointPosition.Y), FMath::Min(BoxMin.Z, PointPosition.Z));
		BoxMax = FVector3f(FMath::Max(BoxMax.X, PointPosition.X), FMath::Max(BoxMax.Y, PointPosition.Y), FMath::Max(BoxMax.Z, PointPosition.Z));
	
		if (i % Asset->numVerticesPerStrand == Asset->numVerticesPerStrand - 1)
			MaxRestStrandLength = FMath::Max(MaxRestStrandLength, TempRestStrandLength);
	}

	TressFXDatas.MaxRestLength = MaxRestStrandLength;

	TressFXDatas.BoundingBox = FBox3f(BoxMin,BoxMax);

	for (int32 i = 0; i < Asset->numGuideStrands; ++i)
	{
		int32 RandomCurveIndex = i >= NumCurvesNoPadding ? i : RandomCurveIndexArr[i];
		TressFXDatas.StrandsCurves.CurvesRootUV[i] = Asset->strandUV[RandomCurveIndex];
	}

	if (bBoneDataLoaded && Asset->boneIndexData.Num() >= Asset->numGuideStrands)
	{
		TressFXDatas.SimData.BoneSkinningDatas = Asset->boneSkinningData;
		TressFXDatas.SimData.BoneIndexDatas.SetNum(NumCurves);
		for (int32 i = 0; i < Asset->numGuideStrands; ++i)
		{
			TressFXDatas.SimData.BoneIndexDatas[i] = Asset->boneIndexData[i];
		}
	}
	else
	{
		TressFXDatas.SimData.BoneSkinningDatas.Reset();
		TressFXDatas.SimData.BoneIndexDatas.Reset();
	}
	
	if (ExistingHair)
	{
		OutTressFXAsset->TressFXGroupsRendering.NumStrands = NumCurves;
		OutTressFXAsset->TressFXGroupsRendering.NumGuides = NumCurves;
		OutTressFXAsset->TressFXGroupsRendering.NumStrandsVertices = NumCurves * Asset->numVerticesPerStrand;
		OutTressFXAsset->TressFXGroupsRendering.NumGuidesVertices = NumCurves * Asset->numVerticesPerStrand;
		OutTressFXAsset->TressFXGroupsRendering.NumVerticesPerStrand = Asset->numVerticesPerStrand;
	}
	else
	{
		//OutTressFXAsset->TressFXGroupsMaterials.AddDefaulted();
		OutTressFXAsset->TressFXGroupsMaterials.Material = OutTressFXAsset->Strands_DefaultMaterial;
		//OutTressFXAsset->TressFXGroupsRendering.AddDefaulted();
		OutTressFXAsset->TressFXGroupsRendering.NumStrands = NumCurves;
		OutTressFXAsset->TressFXGroupsRendering.NumGuides = NumCurves;
		OutTressFXAsset->TressFXGroupsRendering.NumStrandsVertices = NumCurves * Asset->numVerticesPerStrand;
		OutTressFXAsset->TressFXGroupsRendering.NumGuidesVertices = NumCurves * Asset->numVerticesPerStrand;
		OutTressFXAsset->TressFXGroupsRendering.NumVerticesPerStrand = Asset->numVerticesPerStrand;
		//OutTressFXAsset->TressFXGroupsInterpolation.AddDefaulted();
		//OutTressFXAsset->TressFXGroupsLOD.AddDefaulted();
		OutTressFXAsset->TressFXGroupsLOD.LODs.AddDefaulted();
		//OutTressFXAsset->TressFXGroupsSimulation.AddDefaulted();
	}
	
	delete Asset;

	const bool bSucceeded = OutTressFXAsset->CacheDerivedDatas();
	if (!bSucceeded)
	{
		// Purge the newly created asset that failed to import
		if (OutTressFXAsset != ExistingHair)
		{
			OutTressFXAsset->ClearFlags(RF_Standalone);
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
		return nullptr;
	}

	return OutTressFXAsset;
}

UTressFXMeshAsset* FTressFXImporter::ImportTFXMesh(const FTressFXMeshImportContext& ImportContext, FTressFXDescription& TressFXDescription, const FString& FilePath, UTressFXMeshAsset* ExistingMesh)
{
	uint32 m_NumVertices = 0;
	uint32 m_NumTriangles = 0;

	TArray<FTressFXBoneSkinningData> boneSkinningData;
	TArray<FTressFXBoneIndexData> boneIndexData;

	TArray<FVector3f> m_pTempVertices;
	TArray<FVector3f> m_pTempNormals;
	TArray<uint32>  m_pTempIndices;

	m_pTempIndices.Empty();
	m_pTempNormals.Empty();
	m_pTempVertices.Empty();

	TArray<FString> Lines;
	FFileHelper::LoadFileToStringArray(Lines, *FilePath);

	if (Lines.Num() == 0)
		return nullptr;

	int numOfBones;
	TArray<FString> BoneNames;

	for (int i = 0; i < Lines.Num(); i)
	{
		FString line = Lines[i];

		if (line.Contains(FString("#")) || line.Len() == 0)
		{
			i++;
			continue;
		}
		else
		{
			TArray<FString> words;
			line.ParseIntoArrayWS(words);

			if (words[0] == FString("numOfBones"))
			{
				numOfBones = FCString::Atoi(*words[1]);

				int boneCounter = 0;
				i++;

				while (boneCounter < numOfBones)
				{
					line = Lines[i];
					i++;
					if (line.Contains(FString("#")) || line.Len() == 0)
					{
						continue;
					}

					words.Empty(2);
					line.ParseIntoArrayWS(words);
					BoneNames.Push(words[1]);

					boneCounter++;
				}
			}
			else if (words[0] == FString("numOfVertices"))
			{
				m_NumVertices = FCString::Atoi(*words[1]);
				boneIndexData.SetNum(m_NumVertices);
				m_pTempVertices.AddDefaulted(m_NumVertices);
				m_pTempNormals.AddDefaulted(m_NumVertices);

				uint32 index = 0;
				i++;
				while (index < m_NumVertices)
				{
					line = Lines[i];
					i++;
					if (line.Contains(FString("#")) || line.Len() == 0)
					{
						continue;
					}

					words.Empty(39);
					line.ParseIntoArrayWS(words);

					index = FCString::Atoi(*words[0]);

					FVector3f pos;
					pos.X = FCString::Atof(*words[1]);
					pos.Z = FCString::Atof(*words[2]);
					pos.Y = FCString::Atof(*words[3]);
					m_pTempVertices[index] = pos;


					FVector3f norm;
					norm.X = FCString::Atof(*words[4]);
					norm.Z = FCString::Atof(*words[5]);
					norm.Y = FCString::Atof(*words[6]);
					m_pTempNormals[index] = norm;

					FTressFXBoneIndexData idxData;
					idxData.StartIdx = boneSkinningData.Num();

					for (int32 j = 0; j < TRESSFX_MAX_INFLUENTIAL_BONE_COUNT; ++j)
					{
						FTressFXBoneSkinningData skinData;

						int boneIndex = FCString::Atoi(*words[7+j]);
						FName BoneName = TCHAR_TO_ANSI(*BoneNames[boneIndex]);
						if (BoneName != NAME_None && IsValid(ImportContext.ImportOptions->Skeleton))
						{
							int engineIndex = ImportContext.ImportOptions->Skeleton->GetRefSkeleton().FindBoneIndex(BoneName);
							skinData.IndexBone = engineIndex;
						}
						else
						{
							//skinData.IndexBone = boneIndex;
							continue;
						}

						skinData.Weight = FCString::Atof(*words[7+ TRESSFX_MAX_INFLUENTIAL_BONE_COUNT+j]);
						if (skinData.Weight == 0)
							continue;
						
						idxData.BoneCount++;

						boneSkinningData.Add(skinData);
					}

					boneIndexData[index] = idxData;

					index++;
				}

			}
			else if (words[0] == FString("numOfTriangles"))
			{
				m_NumTriangles = FCString::Atoi(*words[1]);
				int numIndices = m_NumTriangles * 3;
				m_pTempIndices.AddDefaulted(numIndices);

				uint32 index = 0;
				i++;
				while (index < m_NumTriangles)
				{
					line = Lines[i];
					i++;
					if (line.Contains(FString("#")) || line.Len() == 0)
					{
						continue;
					}

					words.Empty(4);
					line.ParseIntoArrayWS(words);

					m_pTempIndices[index * 3 + 0] = FCString::Atoi(*words[1]);
					m_pTempIndices[index * 3 + 1] = FCString::Atoi(*words[2]);
					m_pTempIndices[index * 3 + 2] = FCString::Atoi(*words[3]);

					++index;
				}
			}
		}
	}

	UTressFXMeshAsset* OutTressFXMeshAsset = nullptr;
	if (ExistingMesh)
		OutTressFXMeshAsset = ExistingMesh;
	else
		OutTressFXMeshAsset = NewObject<UTressFXMeshAsset>(ImportContext.Parent, ImportContext.Class, ImportContext.Name, ImportContext.Flags);

	OutTressFXMeshAsset->MeshGroupData.TFXMeshData.BoneSkinningDatas = boneSkinningData;
	OutTressFXMeshAsset->MeshGroupData.TFXMeshData.BoneIndexDatas = boneIndexData;
	OutTressFXMeshAsset->MeshGroupData.TFXMeshData.Indices = m_pTempIndices;

	FVector3f BoxMin(FLT_MAX, FLT_MAX, FLT_MAX), BoxMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	uint32 NumVertices = m_pTempVertices.Num();
	OutTressFXMeshAsset->MeshGroupData.TFXMeshData.Vertices.SetNum(NumVertices);
	
	for (uint32 i = 0; i < NumVertices; ++i)
	{
		const FVector3f& PointPosition = m_pTempVertices[i];
		const FVector3f& PointNormal = m_pTempNormals[i];

		FVector3f Position = PointPosition;
		FVector3f Normal = PointNormal;
		OutTressFXMeshAsset->MeshGroupData.TFXMeshData.Vertices[i].Position = Position;
		OutTressFXMeshAsset->MeshGroupData.TFXMeshData.Vertices[i].Normal = Normal;

		BoxMin = FVector3f(FMath::Min(BoxMin.X, PointPosition.X), FMath::Min(BoxMin.Y, PointPosition.Y), FMath::Min(BoxMin.Z, PointPosition.Z));
		BoxMax = FVector3f(FMath::Max(BoxMax.X, PointPosition.X), FMath::Max(BoxMax.Y, PointPosition.Y), FMath::Max(BoxMax.Z, PointPosition.Z));
	}

	OutTressFXMeshAsset->MeshGroupData.TFXMeshData.BoundingBox = FBox(BoxMin, BoxMax);

	return OutTressFXMeshAsset;
}

