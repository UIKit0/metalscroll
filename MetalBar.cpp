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

#define REFRESH_CODE_TIMER_ID		1
#define REFRESH_CODE_INTERVAL		2000

extern CComPtr<EnvDTE80::DTE2>		g_dte;
extern CComPtr<IVsTextManager>		g_textMgr;
extern long							g_highlightMarkerType;
extern HWND							g_mainVSHwnd;

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
	m_tabSize = 4;

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

void MetalBar::OnTrackPreview()
{
	POINT mouse;
	GetCursorPos(&mouse);

	RECT clRect;
	GetWindowRect(m_hwnd, &clRect);

	// Determine the line, taking care of the case when the code image is scaled.
	int codeImgHeight = std::min((int)m_backBufferHeight, (int)m_numLines);
	int clampedY = clamp<int>(mouse.y - clRect.top, 0, codeImgHeight);

	int line;
	if((int)m_backBufferHeight < m_numLines)
		line = int(1.0f * clampedY * m_numLines / m_backBufferHeight);
	else
		line = clampedY;
	
	CComPtr<IVsTextLines> buffer;
	HRESULT hr = m_view->GetBuffer(&buffer);
	if(FAILED(hr) || !buffer)
		return;

	long numLines;
	hr = buffer->GetLineCount(&numLines);
	if(FAILED(hr))
		return;

	CComBSTR text;
	int minLine = std::max<int>(0, line - s_codePreviewHeight / 2);
	int maxLine = std::min<int>((int)numLines - 1, minLine + s_codePreviewHeight);
	hr = buffer->GetLineText(minLine, 0, maxLine, 0, &text);
	if(FAILED(hr) || !text)
		return;

	g_codePreviewWnd.Update(clRect.top + clampedY, text, m_tabSize);
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
			g_previewShown = true;
			OnTrackPreview();
			g_codePreviewWnd.Show(m_hwnd);
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

void MetalBar::FindHiddenLines(LineList& lines, IVsTextLines* buffer)
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
			for(int l = span.iStartLine + 1; l <= span.iEndLine; ++l)
				lines[l].flags |= LineMarker_Hidden;
		}

		region->Release();
	}
}

void MetalBar::MarkLineRange(LineList& lines, unsigned int flag, int start, int end)
{
	// Mark the surrounding lines too, because single-pixel margins are impossible to see.
	int extraLines;
	if((int)m_backBufferHeight < m_numLines)
		extraLines = int(2.0f * m_numLines / m_backBufferHeight);
	else
		extraLines = 2;

	start = std::max(start - extraLines, 0);
	end = std::min(end + extraLines, (int)m_numLines - 1);
	for(int i = start; i <= end; ++i)
		lines[i].flags |= flag;
}

void MetalBar::ProcessLineMarkers(IVsTextLines* buffer, int type, const MarkerOperator& op)
{
	long numLines;
	HRESULT hr = buffer->GetLineCount(&numLines);
	if(FAILED(hr))
		return;

	CComPtr<IVsEnumLineMarkers> enumMarkers;
	hr = buffer->EnumMarkers(0, 0, numLines, 0, type, 0, &enumMarkers);
	if(FAILED(hr) || !enumMarkers)
		return;

	long numMarkers;
	hr = enumMarkers->GetCount(&numMarkers);
	if(FAILED(hr))
		return;

	op.NotifyCount(numMarkers);

	for(int idx = 0; idx < numMarkers; ++idx)
	{
		CComPtr<IVsTextLineMarker> marker;
		hr = enumMarkers->Next(&marker);
		if(FAILED(hr))
			break;

		op.Process(marker, idx);
	}
}

bool MetalBar::GetFileName(CComBSTR& name, IVsTextLines* buffer)
{
	// Behold the absolutely ridiculous way of getting the file name for a given text buffer.
	CComPtr<IDispatch> disp;
	HRESULT hr = buffer->CreateEditPoint(0, 0, &disp);
	if(FAILED(hr) || !disp)
		return false;

	CComQIPtr<EnvDTE::EditPoint> ep = disp;
	if(!ep)
		return false;

	CComPtr<EnvDTE::TextDocument> txtDoc;
	hr = ep->get_Parent(&txtDoc);
	if(FAILED(hr) || !txtDoc)
		return false;

	CComPtr<EnvDTE::Document> doc;
	hr = txtDoc->get_Parent(&doc);
	if(FAILED(hr) || !doc)
		return false;

	hr = doc->get_FullName(&name);
	return SUCCEEDED(hr) && name;
}

void MetalBar::FindBreakpoints(LineList& lines, IVsTextLines* buffer)
{
	CComBSTR fileName;
	if(!GetFileName(fileName, buffer))
		return;

	CComPtr<EnvDTE::Debugger> debugger;
	HRESULT hr = g_dte->get_Debugger(&debugger);
	if(FAILED(hr) || !debugger)
		return;

	CComPtr<EnvDTE::Breakpoints> breakpoints;
	hr = debugger->get_Breakpoints(&breakpoints);
	if(FAILED(hr) || !breakpoints)
		return;

	long numBp;
	hr = breakpoints->get_Count(&numBp);
	if(FAILED(hr))
		return;

	for(int bpIdx = 1; bpIdx <= numBp; ++bpIdx)
	{
		CComPtr<EnvDTE::Breakpoint> breakpoint;
		hr = breakpoints->Item(CComVariant(bpIdx), &breakpoint);
		if(FAILED(hr) || !breakpoint)
			continue;

		EnvDTE::dbgBreakpointLocationType bpType;
		hr = breakpoint->get_LocationType(&bpType);
		if( FAILED(hr) || (bpType != EnvDTE::dbgBreakpointLocationTypeFile) )
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

		MarkLineRange(lines, LineMarker_Breakpoint, line, line);
	}
}

void MetalBar::GetLineFlags(LineList& lines, IVsTextLines* buffer)
{
	FindHiddenLines(lines, buffer);

	struct MarkRangeOp : public MarkerOperator
	{
		MarkRangeOp(MetalBar* bar_, LineList& lines_, unsigned int flag_) : bar(bar_), lines(lines_), flag(flag_) {}

		void Process(IVsTextLineMarker* marker, int /*idx*/) const
		{
			TextSpan span;
			marker->GetCurrentSpan(&span);
			bar->MarkLineRange(lines, flag, span.iStartLine, span.iEndLine);
		}

		MetalBar* bar;
		LineList& lines;
		unsigned int flag;
	};

	// The magic IDs for the changed lines are not in the MARKERTYPE enum, because they are useful and
	// we wouldn't want people to have access to useful stuff. I found them by disassembling RockScroll.
	ProcessLineMarkers(buffer, 0x13, MarkRangeOp(this, lines, LineMarker_ChangedUnsaved));
	ProcessLineMarkers(buffer, 0x14, MarkRangeOp(this, lines, LineMarker_ChangedSaved));
	ProcessLineMarkers(buffer, MARKER_BOOKMARK, MarkRangeOp(this, lines, LineMarker_Bookmark));

	// Breakpoints must be retrieved in a different way.
	FindBreakpoints(lines, buffer);
}

void MetalBar::PaintLineFlags(unsigned int* line, unsigned int flags)
{
	// Left margin flags.
	if( (flags & LineMarker_ChangedUnsaved) || (flags & LineMarker_ChangedSaved) )
	{
		// We can get both flags on the same line because we don't mark individual lines, but take surrounding lines
		// too (to improve visibility). In that case, the unsaved marker takes precedence.
		unsigned int color = (flags & LineMarker_ChangedUnsaved) ? s_unsavedLineColor : s_modifiedLineColor;
		line[0] = line[1] = line[2] = color;
	}

	// Right margin flags.
	unsigned int color;
	if(flags & LineMarker_Match)
		color = s_matchColor;
	else if(flags & LineMarker_Breakpoint)
		color = s_breakpointColor;
	else if(flags & LineMarker_Bookmark)
		color = s_bookmarkColor;
	else
		return;

	for(unsigned int i = s_barWidth - 5; i < s_barWidth; ++i)
		line[i] = color;
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

void MetalBar::GetHighlights(LineList& lines, IVsTextLines* buffer, HighlightList& storage)
{
	struct AddHighlightOp : public MarkerOperator
	{
		AddHighlightOp(MetalBar* bar_, LineList& lines_, HighlightList& storage_) : bar(bar_), lines(lines_), storage(storage_) {}

		void NotifyCount(int numMarkers) const { storage.resize(numMarkers); }

		void Process(IVsTextLineMarker* marker, int idx) const
		{
			TextSpan span;
			marker->GetCurrentSpan(&span);
			assert(span.iStartLine == span.iEndLine);

			LineInfo& line = lines[span.iStartLine];
			Highlight* newh = &storage[idx];
			newh->start = span.iStartIndex;
			newh->end = span.iEndIndex;

			// Add a marker on the right to make it easier to see.
			if(!line.highlights)
				bar->MarkLineRange(lines, LineMarker_Match, span.iStartLine, span.iStartLine);

			// Preserve ordering. Since this is a linear search, it sounds like it would lead to quadratic behavior
			// (well, O(lines*matches_per_line), actually) but it doesn't. VS pushes newly created markers at the top
			// of a list; we create them in ascending order, by scanning the text, so they end up in reverse order in
			// the list. This means we get them in descending order here, so we add them to our list in constant time
			// in the following if block. Obviously, there's no guarantee to this, which is why we do the full scan if
			// needed, but it seems to hold true in all the scenarios I've tested.
			Highlight* h = line.highlights;
			if(!h || (h->start > newh->start))
			{
				newh->next = h;
				line.highlights = newh;
				return;
			}

			while(h->next && (newh->start > h->next->start))
				h = h->next;

			newh->next = h->next;
			h->next = newh;
		}

		MetalBar* bar;
		LineList& lines;
		HighlightList& storage;
	};

	ProcessLineMarkers(buffer, g_highlightMarkerType, AddHighlightOp(this, lines, storage));
}

#define ADVANCE_LINE()													\
	if(!(lines[currentTextLine].flags & LineMarker_Hidden))				\
	{																	\
		for(unsigned int i = linePos; i < s_barWidth; ++i)				\
			pixel[i] = s_whitespaceColor;								\
																		\
		PaintLineFlags(pixel, lines[currentTextLine].flags);			\
	}																	\
	++currentTextLine;													\
	currentTextColumn = 0

void MetalBar::RenderCodeImg(int barHeight)
{
	CComPtr<IVsTextLines> buffer;
	CComBSTR text;
	if(!GetBufferAndText(&buffer, &text, &m_numLines))
		return;

	LANGPREFERENCES langPrefs;
	if( SUCCEEDED(buffer->GetLanguageServiceID(&langPrefs.guidLang)) && SUCCEEDED(g_textMgr->GetUserPreferences(0, 0, &langPrefs, 0)) )
		m_tabSize = langPrefs.uTabSize;
	else
		m_tabSize = 4;

	if(m_numLines < 1)
		m_numLines = 1;

	LineInfo defaultLineInfo = { 0 };
	LineList lines(m_numLines, defaultLineInfo);
	GetLineFlags(lines, buffer);
	HighlightList highlightStorage;
	GetHighlights(lines, buffer, highlightStorage);

	unsigned int linePos = 0;
	unsigned int* imgBuffer = new unsigned int[m_numLines*s_barWidth];
	// Windows bitmaps are upside down for some retarded reason.
	unsigned int* pixel = imgBuffer + (m_numLines - 1)*s_barWidth;

	enum CommentType
	{
		CommentType_None,
		CommentType_SingleLine,
		CommentType_MultiLine
	} commentType = CommentType_None;
	
	int realNumLines = 0;
	int currentTextLine = 0;
	unsigned int currentTextColumn = 0;
	Highlight* crHighlight = lines[0].highlights;
	for(wchar_t* chr = text; *chr; ++chr)
	{
		// Check for newline.
		if( (chr[0] == L'\r') || (chr[0] == L'\n') )
		{
			// In case of CRLF, eat the next character.
			if( (chr[0] == L'\r') && (chr[1] == L'\n') )
				++chr;

			ADVANCE_LINE();

			crHighlight = lines[currentTextLine].highlights;

			if(!(lines[currentTextLine].flags & LineMarker_Hidden))
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

			// Advance the highlight interval, if needed.
			while(crHighlight && (currentTextColumn > crHighlight->end))
				crHighlight = crHighlight->next;

			// Override the color with the match color if inside a marker.
			if(crHighlight && (currentTextColumn >= crHighlight->start))
				color = s_matchColor;

			// Advance the current column by the correct number of characters.
			currentTextColumn += numChars;
		}
		else
		{
			color = s_whitespaceColor;
			if(*chr == L'\t')
				numChars = (int)m_tabSize;

			// Tabs count as a single character for the column tracking.
			++currentTextColumn;
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

	ADVANCE_LINE();
	assert(currentTextLine == m_numLines);

	if(m_codeImg)
		DeleteObject(m_codeImg);

	// The image may be shorter than we anticipated due to hidden sections. Since bitmaps are upside down,
	// we must skip the first part of the buffer, which represents the last (unused) lines.
	realNumLines += 1;
	unsigned int* realImgStart = imgBuffer + (m_numLines - realNumLines)*s_barWidth;
	m_numLines = realNumLines;
	m_codeImgHeight = m_numLines < barHeight ? m_numLines : barHeight;

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

	if(m_numLines < barHeight)
	{
		// Copy the buffer to the bitmap directly.
		memcpy(bmpBits, realImgStart, m_numLines*s_barWidth*4);
	}
	else
	{
		// Scale.
		ScaleImageVertically(bmpBits, barHeight, realImgStart, m_numLines, s_barWidth);
	}

	delete[] imgBuffer;

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

	if(!m_codeImg || m_codeImgDirty)
	{
		m_codeImgDirty = false;
		RenderCodeImg(barHeight);
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
}

static bool IsSeparator(wchar_t chr)
{
	if( (chr >= L'0') && (chr <= L'9') )
		return false;

	if( (chr >= L'A') && (chr <= L'Z') )
		return false;

	if( (chr >= L'a') && (chr <= L'z') )
		return false;

	if(chr == L'_')
		return false;

	return true;
}

void MetalBar::HighlightMatchingWords()
{
	CComPtr<IVsTextLines> buffer;
	CComBSTR allText;
	long numLines;
	if(!GetBufferAndText(&buffer, &allText, &numLines))
		return;

	RemoveWordHighlight(buffer);

	CComBSTR selText;
	HRESULT hr = m_view->GetSelectedText(&selText);
	if(FAILED(hr) || !selText)
		return;

	unsigned int selTextLen = selText.Length();
	if(selTextLen < 1)
		return;

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

		if( (wcsncmp(chr, selText, selTextLen) == 0) && (chr == allText || IsSeparator(chr[-1])) && IsSeparator(chr[selTextLen]) )
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
