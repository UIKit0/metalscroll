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

class CodePreview
{
public:
	CodePreview();

	static void					Register();
	static void					Unregister();

	void						Create(HWND parent, int width, int height);
	void						Destroy();

	void						Show(HWND bar);
	void						Hide();
	void						Update(int y, wchar_t* text, int tabSize, const wchar_t* highlightWord, bool doSyntaxHighlight);
	void						Resize(int width, int height);

private:
	static const char			s_className[];
	static HFONT				s_normalFont;
	static HFONT				s_boldFont;
	static int					s_charWidth;
	static int					s_lineHeight;

	HWND						m_hwnd;
	HDC							m_paintDC;
	HBITMAP						m_codeBmp;
	int							m_width;
	int							m_height;
	int							m_rightEdge;
	int							m_parentYMin;
	int							m_parentYMax;
	int							m_tabSize;

	static LRESULT FAR PASCAL	WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
	
	wchar_t*					EatWhitespace(wchar_t* text, int* x, int* y);
	wchar_t*					EatString(wchar_t* text, int* x);
	wchar_t*					EatComment(wchar_t* text, int* x);
	wchar_t*					EatIdentifier(wchar_t* text, int* x);
	void						OnPaint(HDC dc);

};
