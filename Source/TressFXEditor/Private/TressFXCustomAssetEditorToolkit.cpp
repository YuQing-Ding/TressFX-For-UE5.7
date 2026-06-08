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

#include "TressFXCustomAssetEditorToolkit.h"
#include "TressFXAsset.h"
#include "Modules/ModuleManager.h"
#include "EditorStyleSet.h"
#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "AssetEditorModeManager.h"
#include "TressFXAsset.h"
#include "TressFXComponent.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "TressFXEditor.h"
#include "TressFXAssetDetails.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"


#define TRESSFXEDITOR_ENABLE_COMPONENT_PANEL 0
#define LOCTEXT_NAMESPACE "TressFXCustomAssetEditor"

const FName FTressFXCustomAssetEditorToolkit::ToolkitFName(TEXT("TressFXEditor"));

const FName FTressFXCustomAssetEditorToolkit::TabId_Viewport(TEXT("TressFXCustomAssetEditor_Render"));
const FName FTressFXCustomAssetEditorToolkit::TabId_MaterialsProperties(TEXT("TressFXCustomAssetEditor_MaterialsProperties"));
const FName FTressFXCustomAssetEditorToolkit::TabId_RenderingProperties(TEXT("TressFXCustomAssetEditor_RenderProperties"));
const FName FTressFXCustomAssetEditorToolkit::TabId_SimulationProperties(TEXT("TressFXCustomAssetEditor_SimulationProperties"));
const FName FTressFXCustomAssetEditorToolkit::TabId_LODProperties(TEXT("TressFXCustomAssetEditor_LODProperties"));

static uint32 GOpenedTressFXEditorCount = 0;


FTressFXCustomAssetEditorToolkit::FTressFXCustomAssetEditorToolkit()
{
	// Set LOD debug mode to show current LOD index & screen size
	if (GOpenedTressFXEditorCount == 0)
	{
		SetTressFXScreenLODInfo(true);
	}
	++GOpenedTressFXEditorCount;
}

FTressFXCustomAssetEditorToolkit::~FTressFXCustomAssetEditorToolkit()
{
	// Disable LOD debug mode info
	--GOpenedTressFXEditorCount;
	if (GOpenedTressFXEditorCount == 0)
	{
		SetTressFXScreenLODInfo(false);
	}
	check(GOpenedTressFXEditorCount >= 0);
}

void FTressFXCustomAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuTressFXEditor", "TressFX Asset Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(TabId_Viewport, FOnSpawnTab::CreateSP(this, &FTressFXCustomAssetEditorToolkit::SpawnViewportTab))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Render"));

	InTabManager->RegisterTabSpawner(TabId_MaterialsProperties, FOnSpawnTab::CreateSP(this, &FTressFXCustomAssetEditorToolkit::SpawnTab_MaterialsProperties))
		.SetDisplayName(LOCTEXT("MaterialsPropertiesTab", "Materials"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(TabId_RenderingProperties, FOnSpawnTab::CreateSP(this, &FTressFXCustomAssetEditorToolkit::SpawnTab_RenderingProperties))
		.SetDisplayName(LOCTEXT("RenderingPropertiesTab", "Strands"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(TabId_LODProperties, FOnSpawnTab::CreateSP(this, &FTressFXCustomAssetEditorToolkit::SpawnTab_LODProperties))
		.SetDisplayName(LOCTEXT("LODPropertiesTab", "LOD"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(TabId_SimulationProperties, FOnSpawnTab::CreateSP(this, &FTressFXCustomAssetEditorToolkit::SpawnTab_SimulationProperties))
		.SetDisplayName(LOCTEXT("SimulationPropertiesTab", "Simulation"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

}

void FTressFXCustomAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(TabId_Viewport);
	InTabManager->UnregisterTabSpawner(TabId_MaterialsProperties);
	InTabManager->UnregisterTabSpawner(TabId_RenderingProperties);
	InTabManager->UnregisterTabSpawner(TabId_SimulationProperties);
	InTabManager->UnregisterTabSpawner(TabId_LODProperties);
	
#if TRESSFXEDITOR_ENABLE_COMPONENT_PANEL
	InTabManager->UnregisterTabSpawner(TabId_PreviewTressFXComponent);
#endif
}

void FTressFXCustomAssetEditorToolkit::InitPreviewComponents()
{
	check(TressFXAsset != nullptr);
	
	check(PreviewTressFXComponent == nullptr);
	check(PreviewStaticMeshComponent == nullptr);
	check(PreviewSkeletalMeshComponent == nullptr);

	PreviewTressFXComponent = NewObject<UTressFXComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	PreviewTressFXComponent->CastShadow = 1;
	PreviewTressFXComponent->bCastDynamicShadow = 1;
	PreviewTressFXComponent->SetTressFXAsset(TressFXAsset.Get());
	PreviewTressFXComponent->Activate(true);


}

void FTressFXCustomAssetEditorToolkit::InitCustomAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UTressFXAsset* InCustomAsset)
{
	const bool bIsUpdatable = false;
	const bool bAllowFavorites = true;
	const bool bIsLockable = false;

	SetCustomAsset(InCustomAsset);
	InitPreviewComponents();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailView_MaterialsProperties = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailView_RenderingProperties = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailView_SimulationProperties = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailView_LODProperties = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	
	
	DetailView_MaterialsProperties->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FTressFXRenderingDetails::MakeInstance, (ITressFXCustomAssetEditorToolkit*)this, EMaterialPanelType::Materials));
	DetailView_RenderingProperties->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FTressFXRenderingDetails::MakeInstance, (ITressFXCustomAssetEditorToolkit*)this, EMaterialPanelType::Strands));
	DetailView_SimulationProperties->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FTressFXRenderingDetails::MakeInstance, (ITressFXCustomAssetEditorToolkit*)this, EMaterialPanelType::Simulation));
	DetailView_LODProperties->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateStatic(&FTressFXRenderingDetails::MakeInstance, (ITressFXCustomAssetEditorToolkit*)this, EMaterialPanelType::LODs));


	SEditorViewport::FArguments args;
	ViewportTab = SNew(STressFXEditorViewport);
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_TressFXAssetEditor_Layout_v1.0")
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
					->AddTab(TabId_MaterialsProperties, ETabState::OpenedTab)
					->AddTab(TabId_RenderingProperties, ETabState::OpenedTab)
					->AddTab(TabId_LODProperties, ETabState::OpenedTab)
					->AddTab(TabId_SimulationProperties, ETabState::OpenedTab)
#if TRESSFXEDITOR_ENABLE_COMPONENT_PANEL
					->AddTab(TabId_PreviewTressFXComponent, ETabState::OpenedTab)
#endif
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

	FProperty* P0 = FindFProperty<FProperty>(TressFXAsset->GetClass(), GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsLOD));
	FProperty* P1 = FindFProperty<FProperty>(TressFXAsset->GetClass(), GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsInterpolation));
	FProperty* P2 = FindFProperty<FProperty>(TressFXAsset->GetClass(), GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsRendering));
	FProperty* P3 = FindFProperty<FProperty>(TressFXAsset->GetClass(), GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsMaterials));
	FProperty* P4 = FindFProperty<FProperty>(TressFXAsset->GetClass(), GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXSimulationSettings));
	
	P0->RemoveMetaData(TEXT("ShowOnlyInnerProperties"));
	P1->RemoveMetaData(TEXT("ShowOnlyInnerProperties"));
	P2->RemoveMetaData(TEXT("ShowOnlyInnerProperties"));
	P3->RemoveMetaData(TEXT("ShowOnlyInnerProperties"));
	P4->RemoveMetaData(TEXT("ShowOnlyInnerProperties"));
	
	P0->SetMetaData(TEXT("DisplayName"), TEXT("GroupLODs"));
	P1->SetMetaData(TEXT("DisplayName"), TEXT("Interpolation"));
	P2->SetMetaData(TEXT("DisplayName"), TEXT("Rendering"));
	P3->SetMetaData(TEXT("DisplayName"), TEXT("Material"));
	P4->SetMetaData(TEXT("DisplayName"), TEXT("Simulation"));
	
	// Set the asset we are editing in the details view
	if (DetailView_MaterialsProperties.IsValid())
	{
		DetailView_MaterialsProperties->SetObject(Cast<UObject>(TressFXAsset));
	}

	if (DetailView_RenderingProperties.IsValid())
	{
		DetailView_RenderingProperties->SetObject(Cast<UObject>(TressFXAsset));
	}

	if (DetailView_LODProperties.IsValid())
	{
		DetailView_LODProperties->SetObject(Cast<UObject>(TressFXAsset));
	}

	if (DetailView_SimulationProperties.IsValid())
	{
		DetailView_SimulationProperties->SetObject(Cast<UObject>(TressFXAsset));
	}

}

void FTressFXCustomAssetEditorToolkit::SetCustomAsset(UTressFXAsset* InCustomAsset)
{
	TressFXAsset = InCustomAsset;
}

UTressFXAsset* FTressFXCustomAssetEditorToolkit::GetCustomAsset() const
{
	return TressFXAsset.Get();
}

FText FTressFXCustomAssetEditorToolkit::GetToolkitName() const
{
	return FText::FromString(TressFXAsset->GetName());
}

FName FTressFXCustomAssetEditorToolkit::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FTressFXCustomAssetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "TressFX Asset Editor");
}

FText FTressFXCustomAssetEditorToolkit::GetToolkitToolTipText() const
{
	return LOCTEXT("ToolTip", "TressFX Asset Editor");
}

FString FTressFXCustomAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "AnimationDatabase ").ToString();
}

FLinearColor FTressFXCustomAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FColor::Red;
}

UTressFXComponent* FTressFXCustomAssetEditorToolkit::GetPreview_TressFXComponent() const
{
	return PreviewTressFXComponent.Get();
}

UStaticMeshComponent* FTressFXCustomAssetEditorToolkit::GetPreview_StaticMeshComponent() const
{
	return PreviewStaticMeshComponent.Get();
}

USkeletalMeshComponent* FTressFXCustomAssetEditorToolkit::GetPreview_SkeletalMeshComponent() const
{
	return PreviewSkeletalMeshComponent.Get();
}

TSharedRef<SDockTab> FTressFXCustomAssetEditorToolkit::SpawnViewportTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TabId_Viewport);
	check(GetPreview_TressFXComponent());

	ViewportTab->SetTressFXComponent(GetPreview_TressFXComponent());
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

TSharedRef<SDockTab> FTressFXCustomAssetEditorToolkit::SpawnTab_MaterialsProperties(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TabId_MaterialsProperties);

	return SNew(SDockTab)
		//.Icon(FEditorStyle::GetBrush("GenericEditor.Tabs.Properties"))
		.Label(LOCTEXT("MaterialsPropertiesTab", "Materials"))
		.TabColorScale(GetTabColorScale())
		[
			DetailView_MaterialsProperties.ToSharedRef()
		];
}

TSharedRef<SDockTab> FTressFXCustomAssetEditorToolkit::SpawnTab_RenderingProperties(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TabId_RenderingProperties);

	return SNew(SDockTab)
		//.Icon(FEditorStyle::GetBrush("GenericEditor.Tabs.Properties"))
		.Label(LOCTEXT("RenderingPropertiesTab", "Strands"))
		.TabColorScale(GetTabColorScale())
		[
			DetailView_RenderingProperties.ToSharedRef()
		];
}

TSharedRef<SDockTab> FTressFXCustomAssetEditorToolkit::SpawnTab_LODProperties(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TabId_LODProperties);

	return SNew(SDockTab)
		//.Icon(FEditorStyle::GetBrush("GenericEditor.Tabs.Properties"))
		.Label(LOCTEXT("LODPropertiesTab", "LODs"))
		.TabColorScale(GetTabColorScale())
		[
			DetailView_LODProperties.ToSharedRef()
		];
}

TSharedRef<SDockTab> FTressFXCustomAssetEditorToolkit::SpawnTab_SimulationProperties(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TabId_SimulationProperties);

	return SNew(SDockTab)
		//.Icon(FEditorStyle::GetBrush("GenericEditor.Tabs.Properties"))
		.Label(LOCTEXT("SimulationPropertiesTab", "Simulation"))
		.TabColorScale(GetTabColorScale())
		[
			DetailView_SimulationProperties.ToSharedRef()
		];
}


#undef LOCTEXT_NAMESPACE
