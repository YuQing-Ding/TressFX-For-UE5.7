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


#include "TressFXCreateBindingOptionsWindow.h"

#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "TressFXCreateBindingOptions.h"
#include "IDetailsView.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "AssetToolsModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"
#include "TressFXBindingAsset.h"


#define LOCTEXT_NAMESPACE "TressFXCreateBindingOptionsWindow"

void STressFXCreateBindingOptionsWindow::Construct(const FArguments& InArgs)
{
	BindingOptions = InArgs._BindingOptions;
	WidgetWindow = InArgs._WidgetWindow;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(BindingOptions);

	this->ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("CurveEd.LabelFont"))
		.Text(LOCTEXT("CurrentFile", "Current File: "))
		]
	+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("CurveEd.InfoFont"))
		.Text(InArgs._FullPath)
		]
		]
		]

	+ SVerticalBox::Slot()
		.Padding(2)
		.MaxHeight(500.0f)
		[
			DetailsView->AsShared()
		]

	+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(2)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(2)
		+ SUniformGridPanel::Slot(0, 0)
		[
			SAssignNew(ImportButton, SButton)
			.HAlign(HAlign_Center)
		.Text(InArgs._ButtonLabel)
		.IsEnabled(this, &STressFXCreateBindingOptionsWindow::CanCreateBinding)
		.OnClicked(this, &STressFXCreateBindingOptionsWindow::OnCreateBinding)
		]
	+ SUniformGridPanel::Slot(1, 0)
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
		.Text(LOCTEXT("Cancel", "Cancel"))
		.OnClicked(this, &STressFXCreateBindingOptionsWindow::OnCancel)
		]
		]
		];
}

bool STressFXCreateBindingOptionsWindow::CanCreateBinding()  const
{
	return BindingOptions->TargetSkeletalMesh != nullptr;
}

enum class ETressFXBindingOptionsVisibility : uint8
{
	None = 0x00,
	ConversionOptions = 0x01,
	BuildOptions = 0x02,
	All = ConversionOptions | BuildOptions
};

ENUM_CLASS_FLAGS(ETressFXBindingOptionsVisibility);

TSharedPtr<STressFXCreateBindingOptionsWindow> DisplayOptions(UTressFXCreateBindingOptions* BindingOptions, ETressFXBindingOptionsVisibility VisibilityFlag, FText WindowTitle, FText InButtonLabel)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(WindowTitle)
		.SizingRule(ESizingRule::Autosized);

	TSharedPtr<STressFXCreateBindingOptionsWindow> OptionsWindow;

	Window->SetContent
	(
		SAssignNew(OptionsWindow, STressFXCreateBindingOptionsWindow)
		.BindingOptions(BindingOptions)
		.WidgetWindow(Window)
		//		.FullPath(FText::FromString(FileName))
		.ButtonLabel(InButtonLabel)
	);

	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	return OptionsWindow;
}

TSharedPtr<STressFXCreateBindingOptionsWindow> STressFXCreateBindingOptionsWindow::DisplayCreateBindingOptions(UTressFXCreateBindingOptions* BindingOptions)
{
	return DisplayOptions(BindingOptions, ETressFXBindingOptionsVisibility::BuildOptions, LOCTEXT("TressFXBindingRebuildWindowTitle", "TressFX Binding Options"), LOCTEXT("Build", "Create"));
}


static void CreateFilename(const FString& InAssetName, const FString& Suffix, FString& OutPackageName, FString& OutAssetName)
{
	// Get a unique package and asset name
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(InAssetName, Suffix, OutPackageName, OutAssetName);
}

static UTressFXBindingAsset* CreateTressFXBindingAsset(UObject* InParent, UTressFXAsset* TressFXAsset, USkeletalMesh* TargetSkelMesh)
{
#if WITH_EDITOR
	if (!TargetSkelMesh || !TressFXAsset)
	{
		return nullptr;
	}

	// If provided name is empty, then create an auto-generated (unique) filename
	FString Name;
	FString PackageName;
	//if (InPackageName.IsEmpty())
	{
		FString Suffix;

		if (TargetSkelMesh)
		{
			Suffix += TEXT("_") + TargetSkelMesh->GetName();
		}
		Suffix += TEXT("_Binding");
		CreateFilename(TressFXAsset->GetOutermost()->GetName(), Suffix, PackageName, Name);
	}

	UPackage* Package = Cast<UPackage>(InParent);
	if (InParent == nullptr && !PackageName.IsEmpty())
	{
		// Then find/create it.
		Package = CreatePackage(*PackageName);
		if (!ensure(Package))
		{
			// There was a problem creating the package
			return nullptr;
		}
	}

	if (UTressFXBindingAsset* Out = NewObject<UTressFXBindingAsset>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional))
	{
		Out->TressFXAsset = TressFXAsset;
		Out->TargetSkeletalMesh = TargetSkelMesh;
		Out->MarkPackageDirty();
		return Out;
	}
#endif
	return nullptr;
}

UTressFXBindingAsset* CreateTressFXBindingAsset(UTressFXAsset* TressFXAsset, USkeletalMesh* TargetSkelMesh)
{
	if (!TressFXAsset || !TargetSkelMesh)
	{
		return nullptr;
	}

	UObject* BindingAsset = CreateTressFXBindingAsset(nullptr, TressFXAsset, TargetSkelMesh);

	return (UTressFXBindingAsset*)BindingAsset;
}


#undef LOCTEXT_NAMESPACE
