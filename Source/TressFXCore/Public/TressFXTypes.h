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

typedef signed long long int64;
typedef signed int       int32;
typedef signed short     int16;
typedef signed char		 int8;

typedef unsigned long long uint64;
typedef unsigned int       uint32;
typedef unsigned short     uint16;
typedef unsigned char      uint8;
typedef unsigned char      byte;

// Define multi-dimension types
#pragma warning(push)
#pragma warning(disable : 4201)

template <typename T>
struct type2 {
	union {
		struct { T x, y; };
		T v[2];
	};
};

template <typename T>
struct type3 {
	union {
		struct { T x, y, z; };
		T v[3];
	};
};

template <typename T>
struct type4 {
	union {
		struct { T x, y, z, w; };
		T v[4];
	};
};

typedef type2<float> float2;
typedef type3<float> float3;
typedef type4<float> float4;

typedef type2<unsigned int> uint2;
typedef type3<unsigned int> uint3;
typedef type4<unsigned int> uint4;

typedef type2<signed int> sint2;
typedef type3<signed int> sint3;
typedef type4<signed int> sint4;

// If you ever need this for another type, please template as above and 
// define the type similarly to how it was done for float4
struct float4x4 {
	union {
		float4 r[4];
		float  m[16]; };
};
#pragma warning(pop)

