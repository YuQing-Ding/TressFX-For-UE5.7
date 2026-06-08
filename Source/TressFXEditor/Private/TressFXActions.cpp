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
#include "TressFXActions.h"
#include "TressFXCustomAssetEditorToolkit.h"
#include "TressFXAsset.h"

#include "TressFXBindingAsset.h"
#include "TressFXCreateBindingOptions.h"
#include "TressFXCreateBindingOptionsWindow.h"

#include "ToolMenuSection.h"

#include "Rendering/SkeletalMeshRenderData.h"
#include "Async/ParallelFor.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"


#define LOCTEXT_NAMESPACE "TressFX"


FTressFXActions::FTressFXActions()
{
	
}

bool FTressFXActions::HasActions(const TArray<UObject*>& InObjects) const 
{
	return true;
}

void FTressFXActions::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section) 
{
	FAssetTypeActions_Base::GetActions(InObjects, Section);

	TArray<TWeakObjectPtr<UTressFXAsset>> TressFXAssets = GetTypedWeakObjectPtrs<UTressFXAsset>(InObjects);

	Section.AddMenuEntry(
		"CreateTressFXBinding",
		LOCTEXT("CreateTressFXBinding", "Create TressFX Binding"),
		LOCTEXT("CreateTressFXBindingTooltip", "Create TressFX Binding"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FTressFXActions::CreateBindingAsset, TressFXAssets))
	);
}

void FTressFXActions::CreateBindingAsset(TArray<TWeakObjectPtr<UTressFXAsset>> Objects) const
{
	const TWeakObjectPtr<UTressFXAsset>& TressFXAsset = Objects[0];
	if (TressFXAsset.IsValid())
	{
		UTressFXCreateBindingOptions* CurrentOptions = NewObject<UTressFXCreateBindingOptions>();
		TSharedPtr<STressFXCreateBindingOptionsWindow> TressFXOptionWindow = STressFXCreateBindingOptionsWindow::DisplayCreateBindingOptions(CurrentOptions);

		if (!TressFXOptionWindow->ShouldCreate())
		{
			return; //continue;
		}
		else if (TressFXAsset.Get() && CurrentOptions && CurrentOptions->TargetSkeletalMesh)
		{
			//TressFXAsset->ConditionalPostLoad();
			//CurrentOptions->TargetSkeletalMesh->ConditionalPostLoad();
			UTressFXBindingAsset* BindingAsset = CreateTressFXBindingAsset(TressFXAsset.Get(), CurrentOptions->TargetSkeletalMesh);
			if (BindingAsset)
			{
				BindingAsset->Build();

				TArray<UObject*> CreatedObjects;
				CreatedObjects.Add(BindingAsset);

				FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				ContentBrowserModule.Get().SyncBrowserToAssets(CreatedObjects);

			}
		}
	}
	
}

uint32 FTressFXActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

FText FTressFXActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TressFX", "TressFX");
}

UClass* FTressFXActions::GetSupportedClass() const
{
	return UTressFXAsset::StaticClass();
}

FColor FTressFXActions::GetTypeColor() const
{
	return FColor::White;
}

void FTressFXActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// #ueent_todo: Will need a custom editor at some point, for now just use the Properties editor
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto TressFXAsset = Cast<UTressFXAsset>(*ObjIt);
		if (TressFXAsset != nullptr)
		{
			// Make sure the TressFX asset has a document 

			TSharedRef<FTressFXCustomAssetEditorToolkit> NewCustomAssetEditor(new FTressFXCustomAssetEditorToolkit());
			NewCustomAssetEditor->InitCustomAssetEditor(EToolkitMode::Standalone, EditWithinLevelEditor, TressFXAsset);
		}
	}
}

#undef LOCTEXT_NAMESPACE 