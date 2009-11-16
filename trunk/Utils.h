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

#pragma once

#define RGB_TO_COLORREF(rgb) ((rgb & 0xff00) | ((rgb & 0xff) << 16) | ((rgb & 0xff0000) >> 16))
#define COLORREF_TO_RGB(cr) ((cr & 0xff00) | ((cr & 0xff) << 16) | ((cr & 0xff0000) >> 16) | 0xff000000)

template<typename T> static inline int clamp(const T& x, const T& min, const T& max)
{
	return (x < min) ? min : ((x > max) ? max : x);
}

void InitScaler();
void ScaleImageVertically(unsigned int* dest, int destHeight, const unsigned int* src, int srcHeight, int width);

void Log(const char* fmt, ...);

void FillSolidRect(HDC hdc, unsigned int color, const RECT& r);
void StrokeRect(HDC hdc, unsigned int color, const RECT& r);
