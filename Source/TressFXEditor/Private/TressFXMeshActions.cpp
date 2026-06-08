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
#include "TressFXMeshActions.h"
#include "TressFXMeshCustomAssetEditorToolkit.h"
#include "TressFXMeshAsset.h"



FTressFXMeshActions::FTressFXMeshActions()
{
	
}

uint32 FTressFXMeshActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

FText FTressFXMeshActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TressFXMesh", "TressFXMesh");
}

UClass* FTressFXMeshActions::GetSupportedClass() const
{
	return UTressFXMeshAsset::StaticClass();
}

FColor FTressFXMeshActions::GetTypeColor() const
{
	return FColor::White;
}

void FTressFXMeshActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// #ueent_todo: Will need a custom editor at some point, for now just use the Properties editor
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto TressFXMeshAsset = Cast<UTressFXMeshAsset>(*ObjIt);
		if (TressFXMeshAsset != nullptr)
		{
			// Make sure the TressFX asset has a document 

			TSharedRef<FTressFXMeshCustomAssetEditorToolkit> NewCustomAssetEditor(new FTressFXMeshCustomAssetEditorToolkit());
			NewCustomAssetEditor->InitCustomAssetEditor(EToolkitMode::Standalone, EditWithinLevelEditor, TressFXMeshAsset);
		}
	}
}
