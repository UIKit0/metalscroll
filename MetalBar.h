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

class MetalBar
{
public:
	MetalBar(HWND vertBar, HWND editor, HWND horizBar, WNDPROC oldProc, TextDocument* doc);
	~MetalBar();

	LRESULT WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
	// User-controllable parameters.
	static unsigned int		s_barWidth;
	static unsigned int		s_whitespaceColor;
	static unsigned int		s_upperCaseColor;
	static unsigned int		s_characterColor;
	static unsigned int		s_commentColor;
	static unsigned char	s_pageColor[3];
	static unsigned char	s_pageOpacity;
	static unsigned int		s_matchColor;
	static unsigned int		s_addedLineColor;
	static unsigned int		s_unsavedLineColor;

	// Handles and other window-related stuff.
	WNDPROC					m_oldProc;
	HWND					m_hwnd;
	HWND					m_editorWnd;
	HWND					m_horizBar;

	// Text.
	TextDocument*			m_doc;
	long					m_numLines;

	// Painting.
	HBITMAP					m_codeImg;
	HDC						m_imgDC;
	HBITMAP					m_backBufferImg;
	HDC						m_backBufferDC;
	unsigned int*			m_backBufferBits;
	int						m_backBufferHeight;

	// Scrollbar stuff.
	int						m_pageSize;
	int						m_scrollPos;
	int						m_scrollMin;
	int						m_scrollMax;
	bool					m_dragging;

	void					OnDrag(bool initial);
	void					OnPaint(HDC ctrlDC);
	void					AdjustSize();
	void					RenderCodeImg();
};
