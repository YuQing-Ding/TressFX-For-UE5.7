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
#include "TressFXFactory.h"
#include "TressFXEditor.h"
#include "ITressFXTranslator.h"
#include "TressFXAsset.h"
#include "TressFXImportOptionsWindow.h"
#include "TressFXMeshImportOptionsWindow.h"
#include "TressFXImporter.h"
#include "TressFXImportOptions.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "TressFXFactory"

UTressFXFactory::UTressFXFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("tfx;TressFX file"));

	SupportedClass = UTressFXAsset::StaticClass();
	bCreateNew = false;		// manual creation not allow
	bEditAfterNew = false;

	bEditorImport = true;	// only allow import

	// Slightly increased priority to allow its translators to check if they can translate the file
	ImportPriority += 1;
	
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		ImportOptions = NewObject<UTressFXImportOptions>();

		InitTranslators();
	}
}

bool UTressFXFactory::FactoryCanImport(const FString& Filename)
{
	for (TSharedPtr<ITressFXTranslator> Translator : Translators)
	{
		if (Translator->CanTranslate(Filename))
		{
			return true;
		}
	}
	return false;
}

TSharedPtr<ITressFXTranslator> UTressFXFactory::GetTranslator(const FString& Filename)
{
	FString Extension = FPaths::GetExtension(Filename);
	for (TSharedPtr<ITressFXTranslator> Translator : Translators)
	{
		if (Translator->IsFileExtensionSupported(Extension))
		{
			return Translator;
		}
	}
	return {};
}

void UTressFXFactory::InitTranslators()
{
	Formats.Reset();

	Translators = FTressFXEditor::Get().GetTressFXTranslators();
	for (TSharedPtr<ITressFXTranslator> Translator : Translators)
	{
		Formats.Add(Translator->GetSupportedFormat());
	}
}

void UTressFXFactory::GetSupportedFileExtensions(TArray<FString>& OutExtensions) const
{
	if (HasAnyFlags(RF_ClassDefaultObject))// && Formats.Num() == 0)
	{
		// Init the translators the first time the CDO is used
		UTressFXFactory* Factory = const_cast<UTressFXFactory*>(this);
		Factory->InitTranslators();
	}

	Super::GetSupportedFileExtensions(OutExtensions);
}

UObject* UTressFXFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	bOutOperationCanceled = false;

	// Translate the hair data from the file
	TSharedPtr<ITressFXTranslator> SelectedTranslator = GetTranslator(Filename);
	if (!SelectedTranslator.IsValid())
	{
		return nullptr;
	}

	FString Extension = FPaths::GetExtension(Filename);

	if(Extension == TEXT("tfx"))
	{
		// Convert the process hair description into hair groups
		UTressFXHairGroupsPreview* GroupsPreview = nullptr;// NewObject<UTressFXHairGroupsPreview>();

		if (!GIsRunningUnattendedScript && !IsAutomatedImport())
		{
			// Display import options and handle user cancellation
			TSharedPtr<STressFXImportOptionsWindow> TressFXOptionWindow = STressFXImportOptionsWindow::DisplayImportOptions(ImportOptions, GroupsPreview, Filename);
			if (!TressFXOptionWindow->ShouldImport())
			{
				bOutOperationCanceled = true;
				return nullptr;
			}
		}
		else
			return nullptr;

		UTressFXAsset* ExistingAsset = FindObject<UTressFXAsset>(InParent, *InName.ToString());
		//if (ExistingAsset)
		//{
			//ExistingAsset->SetNumGroup(0);
		//}

		UTressFXAsset* CurrentAsset = nullptr;
		FTressFXDescription TressFXDescription;
		FTressFXImportContext TressFXImportContext(ImportOptions, InParent, InClass, InName, Flags);

		FScopedSlowTask Progress((float)1, LOCTEXT("ImportTressFXAsset", "Importing TressFX asset..."), true);
		Progress.MakeDialog(true);
		
		CurrentAsset = FTressFXImporter::ImportTFX(TressFXImportContext, TressFXDescription, Filename, ExistingAsset);

		return CurrentAsset;
	}
	
	return nullptr;
}

#undef LOCTEXT_NAMESPACE

