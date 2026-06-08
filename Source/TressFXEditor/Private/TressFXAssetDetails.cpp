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
#include "TressFXAssetDetails.h"
#include "Widgets/Input/SCheckBox.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "IDetailGroup.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
//#include "TressFXComponent.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorDirectories.h"
#include "UnrealEdGlobals.h"
#include "IDetailsView.h"
#include "MaterialList.h"
#include "PropertyCustomizationHelpers.h"
#include "Interfaces/IMainFrameModule.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Rendering/SkeletalMeshModel.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Logging/LogMacros.h"

#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "MeshAttributeArray.h"

#include "Widgets/Input/STextComboBox.h"

#include "Widgets/Input/SNumericEntryBox.h"
#include "IDocumentation.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "ComponentReregisterContext.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "SKismetInspector.h"
#include "PropertyEditorDelegates.h"
#include "PropertyCustomizationHelpers.h"
#include "TressFXCustomAssetEditorToolkit.h"
#include "IPropertyUtilities.h"
#include "TressFXAsset.h"
#include "Internationalization/Internationalization.h"


#define LOCTEXT_NAMESPACE "TressFXRenderingDetails"


static FLinearColor HairGroupColor(1.0f, 0.5f, 0.0f);
static FLinearColor HairLODColor(1.0f, 0.5f, 0.0f);


FTressFXRenderingDetails::FTressFXRenderingDetails(ITressFXCustomAssetEditorToolkit* InToolkit, EMaterialPanelType Type)
{
	if (InToolkit)
	{
		TressFXAsset = InToolkit->GetCustomAsset();
	}
	bDeleteWarningConsumed = false;
	PanelType = Type;
}

FTressFXRenderingDetails::~FTressFXRenderingDetails()
{
	
}

TSharedRef<IDetailCustomization> FTressFXRenderingDetails::MakeInstance(ITressFXCustomAssetEditorToolkit* InToolkit, EMaterialPanelType Type)
{
	return MakeShareable(new FTressFXRenderingDetails(InToolkit, Type));
}

FName GetCategoryName(EMaterialPanelType Type)
{
	switch (Type)
	{
	case EMaterialPanelType::Materials:		return FName(TEXT("Materials"));
	case EMaterialPanelType::Strands:		return FName(TEXT("Strands"));
	case EMaterialPanelType::Interpolation: return FName(TEXT("Interpolation"));
	case EMaterialPanelType::LODs:			return FName(TEXT("LODs"));
	case EMaterialPanelType::Simulation:	return FName(TEXT("Simulation"));
	}
	return FName(TEXT("Unknown"));
}

void FTressFXRenderingDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();
	check(SelectedObjects.Num() <= 1); // The OnGenerateCustomWidgets delegate will not be useful if we try to process more than one object.

	TressFXAsset = SelectedObjects.Num() > 0 ? Cast<UTressFXAsset>(SelectedObjects[0].Get()) : nullptr;
	TressFXDetailLayout = &DetailLayout;

	FName CategoryName = GetCategoryName(PanelType);
	IDetailCategoryBuilder& TressFXCategory = DetailLayout.EditCategory(CategoryName, FText::GetEmpty(), ECategoryPriority::TypeSpecific);
	CustomizeStrandsGroupProperties(DetailLayout, TressFXCategory);
}

void FTressFXRenderingDetails::CustomizeStrandsGroupProperties(IDetailLayoutBuilder& DetailLayout, IDetailCategoryBuilder& FilesCategory)
{
	switch (PanelType)
	{
	case EMaterialPanelType::Materials:
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsInterpolation), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsRendering), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXSimulationSettings), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXSimulationSettingsArray), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, AnimationSimulationSettingsMap), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsLOD), UTressFXAsset::StaticClass()));
	}
	break;

	case EMaterialPanelType::Strands:
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsInterpolation), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXSimulationSettings), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXSimulationSettingsArray), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, AnimationSimulationSettingsMap), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsMaterials), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsLOD), UTressFXAsset::StaticClass()));
	}
	break;

	case EMaterialPanelType::Simulation:
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsInterpolation), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsRendering), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsMaterials), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsLOD), UTressFXAsset::StaticClass()));
	}
	break;
	case EMaterialPanelType::Interpolation:
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsRendering), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXSimulationSettings), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXSimulationSettingsArray), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, AnimationSimulationSettingsMap), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsMaterials), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsLOD), UTressFXAsset::StaticClass()));
	}
	break;
	case EMaterialPanelType::LODs:
	{
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsInterpolation), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsRendering), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXSimulationSettings), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXSimulationSettingsArray), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, AnimationSimulationSettingsMap), UTressFXAsset::StaticClass()));
		DetailLayout.HideProperty(DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UTressFXAsset, TressFXGroupsMaterials), UTressFXAsset::StaticClass()));
	}
	break;
	}


	switch (PanelType)
	{
	case EMaterialPanelType::Materials:
	{
	}
	break;
	case EMaterialPanelType::Strands:
	{
	}
	break;
	case EMaterialPanelType::Simulation:
	{
	}
	break;
	case EMaterialPanelType::Interpolation:
	{
	}
	break;
	case EMaterialPanelType::LODs:
	{
	}
	break;
	}
}

void FTressFXRenderingDetails::OnGenerateElementForHairGroup(TSharedRef<IPropertyHandle> StructProperty, int32 GroupIndex, IDetailChildrenBuilder& ChildrenBuilder, IDetailLayoutBuilder* DetailLayout)
{
	const FSlateFontInfo DetailFontInfo = IDetailLayoutBuilder::GetDetailFont();

	FProperty* Property = StructProperty->GetProperty();

	ChildrenBuilder.AddCustomRow(LOCTEXT("HairInfo_Separator", "Separator"))
		.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SNew(SSeparator)
			.Thickness(2)
		.ColorAndOpacity(HairGroupColor)
		]
		]
	.ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SNew(SSeparator)
			.Thickness(2)
		]
	];

}

#undef LOCTEXT_NAMESPACE
