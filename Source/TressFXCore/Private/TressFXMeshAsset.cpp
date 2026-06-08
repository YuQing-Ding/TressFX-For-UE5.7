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
#include "TressFXMeshAsset.h"
#include "UObject/ConstructorHelpers.h"
#include "TressFXSDFComponent.h"


FArchive& operator<<(FArchive& Ar, FTressFXMeshVertexData& Info)
{
	Ar << Info.Position;
	Ar << Info.Normal;

	return Ar;
}

void FTressFXMeshDatas::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FTressFXImportDataVersion::GUID);
	int32 Ver = Ar.CustomVer(FTressFXImportDataVersion::GUID);

	if (Ver >= FTressFXImportDataVersion::BaseCustomVersion5_0)
	{
		Ar << Vertices;
		Ar << BoneSkinningDatas;
		Ar << BoneIndexDatas;
		Ar << Indices;

		Ar << BoundingBox;
	}
}


template<typename T>
inline void InternalReleaseResource_MeshAsset(T*& Resource)
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

UTressFXMeshAsset::UTressFXMeshAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UTressFXMeshAsset::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	
	{
		// Old format serialized the computed tressfx data directly
		MeshGroupData.TFXMeshData.Serialize(Ar);
	}
}

void UTressFXMeshAsset::PostLoad()
{

	Super::PostLoad();

	CacheDerivedDatas();
}

void UTressFXMeshAsset::BeginDestroy()
{
	ReleaseResource();
	Super::BeginDestroy();
}

#if WITH_EDITOR

void UTressFXMeshAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	{
		FTressFXSDFComponentRecreateRenderStateContext Context(this);
		
	}

}

#endif

bool UTressFXMeshAsset::CacheDerivedDatas()
{
	const bool bIsGameThread = IsInGameThread();


	if (bIsGameThread)
	{
		InitResources();
	}

	return true;
}

void UTressFXMeshAsset::InitResources()
{

	if (MeshGroupData.HasValidData())
	{
		MeshGroupData.MeshRestResource = new FTressFXMeshRestResource(MeshGroupData.TFXMeshData);
		BeginInitResource(MeshGroupData.MeshRestResource);
	}

	bIsInitialized = true;
}

void UTressFXMeshAsset::ReleaseResource()
{
	if (MeshGroupData.HasValidData())
	{
		InternalReleaseResource_MeshAsset(MeshGroupData.MeshRestResource);
	}

	bIsInitialized = false;
}

