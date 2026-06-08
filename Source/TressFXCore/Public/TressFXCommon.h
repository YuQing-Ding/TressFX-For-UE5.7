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

#include "TressFXTypes.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "Runtime/Core/Public/Serialization/CustomVersion.h"


#define TRESSFX_SDF_VISUALIZATION 0

#define TRESSFX_MAJOR_VERSION 4
#define TRESSFX_MINOR_VERSION 0
#define TRESSFX_PATCH_VERSION 0

// TODO: Should be from TressFX SDK
#define TRESSFX_MAX_NUM_BONES 512

#define TRESSFX_COLLISION_CAPSULES 0
#define TRESSFX_MAX_NUM_COLLISION_CAPSULES 8

#define TRESSFX_SIM_THREAD_GROUP_SIZE 64

//////////////////////////////////////////////////////////////////////////
// Moved here from TressFXGPUInterface - Please remove if no longer needed
#define ENABLE_ROV_TEST 0
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// Common macros used throughout TressFX implementation
//////////////////////////////////////////////////////////////////////////

#ifndef TRESSFX_ASSERT
#include "Misc/AssertionMacros.h"
#define TRESSFX_ASSERT check
#endif

#ifndef AMD_SAFE_DELETE
	#define AMD_SAFE_DELETE(p) { if (p) { delete (p); } (p) = nullptr; }
#endif

#ifndef AMD_SAFE_DELETE_ARRAY
	#define AMD_SAFE_DELETE_ARRAY(p) { if(p) { delete[](p); } (p) = nullptr; }
#endif

#ifndef AMD_SAFE_RELEASE
	#define AMD_SAFE_RELEASE(p) { if (p) { (p)->Release(); } (p) = nullptr; }
#endif

#ifndef AMD_SAFE_FREE
	#include "Serialization/MemoryReader.h"
	#define AMD_SAFE_FREE(x) { if(NULL != (x)) { FMemory::Free(x); (x)=NULL;} }
#endif // AMD_SAFE_FREE

#define AMD_ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

template <class T> T AMDMin(T a, T b) { return (a > b) ? b : a; }
template <class T> T AMDMax(T a, T b) { return (a > b) ? a : b; }

#if defined(_DEBUG) || defined(DEBUG)
	#define AMD_TRESSFX_DEBUG  1
	#define AMD_OUTPUT_DEBUG_STRING(format, ...) outputDebugString(format, ##__VA_ARGS__)
#else
	#define AMD_OUTPUT_DEBUG_STRING(format, ...)
#endif

// uavs on simulation side.
#define TRESSFX_IDUAV_POS_OFFSET 0
#define TRESSFX_IDUAV_POS_PREV_OFFSET 1
#define TRESSFX_IDUAV_POS_PREV_PREV_OFFSET 2
#define TRESSFX_IDUAV_TAN_OFFSET 3

// srvs on render side.
#define TRESSFX_IDSRV_POS_OFFSET 0
#define TRESSFX_IDSRV_TAN_OFFSET 1

#define TRESSFX_IDSRV_THICKNESS_OFFSET 0
#define TRESSFX_IDSRV_ROOT_TEXCOORD_OFFSET 1
#define TRESSFX_IDSRV_COLOR_TEXTURE_OFFSET 2

// PPLL offsets
#define TRESSFX_IDUAV_PPLL_HEADS 0
#define TRESSFX_IDUAV_PPLL_NODES 1
#define TRESSFX_IDSRV_PPLL_HEADS 0
#define TRESSFX_IDSRV_PPLL_NODES 1

// ShortCut offsets
#define TRESSFX_IDUAV_SHORTCUT_DEPTHS 0
#define TRESSFX_IDSRV_SHORTCUT_DEPTHS 0
#define TRESSFX_IDSRV_SHORTCUT_COLORS 1
#define TRESSFX_IDSRV_SHORTCUT_INVALPHA 2

#define TRESSFX_GET_16BYTE_INDEX(s, m) (offsetof(s, m) / 16)


//////////////////////////////////////////////////////////////////////////
// Common type defs used to wrap for the time being. 
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// JLacroix - To be removed. Also, why the heck is this called STRING_HASH
//			  ... it doesn't hash anything!?!?
#define TRESSFX_STRING_HASH(a) a
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////

struct TRESSFXCORE_API FTressFXImportDataVersion
{
	enum Type
	{
		BaseCustomVersion5_0,
		AddMaxStrandLength,
		AddRandomCurveIndex,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FTressFXImportDataVersion() {}
};
