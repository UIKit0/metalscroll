/*
*	Copyright 2009 Griffin Software
*
*	Licensed under the Apache License, Version 2.0 (the "License");
*	you may not use this file except in compliance with the License.
*	You may obtain a copy of the License at
*
*		http://www.apache.org/licenses/LICENSE-2.0
*
*	Unless required by applicable law or agreed to in writing, software
*	distributed under the License is distributed on an "AS IS" BASIS,
*	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*	See the License for the specific language governing permissions and
*	limitations under the License. 
*/

#include "MetalScrollPCH.h"
#include "Utils.h"

static bool g_hasSSE2 = false;

static void ScaleImagePlainC(unsigned char* dest, int destHeight, const unsigned char* src, int srcHeight, int width)
{
	float yaspect = 1.0f * srcHeight / destHeight;
	float invAspect = 1.0f / yaspect;
	float lowY = 0.0f;

	for(int i = 0; i < destHeight; ++i)
	{
		float highY = lowY + yaspect;
		if(highY > srcHeight)
		{
			// If the averaging kernel exceeds the source image height, clamp the kernel.
			highY = (float)srcHeight;
			invAspect = 1.0f / (highY - lowY);
		}

		int srcStartY = (int)lowY;
		const unsigned char* srcLineStart = src + srcStartY*width*4;

		// Compute the weights for the first and last pixel.
		float truncLowY = (float)srcStartY;
		float firstPixelArea = 1.0f - (lowY - truncLowY);
		float lastPixelArea = highY - (float)(int)highY;

		for(int j = 0; j < width; ++j)
		{
			float pixel[3] = { 0.0f };

			const unsigned char* srcPixel = srcLineStart + j*4;

			// First pixel.
			for(int k = 0; k < 3; ++k)
				pixel[k] += srcPixel[k] * firstPixelArea;
			srcPixel += width*4;

			// Whole pixels. For some reason, it's faster to iterate with a float than using srcStartY and
			// casting highY to int outside the loop.
			float y = truncLowY + 1.0f;
			while(y < highY - 1)
			{
				for(int k = 0; k < 3; ++k)
					pixel[k] += srcPixel[k];
				++y;
				srcPixel += width*4;
			}

			// Last pixel.
			for(int k = 0; k < 3; ++k)
				pixel[k] += srcPixel[k] * lastPixelArea;

			// Divide by area.
			for(int k = 0; k < 3; ++k)
				dest[k] = (unsigned char)((int)(pixel[k] * invAspect));
			dest += 4;
		}

		lowY += yaspect;
	}
}

static __m128 LoadPixel(unsigned int p)
{
	__m128i intPixel = _mm_cvtsi32_si128(p);
	__m128i zero = _mm_setzero_si128();
	intPixel = _mm_unpacklo_epi8(intPixel, zero);
	intPixel = _mm_unpacklo_epi16(intPixel, zero);
	return _mm_cvtepi32_ps(intPixel);
}

static void ScaleImageSSE(unsigned int* dest, int destHeight, const unsigned int* src, int srcHeight, int width)
{
	float yaspect = 1.0f * srcHeight / destHeight;
	float lowY = 0.0f;

	float invAspectTmp = 1.0f / yaspect;
	__m128 invAspect = _mm_load1_ps(&invAspectTmp);

	for(int i = 0; i < destHeight; ++i)
	{
		float highY = lowY + yaspect;
		if(highY > srcHeight)
		{
			// If the averaging kernel exceeds the source image height, clamp the kernel.
			highY = (float)srcHeight;
			float invArea = 1.0f / (highY - lowY);
			invAspect = _mm_load1_ps(&invArea);
		}

		int srcStartY = (int)lowY;
		const unsigned int* srcLineStart = src + srcStartY*width;

		// Compute the weights for the first and last pixel.
		float truncLowY = (float)srcStartY;
		float areaTmp = 1.0f - (lowY - truncLowY);
		__m128 firstPixelArea = _mm_load1_ps(&areaTmp);
		areaTmp = highY - (float)(int)highY;
		__m128 lastPixelArea = _mm_load1_ps(&areaTmp);

		for(int j = 0; j < width; ++j)
		{
			__m128 accumPixel = _mm_setzero_ps();

			const unsigned int* srcPixel = srcLineStart + j;

			// First pixel.
			__m128 fltPixel = LoadPixel(srcPixel[0]);
			fltPixel = _mm_mul_ps(fltPixel, firstPixelArea);
			accumPixel = _mm_add_ps(fltPixel, accumPixel);
			srcPixel += width;

			// Whole pixels.
			float y = truncLowY + 1.0f;
			while(y < highY - 1)
			{
				fltPixel = LoadPixel(srcPixel[0]);
				accumPixel = _mm_add_ps(fltPixel, accumPixel);
				++y;
				srcPixel += width;
			}

			// Last pixel.
			fltPixel = LoadPixel(srcPixel[0]);
			fltPixel = _mm_mul_ps(fltPixel, lastPixelArea);
			accumPixel = _mm_add_ps(fltPixel, accumPixel);

			// Convert to 4 bytes.
			accumPixel = _mm_mul_ps(accumPixel, invAspect);
			__m128i intPixel = _mm_cvtps_epi32(accumPixel);
			intPixel = _mm_packs_epi32(intPixel, intPixel);
			intPixel = _mm_packus_epi16(intPixel, intPixel);
			*dest =  _mm_cvtsi128_si32(intPixel);
			++dest;
		}

		lowY += yaspect;
	}
}

void ScaleImageVertically(unsigned int* dest, int destHeight, const unsigned int* src, int srcHeight, int width)
{
	if(g_hasSSE2)
		ScaleImageSSE(dest, destHeight, src, srcHeight, width);
	else
		ScaleImagePlainC((unsigned char*)dest, destHeight, (unsigned char*)src, srcHeight, width);
}

void InitScaler()
{
	int info[4];
	__cpuid(info, 1);
	g_hasSSE2 = (info[3] & (1 << 26)) != 0;
}

void Log(const char* fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	_vsnprintf(buf, sizeof(buf), fmt, ap);
	buf[sizeof(buf)-1] = 0;
	va_end(ap);
	OutputDebugStringA(buf);
}

void FillSolidRect(HDC hdc, unsigned int color, const RECT& r)
{
	// Wondrous hack (c) MFC: fill a rect with ExtTextOut(), since it doesn't require us to create a brush.
	COLORREF oldColor = SetBkColor(hdc, RGB_TO_COLORREF(color));
	ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r, NULL, 0, NULL);
	SetBkColor(hdc, oldColor);
}

void StrokeRect(HDC hdc, unsigned int color, const RECT& r)
{
	COLORREF oldColor = SetBkColor(hdc, RGB_TO_COLORREF(color));

	RECT r1 = { r.left, r.top, r.right, r.top + 1 };
	ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r1, NULL, 0, NULL);

	RECT r2 = { r.right - 1, r.top, r.right, r.bottom };
	ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r2, NULL, 0, NULL);

	RECT r3 = { r.left, r.bottom - 1, r.right, r.bottom };
	ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r3, NULL, 0, NULL);

	RECT r4 = { r.left, r.top, r.left + 1, r.bottom };
	ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r4, NULL, 0, NULL);

	SetBkColor(hdc, oldColor);
}
