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
	PreviewRenderOp(std::vector<CodePreview::CharInfo>& text, int lineWidth) : m_text(text), m_lineWidth(lineWidth) {}

	void Init(int numLines)
	{
		m_text.reserve(numLines*m_lineWidth);
		m_text.resize(m_lineWidth);
	}

	void EndLine(int line, int lastColumn, unsigned int /*lineFlags*/, bool textEnd)
	{
		if(lastColumn < m_lineWidth)
			m_text[line*m_lineWidth + lastColumn].format = CodePreview::FormatType_EOL;

		if(!textEnd)
			m_text.resize((line + 2) * m_lineWidth);
	}

	void RenderSpaces(int line, int column, int count)
	{
		for(int i = column; (i < column + count) && (i < m_lineWidth); ++i)
		{
			m_text[line*m_lineWidth + i].format = CodePreview::FormatType_Plain;
			m_text[line*m_lineWidth + i].chr = L' ';
		}
	}

	void RenderCharacter(int line, int column, wchar_t chr, unsigned int flags)
	{
		if(column >= m_lineWidth)
			return;

		unsigned char format;
		if(flags & TextFlag_Highlight)
			format = CodePreview::FormatType_Highlight;
		else if(flags & TextFlag_Comment)
			format = CodePreview::FormatType_Comment;
		else if(flags & TextFlag_Keyword)
			format = CodePreview::FormatType_Keyword;
		else
			format = CodePreview::FormatType_Plain;

		m_text[line*m_lineWidth + column].format = format;
		m_text[line*m_lineWidth + column].chr = chr;
	}

	std::vector<CodePreview::CharInfo>& m_text;
	int m_lineWidth;
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

	if(!m_paintDC)
	{
		HDC wndDC = GetDC(m_hwnd);
		m_paintDC = CreateCompatibleDC(wndDC);
		m_codeBmp = CreateCompatibleBitmap(wndDC, m_wndWidth, m_wndHeight);
		SelectObject(m_paintDC, m_codeBmp);
		ReleaseDC(m_hwnd, wndDC);
	}

	int charsPerLine = (m_wndWidth - HORIZ_MARGIN*2) / s_charWidth;
	PreviewRenderOp renderOp(m_text, charsPerLine);
	m_imgNumLines = RenderText(renderOp, view, buffer, text, numLines);

	ShowWindow(m_hwnd, SW_SHOW);
}

void CodePreview::Hide()
{
	ShowWindow(m_hwnd, SW_HIDE);
}

void CodePreview::FlushTextBuf(std::vector<wchar_t>& buf, unsigned char format, int x, int y)
{
	int numChars = (int)buf.size();
	if(numChars < 1)
		return;

	HFONT font = s_normalFont;
	unsigned int fgColor = MetalBar::s_codePreviewFg;
	unsigned int bgColor = MetalBar::s_codePreviewBg;

	switch(format)
	{
		case FormatType_Plain:
			break;
		case FormatType_Comment:
			fgColor = MetalBar::s_commentColor;
			break;
		case FormatType_Highlight:
			bgColor = MetalBar::s_codePreviewFg;
			fgColor = MetalBar::s_codePreviewBg;
			break;
		case FormatType_Keyword:
			font = s_boldFont;
			break;
	}

	SetBkColor(m_paintDC, RGB_TO_COLORREF(bgColor));
	SetTextColor(m_paintDC, RGB_TO_COLORREF(fgColor));
	SelectObject(m_paintDC, font);

	RECT r = { 1, 1, m_wndWidth - 1, m_wndHeight - 1 };
	ExtTextOutW(m_paintDC, x, y, ETO_CLIPPED, &r, &buf[0], numChars, 0);

	buf.resize(0);
}

void CodePreview::Update(int y, int line)
{
	// Clear the back buffer and draw a border.
	RECT r = { 0, 0, m_wndWidth, m_wndHeight };
	StrokeRect(m_paintDC, MetalBar::s_codePreviewFg, r);
	r.left = 1; r.right -= 1;
	r.top = 1; r.bottom -= 1;
	FillSolidRect(m_paintDC, MetalBar::s_codePreviewBg, r);

	// Draw the text with buffering, because calling ExtTextOut() at each character is way too slow.
	SetBkMode(m_paintDC, OPAQUE);
	int charsPerLine = (m_wndWidth - HORIZ_MARGIN*2) / s_charWidth;
	int numVisLines = (m_wndHeight-VERT_MARGIN*2) / s_lineHeight;
	numVisLines = std::min(numVisLines, m_imgNumLines);

	int startLine = (line - numVisLines / 2);
	startLine = clamp(startLine, 0, m_imgNumLines - numVisLines);

	std::vector<wchar_t> txtBuf;
	txtBuf.reserve(charsPerLine);

	int textY = VERT_MARGIN / 2;
	for(int line = startLine; line < startLine + numVisLines; ++line, textY += s_lineHeight)
	{
		int textX = HORIZ_MARGIN / 2;

		unsigned char currentFormat = FormatType_Plain;
		int bufX = textX;

		for(int col = 0; col < charsPerLine; ++col, textX += s_charWidth)
		{
			const CharInfo& info = m_text[line*charsPerLine + col];
			if(info.format == FormatType_EOL)
				break;

			// Spaces can be merged with whatever format is currently active, except for highlight.
			if( (info.chr == L' ') && (currentFormat != FormatType_Highlight) )
			{
				txtBuf.push_back(L' ');
				continue;
			}

			if(info.format != currentFormat)
			{
				FlushTextBuf(txtBuf, currentFormat, bufX, textY);
				currentFormat = info.format;
				bufX = textX;
			}

			txtBuf.push_back(info.chr);
		}

		FlushTextBuf(txtBuf, currentFormat, bufX, textY);
	}

	// Update the window position.
	y -= m_wndHeight / 2;
	y = clamp(y, m_parentYMin, m_parentYMax - m_wndHeight);

	int x = m_rightEdge - m_wndWidth - 10;
	SetWindowPos(m_hwnd, 0, x, y, 0, 0, SWP_NOSIZE|SWP_NOZORDER);

	// Refresh.
	InvalidateRect(m_hwnd, 0, TRUE);
	UpdateWindow(m_hwnd);
}

void CodePreview::OnPaint(HDC dc)
{
	if(!m_paintDC)
		return;

	BitBlt(dc, 0, 0, m_wndWidth, m_wndHeight, m_paintDC, 0, 0, SRCCOPY);
}

void CodePreview::Resize(int width, int height)
{
	m_wndWidth = width*s_charWidth + HORIZ_MARGIN*2;
	m_wndHeight = height*s_lineHeight + VERT_MARGIN*2;
	SetWindowPos(m_hwnd, 0, 0, 0, m_wndWidth, m_wndHeight, SWP_NOMOVE|SWP_NOZORDER);

	if(m_paintDC)
	{
		DeleteObject(m_codeBmp);
		m_codeBmp = 0;
		DeleteDC(m_paintDC);
		m_paintDC = 0;
	}
}
