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
#include "Widgets/STressFXMeshEditorViewport.h"
#include "Widgets/Layout/SBox.h"
#include "Editor/UnrealEdEngine.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UnrealEdGlobals.h"
#include "ComponentReregisterContext.h"
#include "Slate/SceneViewport.h"
#include "Engine/TextureCube.h"
#include "ImageUtils.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "DrawDebugHelpers.h"
#include "AdvancedPreviewScene.h"
#include "Widgets/Docking/SDockTab.h"
#include "TressFXSDFComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
//#include "TressFXEditorCommands.h"
//#include "TressFXMeshEditorViewportToolBar.h"

#define LOCTEXT_NAMESPACE "STressFXMeshEditorViewport"



class FTressFXMeshEditorViewportClient : public FEditorViewportClient
{
public:
	FTressFXMeshEditorViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<STressFXMeshEditorViewport>& InTressFXMeshEditorViewport);

	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override {
		return true;
	};
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily, const int32 StereoPass = INDEX_NONE) override;
	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override { return false; }
	virtual bool CanCycleWidgetMode() const override { return false; }

	void SetShowGrid(bool bShowGrid);

	virtual void SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport)override;

	TWeakPtr<STressFXMeshEditorViewport> TressFXMeshEditorViewportPtr;
};

FTressFXMeshEditorViewportClient::FTressFXMeshEditorViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<STressFXMeshEditorViewport>& InTressFXMeshEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InTressFXMeshEditorViewport))
{
	TressFXMeshEditorViewportPtr = InTressFXMeshEditorViewport;

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = false;
	DrawHelper.GridColorAxis = FColor(80, 80, 80);
	DrawHelper.GridColorMajor = FColor(72, 72, 72);
	DrawHelper.GridColorMinor = FColor(64, 64, 64);
	DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;
	ShowWidget(false);

	SetViewMode(VMI_Lit);

	EngineShowFlags.DisableAdvancedFeatures();
	EngineShowFlags.SetSnap(0);
	EngineShowFlags.SetTemporalAA(true);

	OverrideNearClipPlane(1.0f);

	//This seems to be needed to get the correct world time in the preview.
	SetIsSimulateInEditorViewport(true);
}

void FTressFXMeshEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

void FTressFXMeshEditorViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	TSharedPtr<STressFXMeshEditorViewport> TressFXMeshEditorViewport = TressFXMeshEditorViewportPtr.Pin();

	FEditorViewportClient::Draw(InViewport, Canvas);
}

FLinearColor FTressFXMeshEditorViewportClient::GetBackgroundColor() const
{
	FLinearColor BackgroundColor = FLinearColor::Black;
	return BackgroundColor;
}

FSceneView* FTressFXMeshEditorViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily, const int32 StereoPass)
{
	FSceneView* SceneView = FEditorViewportClient::CalcSceneView(ViewFamily);
	FFinalPostProcessSettings::FCubemapEntry& CubemapEntry = *new(SceneView->FinalPostProcessSettings.ContributingCubemaps) FFinalPostProcessSettings::FCubemapEntry;
	CubemapEntry.AmbientCubemap = GUnrealEd->GetThumbnailManager()->AmbientCubemap;
	CubemapEntry.AmbientCubemapTintMulScaleValue = FLinearColor::White;
	return SceneView;
}

void FTressFXMeshEditorViewportClient::SetShowGrid(bool bShowGrid)
{
	DrawHelper.bDrawGrid = bShowGrid;
}

void FTressFXMeshEditorViewportClient::SetIsSimulateInEditorViewport(bool bInIsSimulateInEditorViewport)
{
	bIsSimulateInEditorViewport = bInIsSimulateInEditorViewport;
}



void STressFXMeshEditorViewport::Construct(const FArguments& InArgs)
{
	//bShowGrid = true;
	TressFXSDFComponent = nullptr;
	StaticTressFXTarget = nullptr;
	SkeletalTressFXTarget = nullptr;

	AdvancedPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	AdvancedPreviewScene->SetFloorVisibility(false);

	SEditorViewport::Construct(SEditorViewport::FArguments());
}

STressFXMeshEditorViewport::~STressFXMeshEditorViewport()
{
	if (SystemViewportClient.IsValid())
	{
		SystemViewportClient->Viewport = NULL;
	}
}

void STressFXMeshEditorViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (TressFXSDFComponent != nullptr)
	{
		Collector.AddReferencedObject(TressFXSDFComponent);
	}

	if (StaticTressFXTarget != nullptr)
	{
		Collector.AddReferencedObject(StaticTressFXTarget);
	}
}

TSharedRef<class SEditorViewport> STressFXMeshEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> STressFXMeshEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void STressFXMeshEditorViewport::OnFloatingButtonClicked()
{

}

TSharedRef<FEditorViewportClient> STressFXMeshEditorViewport::MakeEditorViewportClient()
{
	SystemViewportClient = MakeShareable(new FTressFXMeshEditorViewportClient(*AdvancedPreviewScene.Get(), SharedThis(this)));

	SystemViewportClient->SetViewLocation(FVector::ZeroVector);
	SystemViewportClient->SetViewRotation(FRotator::ZeroRotator);
	SystemViewportClient->SetViewLocationForOrbiting(FVector::ZeroVector);
	SystemViewportClient->bSetListenerPosition = false;

	SystemViewportClient->SetRealtime(true);
	SystemViewportClient->VisibilityDelegate.BindSP(this, &STressFXMeshEditorViewport::IsVisible);

	return SystemViewportClient.ToSharedRef();
}

void STressFXMeshEditorViewport::RefreshViewport()
{
	// Invalidate the viewport's display.
	SceneViewport->Invalidate();
}

void  STressFXMeshEditorViewport::SetTressFXSDFComponent(UTressFXSDFComponent* InTressFXSDFComponent)
{
	if (TressFXSDFComponent != nullptr)
	{
		AdvancedPreviewScene->RemoveComponent(TressFXSDFComponent);
	}
	TressFXSDFComponent = InTressFXSDFComponent;

	if (TressFXSDFComponent != nullptr)
	{
		TressFXSDFComponent->PostLoad();
		AdvancedPreviewScene->AddComponent(TressFXSDFComponent, TressFXSDFComponent->GetRelativeTransform());
	}

	if (TressFXSDFComponent != nullptr && SystemViewportClient)
	{
		SystemViewportClient->FocusViewportOnBox(TressFXSDFComponent->Bounds.GetBox());
	}

	RefreshViewport();
}

void STressFXMeshEditorViewport::SetStaticMeshComponent(UStaticMeshComponent* Target)
{
	if (StaticTressFXTarget != nullptr)
	{
		AdvancedPreviewScene->RemoveComponent(StaticTressFXTarget);
	}
	StaticTressFXTarget = Target;

	if (StaticTressFXTarget != nullptr)
	{
		AdvancedPreviewScene->AddComponent(StaticTressFXTarget, StaticTressFXTarget->GetRelativeTransform());
	}
}

void STressFXMeshEditorViewport::SetSkeletalMeshComponent(USkeletalMeshComponent* Target)
{
	if (SkeletalTressFXTarget != nullptr)
	{
		AdvancedPreviewScene->RemoveComponent(SkeletalTressFXTarget);
	}
	SkeletalTressFXTarget = Target;

	if (StaticTressFXTarget != nullptr)
	{
		AdvancedPreviewScene->AddComponent(SkeletalTressFXTarget, SkeletalTressFXTarget->GetRelativeTransform());
	}
}


#undef LOCTEXT_NAMESPACE
