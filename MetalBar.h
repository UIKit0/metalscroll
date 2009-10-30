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

using namespace EnvDTE;

class Intervals;

class MetalBar
{
public:
	MetalBar(HWND vertBar, HWND editor, HWND horizBar, WNDPROC oldProc, IVsTextView* view);
	~MetalBar();

	IVsTextView*				GetView() const { return m_view; }
	LRESULT						WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

	static void					ResetSettings();
	static void					ReadSettings();
	static void					SaveSettings();

	static void					RemoveAllBars();

	// User-controllable parameters.
	static unsigned int			s_barWidth;
	static unsigned int			s_whitespaceColor;
	static unsigned int			s_upperCaseColor;
	static unsigned int			s_characterColor;
	static unsigned int			s_commentColor;
	static unsigned int			s_cursorColor;
	static unsigned int			s_matchColor;
	static unsigned int			s_modifiedLineColor;
	static unsigned int			s_unsavedLineColor;
	static unsigned int			s_breakpointColor;
	static unsigned int			s_bookmarkColor;

private:
	enum LineMarker
	{
		LineMarker_ChangedUnsaved = 0x01,
		LineMarker_ChangedSaved = 0x02,
		LineMarker_Breakpoint = 0x04,
		LineMarker_Bookmark = 0x08
	};

	static std::set<MetalBar*>	s_bars;
	static bool					ReadRegInt(unsigned int* to, HKEY key, const char* name);
	static void					WriteRegInt(HKEY key, const char* name, unsigned int val);

	// Handles and other window-related stuff.
	WNDPROC						m_oldProc;
	HWND						m_hwnd;
	HWND						m_editorWnd;
	HWND						m_horizBar;

	// Text.
	IVsTextView*				m_view;
	long						m_numLines;

	// Painting.
	bool						m_codeImgDirty;
	HBITMAP						m_codeImg;
	HDC							m_imgDC;
	HBITMAP						m_backBufferImg;
	HDC							m_backBufferDC;
	unsigned int*				m_backBufferBits;
	unsigned int				m_backBufferWidth;
	unsigned int				m_backBufferHeight;

	// Scrollbar stuff.
	int							m_pageSize;
	int							m_scrollPos;
	int							m_scrollMin;
	int							m_scrollMax;
	bool						m_dragging;

	void						OnDrag(bool initial);
	void						OnPaint(HDC ctrlDC);
	void						AdjustSize(unsigned int requiredWidth);
	void						GetHiddenLines(IVsTextLines* buffer, Intervals& hiddenRgn);
	void						FindMarkers(std::vector<unsigned char>& markers, IVsTextLines* buffer, int type, unsigned char flag);
	void						GetMarkers(std::vector<unsigned char>& markers, IVsTextLines* buffer, int numLines);
	void						PaintMarkers(unsigned int* line, unsigned char flags);
	void						RenderCodeImg();
	void						RemoveWndProcHook();
};
