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
#include "OptionsDialog.h"
#include "Intervals.h"

#define REFRESH_CODE_TIMER_ID		1
#define REFRESH_CODE_INTERVAL		1000

extern CComPtr<EnvDTE80::DTE2>		g_dte;
extern CComPtr<IVsTextManager>		g_textMgr;

unsigned int MetalBar::s_barWidth;
unsigned int MetalBar::s_whitespaceColor;
unsigned int MetalBar::s_upperCaseColor;
unsigned int MetalBar::s_characterColor;
unsigned int MetalBar::s_commentColor;
unsigned int MetalBar::s_cursorColor;
unsigned int MetalBar::s_matchColor;
unsigned int MetalBar::s_modifiedLineColor;
unsigned int MetalBar::s_unsavedLineColor;
unsigned int MetalBar::s_breakpointColor;
unsigned int MetalBar::s_bookmarkColor;

std::set<MetalBar*> MetalBar::s_bars;

MetalBar::MetalBar(HWND vertBar, HWND editor, HWND horizBar, WNDPROC oldProc, IVsTextView* view)
{
	m_oldProc = oldProc;
	m_hwnd = vertBar;
	m_editorWnd = editor;
	m_horizBar = horizBar;

	m_view = view;
	m_view->AddRef();
	m_numLines = 0;

	m_codeImgDirty = true;
	m_codeImg = 0;
	m_imgDC = 0;
	m_backBufferImg = 0;
	m_backBufferDC = 0;
	m_backBufferBits = 0;
	m_backBufferWidth = 0;
	m_backBufferHeight = 0;

	m_pageSize = 1;
	m_scrollPos = 0;
	m_scrollMin = 0;
	m_scrollMax = 1;
	m_dragging = false;

	s_bars.insert(this);

	AdjustSize(s_barWidth);
}

MetalBar::~MetalBar()
{
	if(m_view)
		m_view->Release();

	if(m_codeImg)
		DeleteObject(m_codeImg);
	if(m_backBufferImg)
		DeleteObject(m_backBufferImg);
	if(m_backBufferDC)
		DeleteObject(m_backBufferDC);
}

void MetalBar::RemoveWndProcHook()
{
	SetWindowLongPtr(m_hwnd, GWL_USERDATA, 0);
	SetWindowLongPtr(m_hwnd, GWL_WNDPROC, (::LONG_PTR)m_oldProc);
}

void MetalBar::RemoveAllBars()
{
	int regularBarWidth = GetSystemMetrics(SM_CXVSCROLL);
	for(std::set<MetalBar*>::iterator it = s_bars.begin(); it != s_bars.end(); ++it)
	{
		MetalBar* bar = *it;
		// We must remove the window proc hook before calling AdjustSize(), otherwise WM_SIZE comes and changes the width again.
		bar->RemoveWndProcHook();
		bar->AdjustSize(regularBarWidth);
		delete bar;
	}
	s_bars.clear();
}

void MetalBar::AdjustSize(unsigned int requiredWidth)
{
	// See if we have the expected width.
	WINDOWPLACEMENT vertBarPlacement;
	vertBarPlacement.length = sizeof(vertBarPlacement);
	GetWindowPlacement(m_hwnd, &vertBarPlacement);
	int width = vertBarPlacement.rcNormalPosition.right - vertBarPlacement.rcNormalPosition.left;
	int diff = requiredWidth - width;
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
		case WM_NCDESTROY:
		{
			RemoveWndProcHook();
			// Remove it from the list of scrollbars.
			std::set<MetalBar*>::iterator it = s_bars.find(this);
			if(it != s_bars.end())
				s_bars.erase(it);
			// Finally delete the object.
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
			AdjustSize(s_barWidth);
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
		{
			OptionsDialog dlg;
			dlg.Execute();
			return 0;
		}

		case WM_TIMER:
		{
			if(wparam != REFRESH_CODE_TIMER_ID)
				break;

			// Remove the timer, invalidate the code image and repaint the control.
			KillTimer(hwnd, REFRESH_CODE_TIMER_ID);
			m_codeImgDirty = true;
			InvalidateRect(hwnd, 0, 0);
			return 0;
		}
	}

	return CallWindowProc(oldProc, hwnd, message, wparam, lparam);
}

void MetalBar::GetHiddenLines(IVsTextLines* buffer, Intervals& hiddenRgn)
{
	CComQIPtr<IServiceProvider> sp = g_dte;
	if(!sp)
		return;

	CComPtr<IVsHiddenTextManager> hiddenMgr;
	HRESULT hr = sp->QueryService(SID_SVsTextManager, IID_IVsHiddenTextManager, (void**)&hiddenMgr);
	if(FAILED(hr) || !hiddenMgr)
		return;

	CComPtr<IVsHiddenTextSession> hiddenSess;
	hr = hiddenMgr->GetHiddenTextSession(buffer, &hiddenSess);
	if(FAILED(hr) || !hiddenSess)
		return;

	CComPtr<IVsEnumHiddenRegions> hiddenEnum;
	hr = hiddenSess->EnumHiddenRegions(FHR_ALL_REGIONS, 0, 0, &hiddenEnum);
	if(FAILED(hr) || !hiddenEnum)
		return;

	ULONG numRegions;
	IVsHiddenRegion* region;
	while( SUCCEEDED(hiddenEnum->Next(1, &region, &numRegions)) && numRegions )
	{
		DWORD state;
		region->GetState(&state);
		if(state == chrDefault)
		{
			TextSpan span;
			region->GetSpan(&span);
			if(span.iStartLine + 1 < span.iEndLine)
				hiddenRgn.Add(span.iStartLine + 1, span.iEndLine);
		}

		region->Release();
	}
}

void MetalBar::FindMarkers(std::vector<unsigned char>& markers, IVsTextLines* buffer, int type, unsigned char flag)
{
	int numLines = (int)markers.size();

	CComPtr<IVsEnumLineMarkers> enumMarkers;
	HRESULT hr = buffer->EnumMarkers(0, 0, numLines, 0, type, 0, &enumMarkers);
	if(FAILED(hr) || !enumMarkers)
		return;

	long numMarkers;
	hr = enumMarkers->GetCount(&numMarkers);
	if(FAILED(hr))
		return;

	for(int m = 0; m < numMarkers; ++m)
	{
		CComPtr<IVsTextLineMarker> marker;
		hr = enumMarkers->Next(&marker);
		if(FAILED(hr))
			break;

		TextSpan span;
		hr = marker->GetCurrentSpan(&span);
		if(FAILED(hr))
			continue;

		// Mark the surrounding lines too, because single-pixel margins are impossible to see.
		int start = std::max((int)span.iStartLine - 2, 0);
		int end = std::min((int)span.iEndLine + 3, numLines);
		for(int i = start; i < end; ++i)
			markers[i] |= flag;
	}
}

bool MetalBar::GetFileName(CComBSTR& name, IVsTextLines* buffer)
{
	// Behold the absolutely ridiculous way of getting the file name for a given text buffer.
	CComPtr<IDispatch> disp;
	HRESULT hr = buffer->CreateEditPoint(0, 0, &disp);
	if(FAILED(hr) || !disp)
		return false;

	CComQIPtr<EditPoint> ep = disp;
	if(!ep)
		return false;

	CComPtr<TextDocument> txtDoc;
	hr = ep->get_Parent(&txtDoc);
	if(FAILED(hr) || !txtDoc)
		return false;

	CComPtr<Document> doc;
	hr = txtDoc->get_Parent(&doc);
	if(FAILED(hr) || !doc)
		return false;

	hr = doc->get_FullName(&name);
	return SUCCEEDED(hr) && name;
}

void MetalBar::FindBreakpoints(std::vector<unsigned char>& markers, IVsTextLines* buffer)
{
	int numLines = (int)markers.size();

	CComBSTR fileName;
	if(!GetFileName(fileName, buffer))
		return;

	CComPtr<Debugger> debugger;
	HRESULT hr = g_dte->get_Debugger(&debugger);
	if(FAILED(hr) || !debugger)
		return;

	CComPtr<Breakpoints> breakpoints;
	hr = debugger->get_Breakpoints(&breakpoints);
	if(FAILED(hr) || !breakpoints)
		return;

	long numBp;
	hr = breakpoints->get_Count(&numBp);
	if(FAILED(hr))
		return;

	for(int bpIdx = 1; bpIdx <= numBp; ++bpIdx)
	{
		CComPtr<Breakpoint> breakpoint;
		hr = breakpoints->Item(CComVariant(bpIdx), &breakpoint);
		if(FAILED(hr) || !breakpoint)
			continue;

		dbgBreakpointLocationType bpType;
		hr = breakpoint->get_LocationType(&bpType);
		if( FAILED(hr) || (bpType != dbgBreakpointLocationTypeFile) )
			continue;

		CComBSTR bpFile;
		hr = breakpoint->get_File(&bpFile);
		if(FAILED(hr) || !bpFile)
			continue;

		if(_wcsicmp(bpFile, fileName) != 0)
			continue;

		long line;
		hr = breakpoint->get_FileLine(&line);
		if(FAILED(hr))
			continue;

		// Mark the surrounding lines too, because single-pixel margins are impossible to see.
		int start = std::max((int)line - 2, 0);
		int end = std::min((int)line + 3, numLines);
		for(int i = start; i < end; ++i)
			markers[i] |= LineMarker_Breakpoint;
	}
}

void MetalBar::GetMarkers(std::vector<unsigned char>& markers, IVsTextLines* buffer, int numLines)
{
	markers.resize(numLines, 0);

	// The magic IDs for the changed lines are not in the MARKERTYPE enum, because they are useful and
	// we wouldn't want people to have access to useful stuff. I found them by disassembling RockScroll.
	FindMarkers(markers, buffer, 0x13, LineMarker_ChangedUnsaved);
	FindMarkers(markers, buffer, 0x14, LineMarker_ChangedSaved);
	FindMarkers(markers, buffer, MARKER_BOOKMARK, LineMarker_Bookmark);

	// Breakpoints must be retrieved in a different way.
	FindBreakpoints(markers, buffer);
}

void MetalBar::PaintMarkers(unsigned int* line, unsigned char flags)
{
	if( (flags & LineMarker_ChangedUnsaved) || (flags & LineMarker_ChangedSaved) )
	{
		// We can get both flags on the same line because we don't mark individual lines, but take surrounding lines
		// too (to improve visibility). In that case, the unsaved marker takes precedence.
		unsigned int color = (flags & LineMarker_ChangedUnsaved) ? s_unsavedLineColor : s_modifiedLineColor;
		line[0] = line[1] = line[2] = color;
	}

	if( (flags & LineMarker_Bookmark) || (flags & LineMarker_Breakpoint) )
	{
		// The breakpoint marker takes precedence in case both are present.
		unsigned int color = (flags & LineMarker_Breakpoint) ? s_breakpointColor : s_bookmarkColor;
		for(int i = 59; i < 64; ++i)
			line[i] = color;
	}
}

void MetalBar::RenderCodeImg()
{
	CComPtr<IVsTextLines> lines;
	HRESULT hr = m_view->GetBuffer(&lines);
	if(FAILED(hr) || !lines)
		return;

	unsigned int tabSize;
	LANGPREFERENCES langPrefs;
	if( SUCCEEDED(lines->GetLanguageServiceID(&langPrefs.guidLang)) && SUCCEEDED(g_textMgr->GetUserPreferences(0, 0, &langPrefs, 0)) )
		tabSize = langPrefs.uTabSize;
	else
		tabSize = 4;

	hr = lines->GetLineCount(&m_numLines);
	if(FAILED(hr))
		return;

	Intervals hiddenRgn;
	GetHiddenLines(lines, hiddenRgn);

	std::vector<unsigned char> markers;
	GetMarkers(markers, lines, m_numLines);

	long numCharsLastLine;
	hr = lines->GetLengthOfLine(m_numLines - 1, &numCharsLastLine);
	if(FAILED(hr))
		return;

	CComBSTR text;
	hr = lines->GetLineText(0, 0, m_numLines - 1, numCharsLastLine, &text);
	if(FAILED(hr) || !text)
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
	
	hiddenRgn.BeginScan();

	int realNumLines = 0;
	int currentTextLine = 0;
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

			// Paint the markers even if the line is hidden. This way, markers inside hidden regions are painted
			// on the line where the region label (ellipsis) is displayed.
			PaintMarkers(pixel, markers[currentTextLine]);

			hiddenRgn.NextValue();
			++currentTextLine;

			if(!hiddenRgn.IsCurrentValueInside())
			{
				// Advance the image pointer.
				linePos = 0;
				pixel -= s_barWidth;
				++realNumLines;
			}
			else
			{
				// Setting linePos to s_barWidth will prevent the code below from trying to draw
				// this line (as it's hidden).
				linePos = s_barWidth;
			}

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

	// Fill the remaining pixels of the first line.
	for(; linePos < s_barWidth; ++linePos)
		pixel[linePos] = s_whitespaceColor;

	m_numLines = realNumLines;

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
		// Re-arm the refresh timer.
		SetTimer(m_hwnd, REFRESH_CODE_TIMER_ID, REFRESH_CODE_INTERVAL, 0);
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

	if(!m_backBufferImg || (m_backBufferWidth != s_barWidth) || (m_backBufferHeight != (unsigned int)barHeight))
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
		m_backBufferWidth = s_barWidth;
		m_backBufferHeight = barHeight;
	}

	// Blit or scale the code image.
	int imgHeight;
	if(m_numLines <= barHeight)
	{
		// Blit the code image and fill the remaining space with the whitespace color.
		imgHeight = m_numLines;
		BitBlt(m_backBufferDC, clRect.left, clRect.top, s_barWidth, m_numLines, m_imgDC, 0, 0, SRCCOPY);
		COLORREF oldColor = SetBkColor(m_backBufferDC, RGB_TO_COLORREF(s_whitespaceColor));
		RECT remainingRect;
		remainingRect.left = clRect.left;
		remainingRect.right = clRect.right;
		remainingRect.top = clRect.top + m_numLines;
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
	unsigned char opacity = (unsigned char)(s_cursorColor >> 24);
	unsigned char cursorColor[3] =
	{
		s_cursorColor & 0xff,
		(s_cursorColor >> 8) & 0xff,
		(s_cursorColor >> 16) & 0xff
	};

	unsigned char* end = (unsigned char*)(m_backBufferBits + (barHeight - normalizedCursor) * s_barWidth);
	unsigned char* pixel = end - normalizedPage * s_barWidth * 4;
	for(; pixel < end; pixel += 4)
	{
		for(int i = 0; i < 3; ++i)
		{
			int p = (opacity*pixel[i])/255 + cursorColor[i];
			pixel[i] = (p <= 255) ? (unsigned char)p : 255;
		}
	}

	// Blit the backbuffer onto the control.
	BitBlt(ctrlDC, clRect.left, clRect.top, s_barWidth, barHeight, m_backBufferDC, 0, 0, SRCCOPY);
}

void MetalBar::ResetSettings()
{
	s_barWidth = 64;
	s_whitespaceColor = 0xfff5f5f5;
	s_upperCaseColor = 0xff101010;
	s_characterColor = 0xff808080;
	s_commentColor = 0xff008000;
	s_cursorColor = 0xe0000028;
	s_matchColor = 0xffd7f600;
	s_modifiedLineColor = 0xff0000ff;
	s_unsavedLineColor = 0xffe1621e;
	s_breakpointColor = 0xffff0000;
	s_bookmarkColor = 0xff0000ff;
}

bool MetalBar::ReadRegInt(unsigned int* to, HKEY key, const char* name)
{
	unsigned int val;
	DWORD type = REG_DWORD;
	DWORD size = sizeof(val);
	if(RegQueryValueExA(key, name, 0, &type, (LPBYTE)&val, &size) != ERROR_SUCCESS)
		return false;

	if(type != REG_DWORD)
		return false;

	*to = val;
	return true;
}

void MetalBar::ReadSettings()
{
	// Make sure we have sane defaults, in case stuff is missing.
	ResetSettings();

	HKEY key;
	// We can't use the constant HKEY_CURRENT_USER because some retard felt the need to define ULONG_PTR inside the EnvDTE
	// namespace, and the compiler reports an ambiguous symbol inside the define.
	if(RegOpenKeyExA((HKEY)0x80000001, "Software\\Griffin Software\\MetalScroll", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
		return;

	ReadRegInt(&s_barWidth, key, "BarWidth");
	ReadRegInt(&s_whitespaceColor, key, "WhitespaceColor");
	ReadRegInt(&s_upperCaseColor, key, "UpperCaseColor");
	ReadRegInt(&s_characterColor, key, "OtherCharColor");
	ReadRegInt(&s_commentColor, key, "CommentColor");
	ReadRegInt(&s_cursorColor, key, "CursorColor");
	ReadRegInt(&s_matchColor, key, "MatchedWordColor");
	ReadRegInt(&s_modifiedLineColor, key, "ModifiedLineColor");
	ReadRegInt(&s_unsavedLineColor, key, "UnsavedLineColor");
	ReadRegInt(&s_breakpointColor, key, "BreakpointColor");
	ReadRegInt(&s_bookmarkColor, key, "BookmarkColor");

	RegCloseKey(key);
}

void MetalBar::WriteRegInt(HKEY key, const char* name, unsigned int val)
{
	RegSetValueEx(key, name, 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
}

void MetalBar::SaveSettings()
{
	HKEY key;
	// See the comments in ReadSettings() about the magic constant.
	if(RegCreateKeyExA((HKEY)0x80000001, "Software\\Griffin Software\\MetalScroll", 0, 0, 0, KEY_SET_VALUE, 0, &key, 0) != ERROR_SUCCESS)
		return;

	WriteRegInt(key, "BarWidth", s_barWidth);
	WriteRegInt(key, "WhitespaceColor", s_whitespaceColor);
	WriteRegInt(key, "UpperCaseColor", s_upperCaseColor);
	WriteRegInt(key, "OtherCharColor", s_characterColor);
	WriteRegInt(key, "CommentColor", s_commentColor);
	WriteRegInt(key, "CursorColor", s_cursorColor);
	WriteRegInt(key, "MatchedWordColor", s_matchColor);
	WriteRegInt(key, "ModifiedLineColor", s_modifiedLineColor);
	WriteRegInt(key, "UnsavedLineColor", s_unsavedLineColor);
	WriteRegInt(key, "BreakpointColor", s_breakpointColor);
	WriteRegInt(key, "BookmarkColor", s_bookmarkColor);

	RegCloseKey(key);

	// Refresh all the scrollbars.
	for(std::set<MetalBar*>::iterator it = s_bars.begin(); it != s_bars.end(); ++it)
	{
		MetalBar* bar = *it;
		bar->m_codeImgDirty = true;
		bar->AdjustSize(s_barWidth);
		InvalidateRect(bar->m_hwnd, 0, 0);
	}
}
