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


struct TRESSFXCORE_API FTressFXBoneSkinningData
{
	FTressFXBoneSkinningData()
		: IndexBone(-1),
		Weight(0){}

	int32 IndexBone;
	float Weight;

	friend FArchive& operator << (FArchive& Ar, FTressFXBoneSkinningData& Data)
	{
		Ar << Data.IndexBone << Data.Weight;

		return Ar;
	}
};

struct TRESSFXCORE_API FTressFXBoneIndexData
{
public:
	FTressFXBoneIndexData()
		: StartIdx(0),
		BoneCount(0){}

	// Start Index in SkinningData Array
	int32 StartIdx;
	// Bone Count in SkinningData Array
	int32 BoneCount;

	friend FArchive& operator << (FArchive& Ar, FTressFXBoneIndexData& Data)
	{
		Ar << Data.StartIdx << Data.BoneCount;
		return Ar;
	}
};

