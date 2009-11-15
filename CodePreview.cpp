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
#include "CodePreview.h"
#include "Utils.h"

const char CodePreview::s_className[] = "MetalScrollCodePreview";

CodePreview::CodePreview()
{
	m_hwnd = 0;
	m_paintDC = 0;
	m_codeBmp = 0;
	m_width = 0;
	m_height = 0;
}

void CodePreview::Destroy()
{
	if(m_hwnd)
	{
		DestroyWindow(m_hwnd);
		m_hwnd = 0;
	}

	if(m_codeBmp)
	{
		DeleteObject(m_codeBmp);
		m_codeBmp = 0;
	}

	if(m_paintDC)
	{
		DeleteDC(m_paintDC);
		m_paintDC = 0;
	}
}

LRESULT FAR PASCAL CodePreview::WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	if(message == WM_CREATE)
	{
		CREATESTRUCT* cs = (CREATESTRUCT*)lparam;
		SetWindowLongPtr(hwnd, GWL_USERDATA, (::LONG_PTR)cs->lpCreateParams);
		return 0;
	}

	CodePreview* inst = (CodePreview*)GetWindowLongPtr(hwnd, GWL_USERDATA);
	if(!inst)
		return DefWindowProc(hwnd, message, wparam, lparam);

	switch(message)
	{
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			if(hdc)
			{
				inst->OnPaint(hdc);
				EndPaint(hwnd, &ps);
			}
			return 0;
		}

		case WM_ERASEBKGND:
			return 1;
	}

	return DefWindowProc(hwnd, message, wparam, lparam);
}

void CodePreview::Register()
{
	WNDCLASSA wndClass;
	memset(&wndClass, 0, sizeof(wndClass));
	wndClass.lpfnWndProc = &WndProc;
	wndClass.hInstance = _AtlModule.GetResourceInstance();
	wndClass.lpszClassName = s_className;
	RegisterClassA(&wndClass);
}

void CodePreview::Unregister()
{
	UnregisterClassA(s_className, _AtlModule.GetResourceInstance());
}

void CodePreview::Create(HWND parent, int width, int height)
{
	m_width = width;
	m_height = height;
	DWORD style = WS_POPUP;
	m_hwnd = CreateWindowA(s_className, "CodePreview", style, 0, 0, width, height, parent, 0, _AtlModule.GetResourceInstance(), this);
}

void CodePreview::Show(HWND bar)
{
	RECT r;
	GetClientRect(bar, &r);
	
	POINT p;
	p.x = r.left;
	p.y = r.top;
	ClientToScreen(bar, &p);
	m_rightEdge = p.x;
	m_parentYMin = p.y;

	p.x = r.left;
	p.y = r.bottom;
	ClientToScreen(bar, &p);
	m_parentYMax = p.y;

	ShowWindow(m_hwnd, SW_SHOW);
}

void CodePreview::Hide()
{
	ShowWindow(m_hwnd, SW_HIDE);
}

void CodePreview::Update(int y, const wchar_t* /*text*/)
{
	// Paint the code image.
	if(!m_paintDC)
	{
		HDC wndDC = GetDC(m_hwnd);
		m_paintDC = CreateCompatibleDC(wndDC);
		m_codeBmp = CreateCompatibleBitmap(wndDC, m_width, m_height);
		SelectObject(m_paintDC, m_codeBmp);
		ReleaseDC(m_hwnd, wndDC);
	}

	RECT r;
	GetClientRect(m_hwnd, &r);
	FillSolidRect(m_paintDC, 0xff00ff, r);

	// Update the window position.
	y -= m_height / 2;
	y = clamp(y, m_parentYMin, m_parentYMax - m_height);

	int x = m_rightEdge - m_width - 10;
	SetWindowPos(m_hwnd, 0, x, y, 0, 0, SWP_NOSIZE|SWP_NOZORDER);
}

void CodePreview::OnPaint(HDC dc)
{
	if(!m_paintDC)
		return;

	BitBlt(dc, 0, 0, m_width, m_height, m_paintDC, 0, 0, SRCCOPY);
}

void CodePreview::Resize(int width, int height)
{
	m_width = width;
	m_height = height;
	SetWindowPos(m_hwnd, 0, 0, 0, width, height, SWP_NOMOVE|SWP_NOZORDER);
	if(m_paintDC)
	{
		DeleteObject(m_codeBmp);
		m_codeBmp = 0;
		DeleteDC(m_paintDC);
		m_paintDC = 0;
	}
}
