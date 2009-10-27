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

class ColorChip
{
public:
	static void						Register();
	static void						Unregister();

	ColorChip();

	void							Init(HWND hwnd, unsigned int color);
	unsigned int					GetColor() const { return m_color; }

private:
	static const char				s_className[];
	static LRESULT FAR PASCAL		WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

	HWND							m_tooltipWnd;
	unsigned int					m_color;

	void							OnClick(HWND hwnd);
	void							OnPaint(HWND hwnd, HDC dc);
	void							GetTooltipText(char* text, size_t maxLen);
};
