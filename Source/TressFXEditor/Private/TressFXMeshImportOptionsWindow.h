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
#include "InputCoreTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "TressFXMeshImportOptions.h"

class SButton;
class UTressFXMeshImportOptions;
class UTressFXHairGroupsPreview;
struct FHairGroupInfo;

class STressFXMeshImportOptionsWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STressFXMeshImportOptionsWindow)
		: _ImportOptions(nullptr)
		, _GroupsPreview(nullptr)
		, _WidgetWindow()
		, _FullPath()
		, _ButtonLabel()
	{}

	SLATE_ARGUMENT(UTressFXMeshImportOptions*, ImportOptions)
		SLATE_ARGUMENT(UTressFXHairGroupsPreview*, GroupsPreview)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
		SLATE_ARGUMENT(FText, FullPath)
		SLATE_ARGUMENT(FText, ButtonLabel)
		SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	static TSharedPtr<STressFXMeshImportOptionsWindow> DisplayImportOptions(
		UTressFXMeshImportOptions* ImportOptions,
		UTressFXHairGroupsPreview* GroupsPreview,
		const FString& FilePath);

	static TSharedPtr<STressFXMeshImportOptionsWindow> DisplayRebuildOptions(
		UTressFXMeshImportOptions* ImportOptions,
		UTressFXHairGroupsPreview* GroupsPreview,
		const FString& FilePath);

	FReply OnImport()
	{
		bShouldImport = true;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	FReply OnCancel()
	{
		bShouldImport = false;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return OnCancel();
		}

		return FReply::Unhandled();
	}

	bool ShouldImport() const
	{
		return bShouldImport;
	}

	STressFXMeshImportOptionsWindow()
		: ImportOptions(nullptr)
		, bShouldImport(false)
		, GroupsPreview(nullptr)
	{
		DetailsView = nullptr;
	}

private:

	bool CanImport() const;

private:
	UTressFXMeshImportOptions* ImportOptions;
	TSharedPtr<class IDetailsView> DetailsView;
	TSharedPtr<class IDetailsView> DetailsView2;
	TWeakPtr<SWindow> WidgetWindow;
	TSharedPtr<SButton> ImportButton;
	bool bShouldImport;
public:
	UTressFXHairGroupsPreview* GroupsPreview;
};
