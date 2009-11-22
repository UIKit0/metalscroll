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
#include "TextFormatting.h"

#define HORIZ_MARGIN		5
#define VERT_MARGIN			5

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
	m_wndWidth = 0;
	m_wndHeight = 0;
}

void CodePreview::Destroy()
{
	Hide();

	if(m_hwnd)
	{
		DestroyWindow(m_hwnd);
		m_hwnd = 0;
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
	m_wndWidth = width * s_charWidth + HORIZ_MARGIN*2;
	m_wndHeight = height * s_lineHeight + VERT_MARGIN*2;
	DWORD style = WS_POPUP;
	m_hwnd = CreateWindowA(s_className, "CodePreview", style, 0, 0, m_wndWidth, m_wndHeight, parent, 0, _AtlModule.GetResourceInstance(), this);
}

struct PreviewRenderOp : RenderOperator
{
	PreviewRenderOp(HDC _paintDC, int _imgWidth) : paintDC(_paintDC), imgWidth(_imgWidth) {}

	void AllocImg(int lines, HBITMAP* bmp, void** bits)
	{
		BITMAPINFO bi;
		memset(&bi, 0, sizeof(bi));
		bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
		bi.bmiHeader.biWidth = imgWidth;
		bi.bmiHeader.biHeight = lines * CodePreview::s_lineHeight;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		*bmp = CreateDIBSection(0, &bi, DIB_RGB_COLORS, bits, 0, 0);
	}

	void Init(int numLines)
	{
		imgLines = numLines;
		AllocImg(imgLines, &codeBmp, (void**)&codeBits);
		SelectObject(paintDC, codeBmp);
		imgRect.left = 0; imgRect.right = imgWidth;
		imgRect.top = 0; imgRect.bottom = imgLines*CodePreview::s_lineHeight;
		FillSolidRect(paintDC, MetalBar::s_codePreviewBg, imgRect);
		SetBkMode(paintDC, TRANSPARENT);
	}

	void EndLine(int line, int lastColumn, unsigned int /*lineFlags*/, bool textEnd)
	{
		RECT r;
		r.left = lastColumn*CodePreview::s_charWidth; r.right = imgWidth;
		r.top = line*CodePreview::s_lineHeight; r.bottom = r.top + CodePreview::s_lineHeight;
		if(r.left < imgWidth)
			FillSolidRect(paintDC, MetalBar::s_codePreviewBg, r);

		if(textEnd || (line < imgLines - 1))
			return;

		// We have more lines than we anticipated, due to word wrapping. Make a larger image.
		HBITMAP newBmp;
		unsigned int* newBits;
		int newNumLines = imgLines + imgLines / 2;
		AllocImg(newNumLines, &newBmp, (void**)&newBits);

		// Copy the old image into the new one. Since BMPs are upside down, we must offset the destination.
		int offset = (newNumLines - imgLines)*CodePreview::s_lineHeight*imgWidth;
		memcpy(newBits + offset, codeBits, imgLines*CodePreview::s_lineHeight*imgWidth*4);

		// Fill the new area with the background color.
		SelectObject(paintDC, newBmp);
		r.left = 0; r.right = imgWidth;
		r.top = imgLines*CodePreview::s_lineHeight; r.bottom = newNumLines*CodePreview::s_lineHeight;
		FillSolidRect(paintDC, MetalBar::s_codePreviewBg, r);

		// Delete the old image and replace it with the new one.
		DeleteObject(codeBmp);
		codeBmp = newBmp;
		codeBits = newBits;
		imgLines = newNumLines;
		imgRect.bottom = imgLines*CodePreview::s_lineHeight;
	}

	void RenderSpaces(int /*line*/, int /*column*/, int /*count*/) {}

	void RenderCharacters(int line, int column, const wchar_t* text, int len, unsigned int flags)
	{
		RECT wordRect;
		wordRect.left = column*CodePreview::s_charWidth;
		wordRect.right = wordRect.left + len*CodePreview::s_charWidth;
		wordRect.top = line*CodePreview::s_lineHeight;
		wordRect.bottom = wordRect.top + CodePreview::s_lineHeight;

		HFONT font = CodePreview::s_normalFont;
		unsigned int fgColor = MetalBar::s_codePreviewFg;

		if(flags & TextFlag_Highlight)
		{
			// Draw highlighted text in "inverse video".
			FillSolidRect(paintDC, MetalBar::s_codePreviewFg, wordRect);
			fgColor = MetalBar::s_codePreviewBg;
		}
		else if(flags & TextFlag_Comment)
			fgColor = MetalBar::s_commentColor;
		else if(flags & TextFlag_Keyword)
			font = CodePreview::s_boldFont;

		SetTextColor(paintDC, RGB_TO_COLORREF(fgColor));
		SelectObject(paintDC, font);
		ExtTextOutW(paintDC, wordRect.left, wordRect.top, ETO_CLIPPED, &imgRect, text, len, 0);
	}

	HDC paintDC;
	int imgWidth;

	HBITMAP codeBmp;
	unsigned int* codeBits;
	int imgLines;
	RECT imgRect;
};

void CodePreview::Show(HWND bar, IVsTextView* view, IVsTextLines* buffer, const wchar_t* text, int numLines)
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

	m_paintDC = CreateCompatibleDC(0);

	PreviewRenderOp renderOp(m_paintDC, m_wndWidth - HORIZ_MARGIN*2);
	m_imgNumLines = RenderText(renderOp, view, buffer, text, numLines);
	m_codeBmp = renderOp.codeBmp;

	ShowWindow(m_hwnd, SW_SHOW);
}

void CodePreview::Hide()
{
	ShowWindow(m_hwnd, SW_HIDE);

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

void CodePreview::Update(int y, int line)
{
	// Update the window position.
	y -= m_wndHeight / 2;
	y = clamp(y, m_parentYMin, m_parentYMax - m_wndHeight);

	int x = m_rightEdge - m_wndWidth - 10;
	SetWindowPos(m_hwnd, 0, x, y, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

	int numVisLines = (m_wndHeight-VERT_MARGIN*2) / s_lineHeight;
	m_imgStartLine = (line - numVisLines / 2);
	if(m_imgNumLines > numVisLines)
		m_imgStartLine = clamp(m_imgStartLine, 0, m_imgNumLines - numVisLines);
	else
		m_imgStartLine = 0;

	InvalidateRect(m_hwnd, 0, TRUE);
	UpdateWindow(m_hwnd);
}

void CodePreview::OnPaint(HDC dc)
{
	if(!m_paintDC)
		return;

	// Draw a border.
	RECT r;
	r.left = 0; r.right = m_wndWidth;
	r.top = 0; r.bottom = m_wndHeight;
	StrokeRect(dc, 0, r);

	int blitHeight = std::min(m_imgNumLines*s_lineHeight, m_wndHeight-VERT_MARGIN*2);

	// Fill the area around the image.
	r.left = 1; r.right = m_wndWidth - 1;
	r.top = 1; r.bottom = VERT_MARGIN;
	FillSolidRect(dc, MetalBar::s_codePreviewBg, r);

	r.right = HORIZ_MARGIN;
	r.top = VERT_MARGIN; r.bottom = m_wndHeight - 1;
	FillSolidRect(dc, MetalBar::s_codePreviewBg, r);

	r.left = m_wndWidth - HORIZ_MARGIN; r.right = m_wndWidth - 1;
	r.top = VERT_MARGIN; r.bottom = m_wndHeight - 1;
	FillSolidRect(dc, MetalBar::s_codePreviewBg, r);

	r.left = HORIZ_MARGIN; r.right = m_wndWidth - HORIZ_MARGIN;
	r.top = blitHeight + VERT_MARGIN;
	FillSolidRect(dc, MetalBar::s_codePreviewBg, r);

	// Blit the code image.
	BitBlt(dc, HORIZ_MARGIN, VERT_MARGIN, m_wndWidth-HORIZ_MARGIN*2, blitHeight, m_paintDC, 0, m_imgStartLine*s_lineHeight, SRCCOPY);
}

void CodePreview::Resize(int width, int height)
{
	m_wndWidth = width*s_charWidth + HORIZ_MARGIN*2;
	m_wndHeight = height*s_lineHeight + VERT_MARGIN*2;
	SetWindowPos(m_hwnd, 0, 0, 0, m_wndWidth, m_wndHeight, SWP_NOMOVE|SWP_NOZORDER);
}
