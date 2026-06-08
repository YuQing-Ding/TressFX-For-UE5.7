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
#include "TressFXMeshCustomAssetEditorToolkit.h"
#include "TressFXMeshAsset.h"
#include "Modules/ModuleManager.h"
#include "EditorStyleSet.h"
#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "AssetEditorModeManager.h"
#include "TressFXMeshAsset.h"
#include "TressFXSDFComponent.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "TressFXEditor.h"
#include "TressFXMeshAssetDetails.h"


#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"


#define TRESSFXEDITOR_ENABLE_COMPONENT_PANEL 0
#define LOCTEXT_NAMESPACE "TressFXMeshCustomAssetEditor"

const FName FTressFXMeshCustomAssetEditorToolkit::ToolkitFName(TEXT("TressFXEditor"));

const FName FTressFXMeshCustomAssetEditorToolkit::TabId_Viewport(TEXT("TressFXMeshCustomAssetEditor_Render"));
const FName FTressFXMeshCustomAssetEditorToolkit::TabId_SDFProperties(TEXT("TressFXMeshCustomAssetEditor_SDFProperties"));


FTressFXMeshCustomAssetEditorToolkit::FTressFXMeshCustomAssetEditorToolkit()
{
	
}

FTressFXMeshCustomAssetEditorToolkit::~FTressFXMeshCustomAssetEditorToolkit()
{
	
}

void FTressFXMeshCustomAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuTressFXEditor", "TressFX Asset Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(TabId_Viewport, FOnSpawnTab::CreateSP(this, &FTressFXMeshCustomAssetEditorToolkit::SpawnViewportTab))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Render"));

	InTabManager->RegisterTabSpawner(TabId_SDFProperties, FOnSpawnTab::CreateSP(this, &FTressFXMeshCustomAssetEditorToolkit::SpawnTab_SDFProperties))
		.SetDisplayName(LOCTEXT("SDFPropertiesTab", "SDF"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

}

void FTressFXMeshCustomAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(TabId_Viewport);
	InTabManager->UnregisterTabSpawner(TabId_SDFProperties);

#if TRESSFXEDITOR_ENABLE_COMPONENT_PANEL
	InTabManager->UnregisterTabSpawner(TabId_PreviewTressFXComponent);
#endif
}

void FTressFXMeshCustomAssetEditorToolkit::InitPreviewComponents()
{
	check(TressFXMeshAsset != nullptr);
	
	check(PreviewTressFXSDFComponent == nullptr);
	check(PreviewStaticMeshComponent == nullptr);
	check(PreviewSkeletalMeshComponent == nullptr);

	PreviewTressFXSDFComponent = NewObject<UTressFXSDFComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	PreviewTressFXSDFComponent->CastShadow = 1;
	PreviewTressFXSDFComponent->bCastDynamicShadow = 1;
	PreviewTressFXSDFComponent->SetTressFXMeshAsset(TressFXMeshAsset.Get());
	PreviewTressFXSDFComponent->Activate(true);


}

void FTressFXMeshCustomAssetEditorToolkit::InitCustomAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UTressFXMeshAsset* InCustomAsset)
{
	const bool bIsUpdatable = false;
	const bool bAllowFavorites = true;
	const bool bIsLockable = false;

	SetCustomAsset(InCustomAsset);
	InitPreviewComponents();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = bIsUpdatable;
	DetailsViewArgs.bLockable = bIsLockable;

	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
	DetailView_SDFProperties = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	
	DetailView_SDFProperties->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FTressFXMeshRenderingDetails::MakeInstance, (ITressFXMeshCustomAssetEditorToolkit*)this, EMeshPanelType::SDF));

	SEditorViewport::FArguments args;
	ViewportTab = SNew(STressFXMeshEditorViewport);
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_TressFXMeshAssetEditor_Layout_v01")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->SetHideTabWell(true)
				//->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->SetHideTabWell(true)
					->AddTab(TabId_Viewport, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(TabId_SDFProperties, ETabState::OpenedTab)
				)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;

	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		FTressFXEditor::TressFXEditorAppIdentifier,
		StandaloneDefaultLayout,
		bCreateDefaultStandaloneMenu,
		bCreateDefaultToolbar,
		(UObject*)InCustomAsset);


	// Set the asset we are editing in the details view
	if (DetailView_SDFProperties.IsValid())
	{
		DetailView_SDFProperties->SetObject(Cast<UObject>(TressFXMeshAsset));
	}

}

void FTressFXMeshCustomAssetEditorToolkit::SetCustomAsset(UTressFXMeshAsset* InCustomAsset)
{
	TressFXMeshAsset = InCustomAsset;
}

UTressFXMeshAsset* FTressFXMeshCustomAssetEditorToolkit::GetCustomAsset() const
{
	return TressFXMeshAsset.Get();
}

FText FTressFXMeshCustomAssetEditorToolkit::GetToolkitName() const
{
	return FText::FromString(TressFXMeshAsset->GetName());
}

FName FTressFXMeshCustomAssetEditorToolkit::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FTressFXMeshCustomAssetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "TressFX Mesh Asset Editor");
}

FText FTressFXMeshCustomAssetEditorToolkit::GetToolkitToolTipText() const
{
	return LOCTEXT("ToolTip", "TressFX Mesh Asset Editor");
}

FString FTressFXMeshCustomAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "AnimationDatabase ").ToString();
}

FLinearColor FTressFXMeshCustomAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FColor::Red;
}

UTressFXSDFComponent* FTressFXMeshCustomAssetEditorToolkit::GetPreview_TressFXSDFComponent() const
{
	return PreviewTressFXSDFComponent.Get();
}

UStaticMeshComponent* FTressFXMeshCustomAssetEditorToolkit::GetPreview_StaticMeshComponent() const
{
	return PreviewStaticMeshComponent.Get();
}

USkeletalMeshComponent* FTressFXMeshCustomAssetEditorToolkit::GetPreview_SkeletalMeshComponent() const
{
	return PreviewSkeletalMeshComponent.Get();
}

TSharedRef<SDockTab> FTressFXMeshCustomAssetEditorToolkit::SpawnViewportTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TabId_Viewport);
	check(GetPreview_TressFXSDFComponent());

	ViewportTab->SetTressFXSDFComponent(GetPreview_TressFXSDFComponent());
	ViewportTab->SetStaticMeshComponent(GetPreview_StaticMeshComponent());
	ViewportTab->SetSkeletalMeshComponent(GetPreview_SkeletalMeshComponent());

	return SNew(SDockTab)
		//.Icon(FEditorStyle::GetBrush("GenericEditor.Tabs.Render"))
		.Label(LOCTEXT("RenderTitle", "Render"))
		.TabColorScale(GetTabColorScale())
		[
			ViewportTab.ToSharedRef()
		];
}

TSharedRef<SDockTab> FTressFXMeshCustomAssetEditorToolkit::SpawnTab_SDFProperties(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TabId_SDFProperties);

	return SNew(SDockTab)
		//.Icon(FEditorStyle::GetBrush("GenericEditor.Tabs.Properties"))
		.Label(LOCTEXT("SDFPropertiesTab", "SDF"))
		.TabColorScale(GetTabColorScale())
		[
			DetailView_SDFProperties.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE

