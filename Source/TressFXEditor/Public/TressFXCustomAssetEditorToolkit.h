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
#pragma once


#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Editor/PropertyEditor/Public/PropertyEditorDelegates.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/STressFXEditorViewport.h"

class UTressFXAsset;
class UTressFXComponent;
class IDetailsView;
class SDockableTab;
class UStaticMeshComponent;
class USkeletalMesh;
class UStaticMesh;
class USkeletalMeshComponent;


class TRESSFXEDITOR_API ITressFXCustomAssetEditorToolkit : public FAssetEditorToolkit
{
public:
	/** Retrieves the current custom asset. */
	virtual UTressFXAsset* GetCustomAsset() const = 0;

	/** Set the current custom asset. */
	virtual void SetCustomAsset(UTressFXAsset* InCustomAsset) = 0;
};

class TRESSFXEDITOR_API FTressFXCustomAssetEditorToolkit : public ITressFXCustomAssetEditorToolkit
{
public:

	FTressFXCustomAssetEditorToolkit();

	/** Destructor */
	virtual ~FTressFXCustomAssetEditorToolkit();


	void InitCustomAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UTressFXAsset* InCustomAsset);

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager) override;


	virtual UTressFXAsset* GetCustomAsset() const override;

	virtual void SetCustomAsset(UTressFXAsset* InCustomAsset) override;

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;
	virtual bool IsPrimaryEditor() const override { return true; }


	UTressFXComponent* GetPreview_TressFXComponent() const;
	UStaticMeshComponent* GetPreview_StaticMeshComponent() const;
	USkeletalMeshComponent* GetPreview_SkeletalMeshComponent() const;


	TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnTab_MaterialsProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_RenderingProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_SimulationProperties(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_LODProperties(const FSpawnTabArgs& Args);

private:


	// create the custom components we need
	void InitPreviewComponents();


	TSharedPtr< STressFXEditorViewport > ViewportTab;

	TSharedPtr<class IDetailsView> DetailView_MaterialsProperties;
	TSharedPtr<class IDetailsView> DetailView_RenderingProperties;
	TSharedPtr<class IDetailsView> DetailView_SimulationProperties;
	TSharedPtr<class IDetailsView> DetailView_LODProperties;

	static const FName ToolkitFName;
	static const FName TabId_Viewport;

	static const FName TabId_MaterialsProperties;
	static const FName TabId_RenderingProperties;
	static const FName TabId_SimulationProperties;
	static const FName TabId_LODProperties;

	TWeakObjectPtr  < class UTressFXAsset>  TressFXAsset;

	TWeakObjectPtr  < UTressFXComponent > PreviewTressFXComponent;
	TWeakObjectPtr  < UStaticMeshComponent > PreviewStaticMeshComponent;
	TWeakObjectPtr  < USkeletalMeshComponent > PreviewSkeletalMeshComponent;
};

