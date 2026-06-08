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
#include "TressFXCore.h"
#include "TressFXInterface.h"
#include "Interfaces/IPluginManager.h"



#define LOCTEXT_NAMESPACE "FTressFXCore"



void ProcessTressFXBookmark(
	FRDGBuilder& GraphBuilder,
	ETressFXBookmark Bookmark,
	FTressFXBookmarkParameters& Parameters);


#if WITH_EDITOR

void ProcessTressFXVisualizeTangent(
	FRDGBuilder& GraphBuilder, 
	FTressFXVisualizeTangentParameters& VisualizeTangentParameters);

void ProcessTressFXVisualizeMesh(
	FRDGBuilder& GraphBuilder,
	FTressFXVisualizeMeshParameters& VisualizeMeshParameters);

void ProcessTressFXVisualizeMeshAABB(
	FRDGBuilder& GraphBuilder,
	FTressFXVisualizeMeshAABBParameters& VisualizeMeshAABBParameters);

void ProcessTressFXVisualizeSDF(
	FRDGBuilder& GraphBuilder,
	FTressFXVisualizeSDFParameters& VisualizeSDFParameters);

void ProcessTressFXVisualizeMTMesh(
	FRDGBuilder& GraphBuilder,
	FTressFXVisualizeMTMeshParameters& VisualizeMTMeshParameters);

#endif

void ProcessTressFXParameters(FTressFXBookmarkParameters& Parameters);

void FTressFXCore::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	RegisterBookmarkFunction(ProcessTressFXBookmark, ProcessTressFXParameters);
	
#if WITH_EDITOR

	RegisterTressFXVisualizeTangent(ProcessTressFXVisualizeTangent);

	RegisterTressFXVisualizeMesh(ProcessTressFXVisualizeMesh);

	RegisterTressFXVisualizeMeshAABB(ProcessTressFXVisualizeMeshAABB);

	RegisterTressFXVisualizeSDF(ProcessTressFXVisualizeSDF);

	RegisterTressFXVisualizeMTMesh(ProcessTressFXVisualizeMTMesh);

#endif

	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("TressFX"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/Runtime/TressFX"), PluginShaderDir);
}

void FTressFXCore::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTressFXCore, TressFXCore)