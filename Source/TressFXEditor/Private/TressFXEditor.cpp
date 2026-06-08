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
#include "TressFXEditor.h"

#include "TressFXCore.h"

#include "TressFXActions.h"
#include "TressFXMeshActions.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "EditorStyleSet.h"
#include "Interfaces/IPluginManager.h"
#include "SlateOptMacros.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"



#define LOCTEXT_NAMESPACE "TressFXEditor"

FName FTressFXEditor::TressFXEditorAppIdentifier(TEXT("TressFXEditor"));



void FTressFXEditor::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TSharedRef<IAssetTypeActions> TressFXAssetActions = MakeShareable(new FTressFXActions());
	TSharedRef<IAssetTypeActions> TressFXMeshAssetActions = MakeShareable(new FTressFXMeshActions());

	AssetTools.RegisterAssetTypeActions(TressFXAssetActions);
	AssetTools.RegisterAssetTypeActions(TressFXMeshAssetActions);

}

void FTressFXEditor::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

TArray<TSharedPtr<ITressFXTranslator>> FTressFXEditor::GetTressFXTranslators()
{
	TArray<TSharedPtr<ITressFXTranslator>> Translators;
	for (TFunction<TSharedPtr<ITressFXTranslator>()>& SpawnTranslator : TranslatorSpawners)
	{
		Translators.Add(SpawnTranslator());
	}

	return Translators;
}

TArray<TSharedPtr<ITressFXTranslator>> FTressFXEditor::GetTressFXMeshTranslators()
{
	TArray<TSharedPtr<ITressFXTranslator>> Translators;
	for (TFunction<TSharedPtr<ITressFXTranslator>()>& SpawnTranslator : MeshTranslatorSpawners)
	{
		Translators.Add(SpawnTranslator());
	}

	return Translators;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTressFXEditor, TressFXEditor)