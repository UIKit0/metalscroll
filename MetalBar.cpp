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
#include "MetalBar.h"

unsigned int MetalBar::s_barWidth = 64;
unsigned int MetalBar::s_whitespaceColor = 0xfff5f5f5;
unsigned int MetalBar::s_upperCaseColor = 0xff101010;
unsigned int MetalBar::s_characterColor = 0xff808080;
unsigned int MetalBar::s_commentColor = 0xff008000;
unsigned char MetalBar::s_pageColor[3] = { 0x28, 0x00, 0x00 };
unsigned char MetalBar::s_pageOpacity = 224;
unsigned int MetalBar::s_matchColor = 0xffd7f600;
unsigned int MetalBar::s_addedLineColor = 0xff0000ff;
unsigned int MetalBar::s_unsavedLineColor = 0xffe1621e;

MetalBar::MetalBar(HWND vertBar, HWND editor, HWND horizBar, WNDPROC oldProc, TextDocument* doc)
{
	m_oldProc = oldProc;
	m_hwnd = vertBar;
	m_editorWnd = editor;
	m_horizBar = horizBar;

	m_doc = doc;
	m_doc->AddRef();
	m_numLines = 0;

	m_codeImgDirty = true;
	m_codeImg = 0;
	m_imgDC = 0;
	m_backBufferImg = 0;
	m_backBufferDC = 0;
	m_backBufferBits = 0;
	m_backBufferHeight = 0;

	m_pageSize = 1;
	m_scrollPos = 0;
	m_scrollMin = 0;
	m_scrollMax = 1;
	m_dragging = false;
}

MetalBar::~MetalBar()
{
	// Restore the original window procedure.
	SetWindowLongPtr(m_hwnd, GWL_USERDATA, 0);
	SetWindowLongPtr(m_hwnd, GWL_WNDPROC, (::LONG_PTR)m_oldProc);

	// Free the document.
	if(m_doc)
		m_doc->Release();

	// Free the paint stuff.
	if(m_codeImg)
		DeleteObject(m_codeImg);
	if(m_backBufferImg)
		DeleteObject(m_backBufferImg);
	if(m_backBufferDC)
		DeleteObject(m_backBufferDC);
}

void MetalBar::AdjustSize()
{
	// See if we have the expected width.
	WINDOWPLACEMENT vertBarPlacement;
	vertBarPlacement.length = sizeof(vertBarPlacement);
	GetWindowPlacement(m_hwnd, &vertBarPlacement);
	int width = vertBarPlacement.rcNormalPosition.right - vertBarPlacement.rcNormalPosition.left;
	int diff = s_barWidth - width;
	if(diff == 0)
		return;

	// Resize the horizontal scroll bar.
	WINDOWPLACEMENT horizBarPlacement;
	horizBarPlacement.length = sizeof(horizBarPlacement);
	GetWindowPlacement(m_horizBar, &horizBarPlacement);
	horizBarPlacement.rcNormalPosition.right -= diff;
	SetWindowPlacement(m_horizBar, &horizBarPlacement);

	// Resize the editor window.
	WINDOWPLACEMENT editorPlacement;
	editorPlacement.length = sizeof(editorPlacement);
	GetWindowPlacement(m_editorWnd, &editorPlacement);
	editorPlacement.rcNormalPosition.right -= diff;
	SetWindowPlacement(m_editorWnd, &editorPlacement);

	// Make the vertical bar wider so we can draw our stuff in it. Also expand its height to fill the gap left when
	// shrinking the horizontal bar.
	vertBarPlacement.rcNormalPosition.left -= diff;
	vertBarPlacement.rcNormalPosition.bottom += horizBarPlacement.rcNormalPosition.bottom - horizBarPlacement.rcNormalPosition.top;
	SetWindowPlacement(m_hwnd, &vertBarPlacement);
}

void MetalBar::OnDrag(bool initial)
{
	POINT mouse;
	GetCursorPos(&mouse);
	ScreenToClient(m_hwnd, &mouse);

	RECT clRect;
	GetClientRect(m_hwnd, &clRect);

	int cursor = mouse.y - clRect.top;
	int barHeight = clRect.bottom - clRect.top;
	float normFactor = (m_numLines > barHeight) ? 1.0f * m_numLines / barHeight : 1.0f;
	int line = int(cursor * normFactor);

	line -= m_pageSize / 2;
	line = (line >= 0) ? line : 0;
	line = (line < m_numLines) ? line : m_numLines - 1;

	SetScrollPos(m_hwnd, SB_CTL, line, TRUE);

	HWND parent = GetParent(m_hwnd);
	WPARAM wparam = (line << 16);
	wparam |= initial ? SB_THUMBPOSITION : SB_THUMBTRACK;
	PostMessage(parent, WM_VSCROLL, wparam, (LPARAM)m_hwnd);
	if(initial)
		PostMessage(parent, WM_VSCROLL, SB_ENDSCROLL, (LPARAM)m_hwnd);
}

LRESULT MetalBar::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	WNDPROC oldProc = m_oldProc;

	switch(message)
	{
		case WM_DESTROY:
		{
			delete this;
			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			if(hdc)
			{
				OnPaint(hdc);
				EndPaint(hwnd, &ps);
			}
			return 0;
		}

		case WM_WINDOWPOSCHANGED:
		case WM_SIZE:
			AdjustSize();
			break;

		case WM_ERASEBKGND:
			return 0;

		// Don't let the system see the SBM_ and mouse messages because it will draw the thumb over us.
		case SBM_SETSCROLLINFO:
		{
			SCROLLINFO* si = (SCROLLINFO*)lparam;
			if(si->fMask & SIF_PAGE)
				m_pageSize = si->nPage;
			if(si->fMask & SIF_POS)
				m_scrollPos = si->nPos;
			if(si->fMask & SIF_RANGE)
			{
				m_scrollMin = si->nMin;
				m_scrollMax = si->nMax;
			}

			// Call the default window procedure so that the scrollbar stores the position and range internally. Pass
			// 0 as wparam so it doesn't draw the thumb by itself.
			LRESULT res = CallWindowProc(oldProc, hwnd, message, 0, lparam);
			if(wparam)
			{
				InvalidateRect(hwnd, 0, TRUE);
				UpdateWindow(hwnd);
			}
			
			return res;
		}

		case SBM_SETPOS:
			m_scrollPos = (int)wparam;
			InvalidateRect(hwnd, 0, FALSE);
			return m_scrollPos;

		case SBM_SETRANGE:
			m_scrollMin = (int)wparam;
			m_scrollMax = (int)lparam;
			return m_scrollPos;

		case WM_LBUTTONDOWN:
			m_dragging = true;
			SetCapture(hwnd);
			OnDrag(true);
			return 0;

		case WM_MOUSEMOVE:
			if(m_dragging)
				OnDrag(false);
			return 0;

		case WM_LBUTTONUP:
			if(m_dragging)
			{
				ReleaseCapture();
				m_dragging = false;
			}
			return 0;

		case WM_LBUTTONDBLCLK:
			return 0;
	}

	return CallWindowProc(oldProc, hwnd, message, wparam, lparam);
}

void MetalBar::RenderCodeImg()
{
	long tabSize;
	HRESULT hr = m_doc->get_TabSize(&tabSize);
	if(FAILED(hr))
		return;

	CComPtr<EditPoint> startEp;
	hr = m_doc->CreateEditPoint(0, &startEp);
	if(FAILED(hr) || !startEp)
		return;

	CComPtr<TextPoint> endTp;
	hr = m_doc->get_EndPoint(&endTp);
	if(FAILED(hr) || !endTp)
		return;

	CComBSTR text;
	hr = startEp->GetText(CComVariant(endTp), &text);
	if(FAILED(hr) || !text)
		return;

	hr = endTp->get_Line(&m_numLines);
	if(FAILED(hr))
		return;

	if(m_numLines < 1)
		m_numLines = 1;

	if(m_codeImg)
		DeleteObject(m_codeImg);
	
	BITMAPINFO bi;
	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = s_barWidth;
	bi.bmiHeader.biHeight = m_numLines;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	unsigned int* img = 0;
	m_codeImg = CreateDIBSection(0, &bi, DIB_RGB_COLORS, (void**)&img, 0, 0);

	unsigned int linePos = 0;
	// Bitmaps are upside down for some retarded reason.
	unsigned int* pixel = img + (m_numLines - 1)*s_barWidth;

	enum CommentType
	{
		CommentType_None,
		CommentType_SingleLine,
		CommentType_MultiLine
	} commentType = CommentType_None;
	
	for(wchar_t* chr = text; *chr; ++chr)
	{
		// Check for newline.
		if( (chr[0] == L'\r') || (chr[0] == L'\n') )
		{
			// In case of CRLF, eat the next character.
			if( (chr[0] == L'\r') && (chr[1] == L'\n') )
				++chr;

			// Fill the rest of the line with white.
			for(unsigned int i = linePos; i < s_barWidth; ++i)
				pixel[i] = s_whitespaceColor;

			linePos = 0;
			pixel -= s_barWidth;
			if(commentType == CommentType_SingleLine)
				commentType = CommentType_None;
			continue;
		}

		unsigned int color = s_whitespaceColor;
		int numChars = 1;
		if(*chr > L' ')
		{
			switch(commentType)
			{
				case CommentType_None:
					if( (chr[0] == L'/') && (chr[1] == L'/') )
					{
						color = s_commentColor;
						commentType = CommentType_SingleLine;
					}
					else if( (chr[0] == L'/') && (chr[1] == L'*') )
					{
						color = s_commentColor;
						commentType = CommentType_MultiLine;
					}
					else if( (*chr >= L'A') && (*chr <= L'Z') )
						color = s_upperCaseColor;
					else
						color = s_characterColor;
					break;

				case CommentType_SingleLine:
					color = s_commentColor;
					break;

				case CommentType_MultiLine:
					color = s_commentColor;
					if( (chr[0] == L'*') && (chr[1] == L'/') )
					{
						commentType = CommentType_None;
						// The slash must be painted with the comment color too, so skip it and draw two dots.
						numChars = 2;
						++chr;
					}
					break;
			}
		}
		else
		{
			color = s_whitespaceColor;
			if(*chr == L'\t')
				numChars = (int)tabSize;
		}

		for(int i = 0; i < numChars; ++i)
		{
			// We can draw at most s_barWidth characters per line.
			if(linePos >= s_barWidth)
				break;

			pixel[linePos] = color;
			++linePos;
		}
	}

	if(!m_imgDC)
		m_imgDC = CreateCompatibleDC(0);
	SelectObject(m_imgDC, m_codeImg);
}

void MetalBar::OnPaint(HDC ctrlDC)
{
	if(!m_codeImg || m_codeImgDirty)
	{
		RenderCodeImg();
		m_codeImgDirty = false;
	}

	BLENDFUNCTION blendFunc;
	blendFunc.BlendOp = AC_SRC_OVER;
	blendFunc.BlendFlags = 0;
	blendFunc.SourceConstantAlpha = 255;
	blendFunc.AlphaFormat = AC_SRC_ALPHA;

	RECT clRect;
	GetClientRect(m_hwnd, &clRect);
	int barHeight = clRect.bottom - clRect.top;

	if(!m_backBufferDC)
		m_backBufferDC = CreateCompatibleDC(ctrlDC);

	if(!m_backBufferImg || (m_backBufferHeight != barHeight))
	{
		if(m_backBufferImg)
			DeleteObject(m_backBufferImg);

		BITMAPINFO bi;
		memset(&bi, 0, sizeof(bi));
		bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
		bi.bmiHeader.biWidth = s_barWidth;
		bi.bmiHeader.biHeight = barHeight;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;
		m_backBufferImg = CreateDIBSection(0, &bi, DIB_RGB_COLORS, (void**)&m_backBufferBits, 0, 0);
		SelectObject(m_backBufferDC, m_backBufferImg);
		m_backBufferHeight = barHeight;
	}

	// Blit or scale the code image.
	int imgHeight;
	if(m_numLines <= barHeight)
	{
		// Blit the code image and fill the remaining space with the whitespace color.
		imgHeight = m_numLines;
		BitBlt(m_backBufferDC, clRect.left, clRect.top, s_barWidth, m_numLines, m_imgDC, 0, 0, SRCCOPY);
		// GDI has R and B flipped and wants alpha to be 0.
		COLORREF fillColor =
			(s_whitespaceColor & 0xff00) |
			( (s_whitespaceColor & 0xff) << 16) |
			( (s_whitespaceColor & 0xff0000) >> 16);
		COLORREF oldColor = SetBkColor(m_backBufferDC, fillColor);
		RECT remainingRect;
		remainingRect.left = clRect.left;
		remainingRect.right = clRect.right;
		remainingRect.top = clRect.top + m_numLines - 1;
		remainingRect.bottom = clRect.bottom;
		// Wondrous hack (c) MFC: fill a rect with ExtTextOut(), since it doesn't require us to create a brush.
		ExtTextOut(m_backBufferDC, 0, 0, ETO_OPAQUE, &remainingRect, NULL, 0, NULL);
		SetBkColor(m_backBufferDC, oldColor);
	}
	else
	{
		// Scale the image to fit the available space.
		AlphaBlend(m_backBufferDC, clRect.left, clRect.top, s_barWidth, barHeight, m_imgDC, 0, 0, s_barWidth, m_numLines, blendFunc);
		imgHeight = barHeight;
	}

	// Compute the size and position of the current page marker.
	int range = m_scrollMax - m_scrollMin - m_pageSize + 2;
	int cursor = m_scrollPos - m_scrollMin;
	float normFact = 1.0f * imgHeight / range;
	int normalizedCursor = int(cursor * normFact);
	int normalizedPage = int(m_pageSize * normFact);
	if(normalizedPage > barHeight)
		normalizedPage = barHeight;
	if(clRect.top + normalizedCursor + normalizedPage > clRect.bottom)
		normalizedCursor = barHeight - normalizedPage;

	// Overlay the current page marker. Since the bitmap is upside down, we must flip the cursor.
	unsigned char* end = (unsigned char*)(m_backBufferBits + (barHeight - normalizedCursor) * s_barWidth);
	unsigned char* pixel = end - normalizedPage * s_barWidth * 4;
	for(; pixel < end; pixel += 4)
	{
		for(int i = 0; i < 3; ++i)
		{
			int p = (s_pageOpacity*pixel[i])/255 + s_pageColor[i];
			pixel[i] = (p <= 255) ? (unsigned char)p : 255;
		}
	}

	// Blit the backbuffer onto the control.
	BitBlt(ctrlDC, clRect.left, clRect.top, s_barWidth, barHeight, m_backBufferDC, 0, 0, SRCCOPY);
}

void MetalBar::OnCodeChanged(TextPoint* /*startPoint*/, TextPoint* /*endPoint*/)
{
	m_codeImgDirty = true;
	InvalidateRect(m_hwnd, 0, 0);
}
