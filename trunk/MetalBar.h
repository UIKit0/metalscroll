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

class CEditCmdFilter;

class MetalBar
{
public:
	MetalBar(HWND vertBar, HWND editor, HWND horizBar, WNDPROC oldProc, IVsTextView* view);
	~MetalBar();

	IVsTextView*					GetView() const { return m_view; }
	HWND							GetHwnd() const { return m_hwnd; }
	LRESULT							WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

	static void						ResetSettings();
	static void						ReadSettings();
	static void						SaveSettings();

	static void						RemoveAllBars();

	// User-controllable parameters.
	static unsigned int				s_barWidth;
	static unsigned int				s_whitespaceColor;
	static unsigned int				s_upperCaseColor;
	static unsigned int				s_characterColor;
	static unsigned int				s_commentColor;
	static unsigned int				s_cursorColor;
	static unsigned int				s_matchColor;
	static unsigned int				s_modifiedLineColor;
	static unsigned int				s_unsavedLineColor;
	static unsigned int				s_breakpointColor;
	static unsigned int				s_bookmarkColor;
	static unsigned int				s_requireAltForHighlight;

private:
	enum LineMarker
	{
		LineMarker_Hidden			= 0x01,
		LineMarker_ChangedUnsaved	= 0x02,
		LineMarker_ChangedSaved		= 0x04,
		LineMarker_Breakpoint		= 0x08,
		LineMarker_Bookmark			= 0x10,
		LineMarker_Match			= 0x20
	};

	struct Highlight
	{
		unsigned int				start;
		unsigned int				end;
		Highlight*					next;
	};

	struct LineInfo
	{
		unsigned int				flags;
		Highlight*					highlights;
	};

	typedef std::vector<LineInfo>	LineList;
	typedef std::vector<Highlight>	HighlightList;

	struct MarkerOperator
	{
		virtual void NotifyCount(int /*numMarkers*/) const {}
		virtual void Process(IVsTextLineMarker* marker, int idx) const = 0;
	};

	static std::set<MetalBar*>		s_bars;
	static bool						ReadRegInt(unsigned int* to, HKEY key, const char* name);
	static void						WriteRegInt(HKEY key, const char* name, unsigned int val);

	// Handles and other window-related stuff.
	WNDPROC							m_oldProc;
	HWND							m_hwnd;
	HWND							m_editorWnd;
	HWND							m_horizBar;
	HWND							m_tooltipWnd;

	// Text.
	IVsTextView*					m_view;
	long							m_numLines;
	CEditCmdFilter*					m_editCmdFilter;

	// Painting.
	HBITMAP							m_codeImg;
	int								m_codeImgHeight;
	bool							m_codeImgDirty;
	HDC								m_imgDC;
	HBITMAP							m_backBufferImg;
	HDC								m_backBufferDC;
	unsigned int*					m_backBufferBits;
	unsigned int					m_backBufferWidth;
	unsigned int					m_backBufferHeight;

	// Scrollbar stuff.
	int								m_pageSize;
	int								m_scrollPos;
	int								m_scrollMin;
	int								m_scrollMax;
	bool							m_dragging;
	bool							m_tooltipShown;

	void							InitTooltip();
	bool							GetBufferAndText(IVsTextLines** buffer, BSTR* text, long* numLines);
	bool							GetFileName(CComBSTR& name, IVsTextLines* buffer);
	void							ProcessLineMarkers(IVsTextLines* buffer, int type, const MarkerOperator& op);

	void							OnDrag(bool initial);
	void							OnTrackTooltip();
	void							OnPaint(HDC ctrlDC);
	void							AdjustSize(unsigned int requiredWidth);
	void							RemoveWndProcHook();

	void							HighlightMatchingWords();
	void							RemoveWordHighlight(IVsTextLines* buffer);

	void							MarkLineRange(LineList& lines, unsigned int flag, int start, int end);
	void							FindHiddenLines(LineList& lines, IVsTextLines* buffer);
	void							FindBreakpoints(LineList& lines, IVsTextLines* buffer);
	void							GetLineFlags(LineList& lines, IVsTextLines* buffer);
	void							GetHighlights(LineList& lines, IVsTextLines* buffer, HighlightList& storage);
	void							PaintLineFlags(unsigned int* line, unsigned int flags);
	void							RenderCodeImg(int barHeight);
};
