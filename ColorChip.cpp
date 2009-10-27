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
#include "ColorChip.h"

const char ColorChip::s_className[] = "MetalScrollChip";

ColorChip::ColorChip()
{
	m_tooltipWnd = 0;
	m_color = 0;
}

LRESULT FAR PASCAL ColorChip::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	ColorChip* chip = (ColorChip*)GetWindowLongPtr(hwnd, GWL_USERDATA);
	if(!chip)
		return DefWindowProc(hwnd, message, wparam, lparam);

	switch(message)
	{
		case WM_LBUTTONDOWN:
			chip->OnClick(hwnd);
			return 0;

		case WM_MOUSEMOVE:
		{
			// MSDN says we should relay all the mouse messages to the tooltip, but if we send it clicks,
			// it stops appearing after we click the control. We could re-arm the tooltip by sending it
			// TTM_ADDTOOL from the click handler (I tried it and it works), but I don't know if that's how
			// it's supposed to be done, or if it causes some kind of leak. Anyway, if we only relay
			// WM_MOUSEMOVE from here, it never sees the clicks, so everything is fine.
			MSG msg;
			memset(&msg, 0, sizeof(msg));
			msg.hwnd = hwnd;
			msg.message = message;
			msg.wParam = wparam;
			msg.lParam = lparam;
			SendMessage(chip->m_tooltipWnd, TTM_RELAYEVENT, 0, (LPARAM)&msg);
			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			if(hdc)
			{
				chip->OnPaint(hwnd, hdc);
				EndPaint(hwnd, &ps);
			}
			return 0;
		}

		case WM_DESTROY:
		{
			if(chip->m_tooltipWnd)
			{
				DestroyWindow(chip->m_tooltipWnd);
				chip->m_tooltipWnd = 0;
			}
			return 0;
		}

		case WM_NOTIFY:
		{
			// Handle tooltip requests.
			NMHDR* hdr = (NMHDR*)lparam;
			if(hdr->code != TTN_GETDISPINFO)
				break;

			NMTTDISPINFO* toolTipInfo = (NMTTDISPINFO*)hdr;
			toolTipInfo->lpszText = toolTipInfo->szText;
			toolTipInfo->hinst = 0;
			chip->GetTooltipText(toolTipInfo->szText, _countof(toolTipInfo->szText));
			return 0;
		}
	}

	return DefWindowProc(hwnd, message, wparam, lparam);
}

void ColorChip::Register()
{
	WNDCLASS wndClass;
	memset(&wndClass, 0, sizeof(wndClass));
	wndClass.lpfnWndProc = &WndProc;
	wndClass.hInstance = _AtlModule.GetResourceInstance();
	wndClass.lpszClassName = s_className;
	RegisterClassA(&wndClass);
}

void ColorChip::Unregister()
{
	UnregisterClassA(s_className, _AtlModule.GetResourceInstance());
}

void ColorChip::Init(HWND hwnd, unsigned int color)
{
	// Make sure it's a color chip.
	char className[64];
	GetClassNameA(hwnd, className, sizeof(className));
	if(strcmp(className, s_className) != 0)
		return;

	// Set the object pointer and initialize the color.
	SetWindowLongPtr(hwnd, GWL_USERDATA, (::LONG_PTR)this);
	m_color = color;

	// Create the tooltip.
	DWORD style = WS_POPUP | TTS_ALWAYSTIP;
	m_tooltipWnd = CreateWindowEx(0, TOOLTIPS_CLASS, 0, style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hwnd, 0, _AtlModule.GetResourceInstance(), 0);

	TOOLINFO toolInfo;
	memset(&toolInfo, 0, sizeof(toolInfo));
	toolInfo.cbSize = sizeof(toolInfo);
	toolInfo.hwnd = hwnd;
	toolInfo.uFlags = TTF_IDISHWND;
	toolInfo.uId = (UINT_PTR)hwnd;
	toolInfo.lpszText = LPSTR_TEXTCALLBACK;
	SendMessage(m_tooltipWnd, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
}

void ColorChip::OnClick(HWND hwnd)
{
	static COLORREF custColors[16] = { 0 };

	CHOOSECOLOR data;
	memset(&data, 0, sizeof(data));
	data.lStructSize = sizeof(data);
	data.hwndOwner = hwnd;
	data.lpCustColors = custColors;
	data.rgbResult = RGB_TO_COLORREF(m_color);
	data.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;
	if(!ChooseColor(&data))
		return;

	m_color = COLORREF_TO_RGB(data.rgbResult);
	InvalidateRect(hwnd, 0, 0);
}

void ColorChip::OnPaint(HWND hwnd, HDC dc)
{
	RECT clRect;
	GetClientRect(hwnd, &clRect);

	HGDIOBJ prevPen = SelectObject(dc, GetStockObject(BLACK_PEN));
	Rectangle(dc, clRect.left, clRect.top, clRect.right, clRect.bottom);
	SelectObject(dc, prevPen);

	clRect.left += 1;
	clRect.right -= 1;
	clRect.top += 1;
	clRect.bottom -= 1;

	COLORREF prevColor = SetBkColor(dc, RGB_TO_COLORREF(m_color));
	ExtTextOut(dc, 0, 0, ETO_OPAQUE, &clRect, NULL, 0, NULL);
	SetBkColor(dc, prevColor);
}

void ColorChip::GetTooltipText(char* text, size_t maxLen)
{
	_snprintf(text, maxLen, "%d, %d, %d", (m_color >> 16) & 0xff, (m_color >> 8) & 0xff, m_color & 0xff);
	text[maxLen - 1] = 0;
}
