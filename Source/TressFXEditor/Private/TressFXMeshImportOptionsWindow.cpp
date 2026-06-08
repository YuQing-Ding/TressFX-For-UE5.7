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
#include "TressFXMeshImportOptionsWindow.h"

#include "EditorStyleSet.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailsView.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "TressFXMeshImportOptionsWindow"


void STressFXMeshImportOptionsWindow::Construct(const FArguments& InArgs)
{
	ImportOptions = InArgs._ImportOptions;
	GroupsPreview = InArgs._GroupsPreview;
	WidgetWindow = InArgs._WidgetWindow;

	TSharedPtr<SBox> InspectorBox;

	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
		[
			SAssignNew(InspectorBox, SBox)
			.MaxDesiredHeight(650.0f)
			.WidthOverride(400.0)
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
			.IsEnabled(this, &STressFXMeshImportOptionsWindow::CanImport)
			.OnClicked(this, &STressFXMeshImportOptionsWindow::OnImport)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked(this, &STressFXMeshImportOptionsWindow::OnCancel)
			]
		]
	];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	InspectorBox->SetContent(DetailsView->AsShared());
	DetailsView->SetObject(ImportOptions);

}

bool STressFXMeshImportOptionsWindow::CanImport()  const
{
	return true;
}

enum class ETressFXMeshOptionsVisibility : uint8
{
	None = 0x00,
	ConversionOptions = 0x01,
	BuildOptions = 0x02,
	All = ConversionOptions | BuildOptions
};

ENUM_CLASS_FLAGS(ETressFXMeshOptionsVisibility);

TSharedPtr<STressFXMeshImportOptionsWindow> DisplayOptions(
	UTressFXMeshImportOptions* ImportOptions,
	UTressFXHairGroupsPreview* GroupsPreview,
	const FString& FilePath,
	ETressFXMeshOptionsVisibility VisibilityFlag,
	FText WindowTitle,
	FText InButtonLabel)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(WindowTitle)
		.SizingRule(ESizingRule::Autosized);

	TSharedPtr<STressFXMeshImportOptionsWindow> OptionsWindow;

	FString FileName = FPaths::GetCleanFilename(FilePath);
	Window->SetContent
	(
		SAssignNew(OptionsWindow, STressFXMeshImportOptionsWindow)
		.ImportOptions(ImportOptions)
		.GroupsPreview(GroupsPreview)
		.WidgetWindow(Window)
		.FullPath(FText::FromString(FileName))
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

TSharedPtr<STressFXMeshImportOptionsWindow> STressFXMeshImportOptionsWindow::DisplayImportOptions(UTressFXMeshImportOptions* ImportOptions, UTressFXHairGroupsPreview* GroupsPreview, const FString& FilePath)
{
	return DisplayOptions(ImportOptions, GroupsPreview, FilePath, ETressFXMeshOptionsVisibility::All, LOCTEXT("TressFXImportWindowTitle", "TressFX Import Options"), LOCTEXT("Import", "Import"));
}

TSharedPtr<STressFXMeshImportOptionsWindow> STressFXMeshImportOptionsWindow::DisplayRebuildOptions(UTressFXMeshImportOptions* ImportOptions, UTressFXHairGroupsPreview* GroupsPreview, const FString& FilePath)
{
	return DisplayOptions(ImportOptions, GroupsPreview, FilePath, ETressFXMeshOptionsVisibility::BuildOptions, LOCTEXT("TressFXRebuildWindowTitle ", "TressFX Build Options"), LOCTEXT("Build", "Build"));
}


#undef LOCTEXT_NAMESPACE
