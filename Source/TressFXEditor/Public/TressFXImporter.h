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
#include "UObject/ObjectMacros.h"

class UTressFXAsset;
class UTressFXMeshAsset;
class FTressFXDescription;
class UTressFXImportOptions;
class UTressFXMeshImportOptions;


class TRESSFXEDITOR_API FTressFXDescription
{
public:

};

struct TRESSFXEDITOR_API FTressFXImportContext
{
	FTressFXImportContext(UTressFXImportOptions* InImportOptions, UObject* InParent = nullptr, UClass* InClass = nullptr, FName InName = FName(), EObjectFlags InFlags = EObjectFlags::RF_NoFlags);

	UTressFXImportOptions* ImportOptions;
	UObject* Parent;
	UClass* Class;
	FName Name;
	EObjectFlags Flags;
};

struct TRESSFXEDITOR_API FTressFXMeshImportContext
{
	FTressFXMeshImportContext(UTressFXMeshImportOptions* InImportOptions, UObject* InParent = nullptr, UClass* InClass = nullptr, FName InName = FName(), EObjectFlags InFlags = EObjectFlags::RF_NoFlags);

	UTressFXMeshImportOptions* ImportOptions;
	UObject* Parent;
	UClass* Class;
	FName Name;
	EObjectFlags Flags;
};

struct TRESSFXEDITOR_API FTressFXImporter
{
	static UTressFXAsset* ImportTFX(const FTressFXImportContext& ImportContext, FTressFXDescription& TressFXDescription, const FString& FilePath, UTressFXAsset* ExistingHair = nullptr);
	
	static UTressFXMeshAsset* ImportTFXMesh(const FTressFXMeshImportContext& ImportContext, FTressFXDescription& TressFXDescription, const FString& FilePath, UTressFXMeshAsset* ExistingMesh = nullptr);

};
