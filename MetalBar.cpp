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
#include "EditCmdFilter.h"
#include "Utils.h"
#include "CodePreview.h"
#include "CppLexer.h"
#include "TextFormatting.h"

#define REFRESH_CODE_TIMER_ID		1
#define REFRESH_CODE_INTERVAL		2000

extern HWND							g_mainVSHwnd;
extern long							g_highlightMarkerType;

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
unsigned int MetalBar::s_requireAltForHighlight;
unsigned int MetalBar::s_codePreviewBg;
unsigned int MetalBar::s_codePreviewFg;
unsigned int MetalBar::s_codePreviewWidth;
unsigned int MetalBar::s_codePreviewHeight;

std::set<MetalBar*> MetalBar::s_bars;

static CodePreview					g_codePreviewWnd;
static bool							g_previewShown = false;

MetalBar::MetalBar(HWND vertBar, HWND editor, HWND horizBar, WNDPROC oldProc, IVsTextView* view)
{
	m_oldProc = oldProc;
	m_hwnd = vertBar;
	m_editorWnd = editor;
	m_horizBar = horizBar;

	m_view = view;
	m_view->AddRef();
	m_numLines = 0;

	m_codeImg = 0;
	m_codeImgHeight = 0;
	m_codeImgDirty = true;
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

	m_editCmdFilter = CEditCmdFilter::AttachFilter(this);

	s_bars.insert(this);
	AdjustSize(s_barWidth);
}

MetalBar::~MetalBar()
{
	// Release the various COM things we used.
	if(m_editCmdFilter)
	{
		m_editCmdFilter->RemoveFilter();
		m_editCmdFilter->Release();
	}
	if(m_view)
		m_view->Release();

	// Free the paint stuff.
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
	int horizBarHeight;
	if(m_horizBar)
	{
		WINDOWPLACEMENT horizBarPlacement;
		horizBarPlacement.length = sizeof(horizBarPlacement);
		GetWindowPlacement(m_horizBar, &horizBarPlacement);
		horizBarPlacement.rcNormalPosition.right -= diff;
		SetWindowPlacement(m_horizBar, &horizBarPlacement);
		horizBarHeight = horizBarPlacement.rcNormalPosition.bottom - horizBarPlacement.rcNormalPosition.top;
	}
	else
		horizBarHeight = 0;

	// Resize the editor window.
	WINDOWPLACEMENT editorPlacement;
	editorPlacement.length = sizeof(editorPlacement);
	GetWindowPlacement(m_editorWnd, &editorPlacement);
	editorPlacement.rcNormalPosition.right -= diff;
	SetWindowPlacement(m_editorWnd, &editorPlacement);

	// Make the vertical bar wider so we can draw our stuff in it. Also expand its height to fill the gap left when
	// shrinking the horizontal bar.
	vertBarPlacement.rcNormalPosition.left -= diff;
	vertBarPlacement.rcNormalPosition.bottom += horizBarHeight;
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

void MetalBar::OnTrackPreview()
{
	POINT mouse;
	GetCursorPos(&mouse);

	RECT clRect;
	GetWindowRect(m_hwnd, &clRect);

	// Determine the line, taking care of the case when the code image is scaled.
	int pixelY = clamp<int>(mouse.y - clRect.top, 0, m_codeImgHeight);
	int line = m_codeImgHeight >= m_numLines ? pixelY : int(1.0f * pixelY * m_numLines / m_codeImgHeight);

	g_codePreviewWnd.Update(clRect.top + pixelY, line);
}

void MetalBar::ShowCodePreview()
{
	CComPtr<IVsTextLines> buffer;
	CComBSTR text;
	long numLines;
	if(!GetBufferAndText(&buffer, &text, &numLines))
		return;

	g_codePreviewWnd.Show(m_hwnd, m_view, buffer, text, numLines);

	g_previewShown = true;
	OnTrackPreview();
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
		{
			AdjustSize(s_barWidth);
			break;
		}

		case WM_ERASEBKGND:
		{
			return 0;
		}

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
				if( (m_scrollMin != si->nMin) || (m_scrollMax != si->nMax) )
				{
					// This event fires when the code changes, when a region is hidden/shown etc. We need to
					// update the code image in all those cases.
					m_codeImgDirty = true;
				}

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
		{
			m_scrollPos = (int)wparam;
			InvalidateRect(hwnd, 0, FALSE);
			return m_scrollPos;
		}

		case SBM_SETRANGE:
		{
			if( (m_scrollMin != (int)wparam) || (m_scrollMax != (int)lparam) )
			{
				m_scrollMin = (int)wparam;
				m_scrollMax = (int)lparam;
				m_codeImgDirty = true;
				InvalidateRect(hwnd, 0, FALSE);
			}
			return m_scrollPos;
		}

		case WM_LBUTTONDOWN:
		{
			m_dragging = true;
			SetCapture(hwnd);
			OnDrag(true);
			return 0;
		}

		case WM_MOUSEMOVE:
		{
			if(m_dragging)
				OnDrag(false);

			if(g_previewShown)
				OnTrackPreview();

			return 0;
		}

		case WM_LBUTTONUP:
		{
			if(m_dragging)
			{
				ReleaseCapture();
				m_dragging = false;
			}
			return 0;
		}

		case WM_LBUTTONDBLCLK:
		{
			OptionsDialog dlg;
			dlg.Execute();
			return 0;
		}

		case WM_MBUTTONDOWN:
		{
			SetCapture(m_hwnd);
			ShowCodePreview();
			return 0;
		}

		case WM_MBUTTONUP:
		{
			if(!g_previewShown)
				return 0;

			ReleaseCapture();
			g_previewShown = false;
			g_codePreviewWnd.Hide();
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

		case (WM_USER + 1):
		{
			// This message is sent by the command filter to inform us we should highlight words matching
			// the current selection.
			HighlightMatchingWords();
			m_codeImgDirty = true;
			InvalidateRect(hwnd, 0, 0);
			return 0;
		}

		case WM_RBUTTONUP:
		case (WM_USER + 2):
		{
			// Right-clicking on the bar or pressing ESC removes the matching word markers. ESC key presses
			// are sent to us by the command filter as WM_USER+2 messages.
			CComPtr<IVsTextLines> buffer;
			HRESULT hr = m_view->GetBuffer(&buffer);
			if(SUCCEEDED(hr) && buffer)
			{
				RemoveWordHighlight(buffer);
				m_codeImgDirty = true;
				InvalidateRect(hwnd, 0, 0);
			}
			// Don't let the default window procedure see right button up events, or it will display the
			// Windows scrollbar menu (scroll here, page up, page down etc.).
			return 0;
		}
	}

	return CallWindowProc(oldProc, hwnd, message, wparam, lparam);
}

bool MetalBar::GetBufferAndText(IVsTextLines** buffer, BSTR* text, long* numLines)
{
	HRESULT hr = m_view->GetBuffer(buffer);
	if(FAILED(hr) || !*buffer)
	{
		static bool warningShown = false;
		if(!warningShown)
		{
			Log("MetalScroll: Failed to get buffer for view 0x%p.", m_view);
			warningShown = true;
		}
		return false;
	}

	hr = (*buffer)->GetLineCount(numLines);
	if(FAILED(hr))
		return false;

	long numCharsLastLine;
	hr = (*buffer)->GetLengthOfLine(*numLines - 1, &numCharsLastLine);
	if(FAILED(hr))
		return false;

	hr = (*buffer)->GetLineText(0, 0, *numLines - 1, numCharsLastLine, text);
	return SUCCEEDED(hr) && (*text);
}

void MetalBar::PaintLineFlags(unsigned int* img, int line, int imgHeight, unsigned int flags)
{
	int startLine = std::max(0, line - 2);
	int endLine = std::min(imgHeight, line + 3);

	// Left margin flags.
	if( (flags & LineFlag_ChangedUnsaved) || (flags & LineFlag_ChangedSaved) )
	{
		unsigned int color = (flags & LineFlag_ChangedUnsaved) ? s_unsavedLineColor : s_modifiedLineColor;
		for(int i = startLine; i < endLine; ++i)
		{
			for(int j = 0; j < 3; ++j)
				img[i*s_barWidth + j] = color;
		}
	}

	// Right margin flags.
	unsigned int color;
	if(flags & LineFlag_Match)
		color = s_matchColor;
	else if(flags & LineFlag_Breakpoint)
		color = s_breakpointColor;
	else if(flags & LineFlag_Bookmark)
		color = s_bookmarkColor;
	else
		return;

	for(int i = startLine; i < endLine; ++i)
	{
		for(int j = s_barWidth - 5; j < (int)s_barWidth; ++j)
			img[i*s_barWidth + j] = color;
	}
}

struct BarRenderOp : public RenderOperator
{
	typedef std::vector<std::pair<unsigned int, unsigned int> > MarkedLineList;

	BarRenderOp(std::vector<unsigned int>& _imgBuffer, MarkedLineList& _markedLines) : imgBuffer(_imgBuffer), markedLines(_markedLines) {}

	void Init(int numLines)
	{
		imgBuffer.reserve(numLines*MetalBar::s_barWidth);
		imgBuffer.resize(MetalBar::s_barWidth);
		markedLines.reserve(numLines);
	}

	void EndLine(int line, int lastColumn, unsigned int lineFlags, bool textEnd)
	{
		// Fill the remaining pixels with the whitespace color.
		for(int i = lastColumn; i < (int)MetalBar::s_barWidth; ++i)
			imgBuffer[line*MetalBar::s_barWidth + i] = MetalBar::s_whitespaceColor;

		if(lineFlags)
			markedLines.push_back(std::pair<unsigned int, unsigned int>(line, lineFlags));

		if(!textEnd)
		{
			// Advance the image pointer.
			imgBuffer.resize((line + 2) * MetalBar::s_barWidth);
		}
	}

	void RenderSpaces(int line, int column, int count)
	{
		for(int i = column; (i < column + count) && (i < (int)MetalBar::s_barWidth); ++i)
			imgBuffer[line*MetalBar::s_barWidth + i] = MetalBar::s_whitespaceColor;
	}

	void RenderCharacter(int line, int column, wchar_t chr, unsigned int flags)
	{
		if(column >= (int)MetalBar::s_barWidth)
			return;

		unsigned int color;
		if(flags & TextFlag_Highlight)
			color = MetalBar::s_matchColor;
		else if(flags & TextFlag_Comment)
			color = MetalBar::s_commentColor;
		else if( (chr >= 'A') && (chr <= 'Z') )
			color = MetalBar::s_upperCaseColor;
		else
			color = MetalBar::s_characterColor;

		imgBuffer[line*MetalBar::s_barWidth + column] = color;
	}

	std::vector<unsigned int>& imgBuffer;
	MarkedLineList& markedLines;
};

void MetalBar::RefreshCodeImg(int barHeight)
{
	// Get the text buffer.
	CComPtr<IVsTextLines> buffer;
	CComBSTR text;
	long numLines;
	if(!GetBufferAndText(&buffer, &text, &numLines))
		return;

	// Paint the code representation.
	std::vector<unsigned int> imgBuffer;
	BarRenderOp::MarkedLineList markedLines;
	BarRenderOp renderOp(imgBuffer, markedLines);
	m_numLines = RenderText(renderOp, m_view, buffer, text, numLines);
	m_codeImgHeight = m_numLines < barHeight ? m_numLines : barHeight;

	// Create a bitmap and put the code image inside, scaling it if necessary.
	if(m_codeImg)
		DeleteObject(m_codeImg);

	BITMAPINFO bi;
	memset(&bi, 0, sizeof(bi));
	bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
	bi.bmiHeader.biWidth = s_barWidth;
	bi.bmiHeader.biHeight = m_codeImgHeight;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	unsigned int* bmpBits = 0;
	m_codeImg = CreateDIBSection(0, &bi, DIB_RGB_COLORS, (void**)&bmpBits, 0, 0);

	float lineScaleFactor;
	if(m_numLines < barHeight)
	{
		lineScaleFactor = 1.0f;
		// Flip the image while copying it.
		std::vector<unsigned int>::iterator line1 = imgBuffer.begin();
		unsigned int* line2 = bmpBits + (m_numLines - 1)*s_barWidth;
		for(int i = 0; i < m_numLines; ++i)
		{
			std::copy(line1, line1 + s_barWidth, line2);
			line1 += s_barWidth;
			line2 -= s_barWidth;
		}
	}
	else
	{
		lineScaleFactor = 1.0f * barHeight / m_numLines;
		// Scale.
		FlipScaleImageVertically(bmpBits, barHeight, &imgBuffer[0], m_numLines, s_barWidth);
	}

	// Paint the line flags directly on the final image, which might have been scaled. By doing it in
	// a separate pass (instead of painting the markers while generating the code image) we get rid of
	// a bunch of complications and we make sure that the size of the markers stays fixed regardless
	// of image scaling.
	for(int i = 0; i < (int)markedLines.size(); ++i)
	{
		int imgLine = int(lineScaleFactor * markedLines[i].first);
		// Flip it, since BMPs are upside down.
		imgLine = m_codeImgHeight - imgLine - 1;
		PaintLineFlags(bmpBits, imgLine, m_codeImgHeight, markedLines[i].second);
	}

	if(!m_imgDC)
		m_imgDC = CreateCompatibleDC(0);
	SelectObject(m_imgDC, m_codeImg);
}

void MetalBar::OnPaint(HDC ctrlDC)
{
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

	// If the bar height has changed and we have more lines than vertical pixels, redraw the code image.
	if( (m_numLines > barHeight) && (m_codeImgHeight != barHeight) )
		m_codeImgDirty = true;

	// Don't refresh the image while the preview is shown.
	if(!m_codeImg || (m_codeImgDirty && !g_previewShown))
	{
		m_codeImgDirty = false;
		RefreshCodeImg(barHeight);
		// Re-arm the refresh timer.
		SetTimer(m_hwnd, REFRESH_CODE_TIMER_ID, REFRESH_CODE_INTERVAL, 0);
	}

	// Blit the code image and fill the remaining space with the whitespace color.
	BitBlt(m_backBufferDC, clRect.left, clRect.top, s_barWidth, m_codeImgHeight, m_imgDC, 0, 0, SRCCOPY);
	RECT remainingRect = clRect;
	remainingRect.top += m_codeImgHeight;
	FillSolidRect(m_backBufferDC, s_whitespaceColor, remainingRect);

	// Compute the size and position of the current page marker.
	int range = m_scrollMax - m_scrollMin - m_pageSize + 2;
	int cursor = m_scrollPos - m_scrollMin;
	float normFact = (m_numLines < barHeight) ? 1.0f : 1.0f * barHeight / range;
	int normalizedCursor = int(cursor * normFact);
	int normalizedPage = int(m_pageSize * normFact);
	normalizedPage = std::max(15, normalizedPage);
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
	s_matchColor = 0xffff8000;
	s_modifiedLineColor = 0xff0000ff;
	s_unsavedLineColor = 0xffe1621e;
	s_breakpointColor = 0xffff0000;
	s_bookmarkColor = 0xff0000ff;
	s_requireAltForHighlight = TRUE;
	s_codePreviewBg = 0xffffffe1;
	s_codePreviewFg = 0xff000000;
	s_codePreviewWidth = 80;
	s_codePreviewHeight = 15;
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
	if(RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Griffin Software\\MetalScroll", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
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
	ReadRegInt(&s_requireAltForHighlight, key, "RequireALT");
	ReadRegInt(&s_codePreviewFg, key, "CodePreviewFg");
	ReadRegInt(&s_codePreviewBg, key, "CodePreviewBg");
	ReadRegInt(&s_codePreviewWidth, key, "CodePreviewWidth");
	ReadRegInt(&s_codePreviewHeight, key, "CodePreviewHeight");

	RegCloseKey(key);
}

void MetalBar::WriteRegInt(HKEY key, const char* name, unsigned int val)
{
	RegSetValueExA(key, name, 0, REG_DWORD, (LPBYTE)&val, sizeof(val));
}

void MetalBar::SaveSettings()
{
	HKEY key;
	if(RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Griffin Software\\MetalScroll", 0, 0, 0, KEY_SET_VALUE, 0, &key, 0) != ERROR_SUCCESS)
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
	WriteRegInt(key, "RequireALT", s_requireAltForHighlight);
	WriteRegInt(key, "CodePreviewFg", s_codePreviewFg);
	WriteRegInt(key, "CodePreviewBg", s_codePreviewBg);
	WriteRegInt(key, "CodePreviewWidth", s_codePreviewWidth);
	WriteRegInt(key, "CodePreviewHeight", s_codePreviewHeight);

	RegCloseKey(key);

	// Refresh all the scrollbars.
	for(std::set<MetalBar*>::iterator it = s_bars.begin(); it != s_bars.end(); ++it)
	{
		MetalBar* bar = *it;
		bar->m_codeImgDirty = true;
		bar->AdjustSize(s_barWidth);
		InvalidateRect(bar->m_hwnd, 0, 0);
	}

	// Resize the code preview window.
	g_codePreviewWnd.Resize(s_codePreviewWidth, s_codePreviewHeight);
}

void MetalBar::RemoveWordHighlight(IVsTextLines* buffer)
{
	struct DeleteMarkerOp : public MarkerOperator
	{
		void Process(IVsTextLineMarker* marker, int /*idx*/) const
		{
			marker->Invalidate();
		}
	};

	ProcessLineMarkers(buffer, g_highlightMarkerType, DeleteMarkerOp());
	m_highlightWord = (wchar_t*)0;
}

void MetalBar::HighlightMatchingWords()
{
	CComPtr<IVsTextLines> buffer;
	CComBSTR allText;
	long numLines;
	if(!GetBufferAndText(&buffer, &allText, &numLines))
		return;

	RemoveWordHighlight(buffer);

	HRESULT hr = m_view->GetSelectedText(&m_highlightWord);
	if(FAILED(hr) || !m_highlightWord)
	{
		m_highlightWord = (wchar_t*)0;
		return;
	}

	unsigned int selTextLen = m_highlightWord.Length();
	if(selTextLen < 1)
	{
		m_highlightWord = (wchar_t*)0;
		return;
	}

	int line = 0;
	int column = 0;
	for(wchar_t* chr = allText; *chr; ++chr)
	{
		// Check for newline.
		if( (chr[0] == L'\r') || (chr[0] == L'\n') )
		{
			// In case of CRLF, eat the next character.
			if( (chr[0] == L'\r') && (chr[1] == L'\n') )
				++chr;
			++line;
			column = 0;
			continue;
		}

		if( (wcsncmp(chr, m_highlightWord, selTextLen) == 0) && (chr == allText || IsCppIdSeparator(chr[-1])) && IsCppIdSeparator(chr[selTextLen]) )
		{
			buffer->CreateLineMarker(g_highlightMarkerType, line, column, line, column + selTextLen, 0, 0);
			// Make sure we don't create overlapping markers.
			chr += selTextLen - 1;
			column += selTextLen - 1;
		}

		++column;
	}
}

void MetalBar::Init()
{
	ReadSettings();

	InitScaler();
	CodePreview::Register();
	OptionsDialog::Init();

	g_codePreviewWnd.Create(g_mainVSHwnd, s_codePreviewWidth, s_codePreviewHeight);
}

void MetalBar::Uninit()
{
	g_codePreviewWnd.Destroy();

	RemoveAllBars();
	CodePreview::Unregister();
	OptionsDialog::Uninit();
}
