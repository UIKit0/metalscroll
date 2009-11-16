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
#include "CodePreview.h"
#include "Utils.h"
#include "MetalBar.h"

#define HORIZ_MARGIN		10
#define VERT_MARGIN			10

const char CodePreview::s_className[] = "MetalScrollCodePreview";
HFONT CodePreview::s_normalFont = 0;
HFONT CodePreview::s_boldFont = 0;
int CodePreview::s_charWidth;
int CodePreview::s_lineHeight;

CodePreview::CodePreview()
{
	m_hwnd = 0;
	m_paintDC = 0;
	m_codeBmp = 0;
	m_width = 0;
	m_height = 0;
}

void CodePreview::Destroy()
{
	if(m_hwnd)
	{
		DestroyWindow(m_hwnd);
		m_hwnd = 0;
	}

	if(m_codeBmp)
	{
		DeleteObject(m_codeBmp);
		m_codeBmp = 0;
	}

	if(m_paintDC)
	{
		DeleteDC(m_paintDC);
		m_paintDC = 0;
	}
}

LRESULT FAR PASCAL CodePreview::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	if(message == WM_CREATE)
	{
		CREATESTRUCT* cs = (CREATESTRUCT*)lparam;
		SetWindowLongPtr(hwnd, GWL_USERDATA, (::LONG_PTR)cs->lpCreateParams);
		return 0;
	}

	CodePreview* inst = (CodePreview*)GetWindowLongPtr(hwnd, GWL_USERDATA);
	if(!inst)
		return DefWindowProc(hwnd, message, wparam, lparam);

	switch(message)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			if(hdc)
			{
				inst->OnPaint(hdc);
				EndPaint(hwnd, &ps);
			}
			return 0;
		}

		case WM_ERASEBKGND:
			return 1;
	}

	return DefWindowProc(hwnd, message, wparam, lparam);
}

void CodePreview::Register()
{
	WNDCLASSA wndClass;
	memset(&wndClass, 0, sizeof(wndClass));
	wndClass.lpfnWndProc = &WndProc;
	wndClass.hInstance = _AtlModule.GetResourceInstance();
	wndClass.lpszClassName = s_className;
	RegisterClassA(&wndClass);

	s_normalFont = CreateFontA(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH|FF_DONTCARE, "Courier New");
	s_boldFont = CreateFontA(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH |FF_DONTCARE, "Courier New");

	HDC memDC = CreateCompatibleDC(0);
	SelectObject(memDC, s_normalFont);
	TEXTMETRIC metrics;
	GetTextMetrics(memDC, &metrics);
	DeleteDC(memDC);
	
	s_charWidth = metrics.tmMaxCharWidth;
	s_lineHeight = metrics.tmHeight;
}

void CodePreview::Unregister()
{
	UnregisterClassA(s_className, _AtlModule.GetResourceInstance());
	
	if(s_normalFont)
	{
		DeleteObject(s_normalFont);
		s_normalFont = 0;
	}

	if(s_boldFont)
	{
		DeleteObject(s_boldFont);
		s_boldFont = 0;
	}
}

void CodePreview::Create(HWND parent, int width, int height)
{
	m_width = width * s_charWidth + HORIZ_MARGIN;
	m_height = height * s_lineHeight + VERT_MARGIN;
	DWORD style = WS_POPUP;
	m_hwnd = CreateWindowA(s_className, "CodePreview", style, 0, 0, m_width, m_height, parent, 0, _AtlModule.GetResourceInstance(), this);
}

void CodePreview::Show(HWND bar)
{
	RECT r;
	GetClientRect(bar, &r);
	
	POINT p;
	p.x = r.left;
	p.y = r.top;
	ClientToScreen(bar, &p);
	m_rightEdge = p.x;
	m_parentYMin = p.y;

	p.x = r.left;
	p.y = r.bottom;
	ClientToScreen(bar, &p);
	m_parentYMax = p.y;

	ShowWindow(m_hwnd, SW_SHOW);
}

void CodePreview::Hide()
{
	ShowWindow(m_hwnd, SW_HIDE);
}

wchar_t* CodePreview::EatWhitespace(wchar_t* text, int* x, int* y)
{
	for(;;)
	{
		switch(*text)
		{
			case L' ':
				*x += s_charWidth;
				break;

			case L'\t':
				*x = (*x / m_tabSize + 1) * m_tabSize;
				break;

			case L'\r':
			case L'\n':
				if( (text[0] == L'\r') && (text[1] == L'\n') )
					++text;
				*x = 0;
				*y += s_lineHeight;
				break;

			default:
				return text;
		}

		++text;
	}
}

wchar_t* CodePreview::EatComment(wchar_t* text, int* x)
{
	// Skip the two slashes.
	text += 2;
	*x += 2*s_charWidth;

	while( (*text != 0) && (*text != '\r') && (*text != '\n') )
	{
		// Hack: we can't expand tabs properly, so we just replace them with spaces.
		if(*text == L'\t')
			*text = L' ';
		++text;
	}

	return text;
}

wchar_t* CodePreview::EatString(wchar_t* text, int* x)
{
	// Skip the starting quote.
	text += 1;
	*x += s_charWidth;

	// Simplified string parsing: end the string with a newline or with a quote which
	// is not preceded by a backslash. This doesn't handle continued line and quotes
	// preceded by escaped backslashed, but I don't care.
	for(;;)
	{
		switch(*text)
		{
			case 0:
			case L'\r':
			case L'\n':
				return text;

			case '"':
				if(text[-1] == L'\\')
					break;
				*x += s_charWidth;
				++text;
				return text;
		}

		// Hack: we can't expand tabs properly, so we just replace them with spaces.
		if(*text == L'\t')
			*text = L' ';

		*x += s_charWidth;
		++text;
	}
}

wchar_t* CodePreview::EatIdentifier(wchar_t* text, int* x)
{
	while( (*text == L'_') || ((*text >= L'A') && (*text <= L'Z')) || ((*text >= L'a') && (*text <= L'z')) || ((*text >= L'0') && (*text <= L'9')) )
	{
		++text;
		*x += s_charWidth;
	}

	return text;
}

#define DRAW_ITEM(eatFunc)																\
	wchar_t* start = chr;																\
	int startX = textX + r.left;														\
	chr = eatFunc(chr, &textX);															\
	ExtTextOutW(m_paintDC, startX, textY + r.top, ETO_CLIPPED, &r, start, chr - start, 0)

extern const wchar_t* IsCppKeyword(const wchar_t* str, unsigned int len);

void CodePreview::Update(int y, wchar_t* text, int tabSize)
{
	m_tabSize = tabSize * s_charWidth;

	if(!m_paintDC)
	{
		HDC wndDC = GetDC(m_hwnd);
		m_paintDC = CreateCompatibleDC(wndDC);
		m_codeBmp = CreateCompatibleBitmap(wndDC, m_width, m_height);
		SelectObject(m_paintDC, m_codeBmp);
		ReleaseDC(m_hwnd, wndDC);
	}

	// Fill the background and draw a border.
	RECT r;
	GetClientRect(m_hwnd, &r);
	FillSolidRect(m_paintDC, MetalBar::s_codePreviewBg, r);
	StrokeRect(m_paintDC, MetalBar::s_codePreviewFg, r);

	// Inset the draw rectangle so that we don't touch the border with the text.
	r.left += HORIZ_MARGIN / 2;
	r.top += VERT_MARGIN / 2;
	r.right -= HORIZ_MARGIN / 2;
	r.bottom -= VERT_MARGIN / 2;

	SetTextColor(m_paintDC, RGB_TO_COLORREF(MetalBar::s_codePreviewFg));
	int oldBkMode = SetBkMode(m_paintDC, TRANSPARENT);
	HGDIOBJ prevFont = SelectObject(m_paintDC, s_normalFont);

	// Paint the code image.
	int textX = 0;
	int textY = 0;
	wchar_t* chr = text;
	while(*chr)
	{
		chr = EatWhitespace(chr, &textX, &textY);
		if(chr[0] == 0)
			break;

		if(chr[0] == L'"')
		{
			DRAW_ITEM(EatString);
			continue;
		}

		if( (chr[0] == L'/') && (chr[1] == L'/') )
		{
			SetTextColor(m_paintDC, RGB_TO_COLORREF(MetalBar::s_commentColor));
			DRAW_ITEM(EatComment);
			SetTextColor(m_paintDC, RGB_TO_COLORREF(MetalBar::s_codePreviewFg));
			continue;
		}

		if( (chr[0] == L'_') || ((chr[0] >= L'A') && (chr[0] <= L'Z')) || ((chr[0] >= L'a') && (chr[0] <= L'z')) )
		{
			wchar_t* start = chr;
			int startX = textX + r.left;
			int startY = textY + r.top;
			chr = EatIdentifier(chr, &textX);
			int idLen = (int)(chr - start);
			bool isKeyword = (IsCppKeyword(start, idLen) != 0);
			if(isKeyword)
				SelectObject(m_paintDC, s_boldFont);
			ExtTextOutW(m_paintDC, startX, startY, ETO_CLIPPED, &r, start, idLen, 0);
			if(isKeyword)
				SelectObject(m_paintDC, s_normalFont);
			continue;
		}

		ExtTextOutW(m_paintDC, textX + r.left, textY + r.top, ETO_CLIPPED, &r, chr, 1, 0);
		++chr;
		textX += s_charWidth;
	}

	SetBkMode(m_paintDC, oldBkMode);
	SelectObject(m_paintDC, prevFont);

	// Update the window position.
	y -= m_height / 2;
	y = clamp(y, m_parentYMin, m_parentYMax - m_height);

	int x = m_rightEdge - m_width - 10;
	SetWindowPos(m_hwnd, 0, x, y, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

	InvalidateRect(m_hwnd, 0, TRUE);
	UpdateWindow(m_hwnd);
}

void CodePreview::OnPaint(HDC dc)
{
	if(!m_paintDC)
		return;

	BitBlt(dc, 0, 0, m_width, m_height, m_paintDC, 0, 0, SRCCOPY);
}

void CodePreview::Resize(int width, int height)
{
	m_width = width*s_charWidth + HORIZ_MARGIN;
	m_height = height*s_lineHeight + VERT_MARGIN;
	SetWindowPos(m_hwnd, 0, 0, 0, m_width, m_height, SWP_NOMOVE|SWP_NOZORDER);
	if(m_paintDC)
	{
		DeleteObject(m_codeBmp);
		m_codeBmp = 0;
		DeleteDC(m_paintDC);
		m_paintDC = 0;
	}
}
