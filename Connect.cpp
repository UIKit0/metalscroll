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
#include "AddIn.h"
#include "Connect.h"
#include "MetalBar.h"

extern CAddInModule _AtlModule;

STDMETHODIMP CConnect::OnConnection(IDispatch* application, ext_ConnectMode /*connectMode*/, IDispatch* addInInst, SAFEARRAY** /*custom*/)
{
	HRESULT hr = application->QueryInterface(__uuidof(DTE2), (void**)&m_dte);
	if(FAILED(hr))
		return hr;

	hr = addInInst->QueryInterface(__uuidof(AddIn), (void**)&m_addInInstance);
	if(FAILED(hr))
		return hr;

	CComPtr<Events> events;
	hr = m_dte->get_Events(&events);
	if(FAILED(hr) || !events)
		return hr;

	hr = events->get_WindowEvents(0, (_WindowEvents**)&m_wndEvents);
	if(FAILED(hr) || !m_wndEvents)
		return hr;

	hr = IDispEventSimpleImpl<1, CConnect, &__uuidof(_dispWindowEvents)>::DispEventAdvise(m_wndEvents);
	if(FAILED(hr))
		return hr;
	
	return S_OK;
}

STDMETHODIMP CConnect::OnDisconnection(ext_DisconnectMode /*removeMode*/, SAFEARRAY** /*custom*/)
{
	if(m_wndEvents)
	{
		IDispEventSimpleImpl<1, CConnect, &__uuidof(_dispWindowEvents)>::DispEventUnadvise(m_wndEvents);
		m_wndEvents = 0;
	}

	// FIXME: delete all the bars so the window procedures get unhooked.

	m_dte = 0;
	m_addInInstance = 0;
	return S_OK;
}

STDMETHODIMP CConnect::OnAddInsUpdate(SAFEARRAY** /*custom*/)
{
	return S_OK;
}

STDMETHODIMP CConnect::OnStartupComplete(SAFEARRAY** /*custom*/)
{
	return S_OK;
}

STDMETHODIMP CConnect::OnBeginShutdown(SAFEARRAY** /*custom*/)
{
	return S_OK;
}

void CConnect::OnWindowActivated(Window* gotFocus, Window* /*lostFocus*/)
{
	CheckWindow(gotFocus);
}

void CConnect::OnWindowCreated(Window* window)
{
	CheckWindow(window);
}

static BOOL CALLBACK FindSplitterRoot(HWND hwnd, LPARAM param)
{
	wchar_t className[64];
	GetClassNameW(hwnd, className, _countof(className));
	if(_wcsicmp(className, L"VsSplitterRoot") == 0)
	{
		*(HWND*)param = hwnd;
		return FALSE;
	}

	return TRUE;
}

struct EditorHandles
{
	HWND	editor;
	HWND	vertScroll;
	HWND	horizScroll;
};

static BOOL CALLBACK FindEditorAndScrollBar(HWND hwnd, LPARAM param)
{
	EditorHandles* handles = (EditorHandles*)param;

	wchar_t className[64];
	GetClassNameW(hwnd, className, _countof(className));

	if(_wcsicmp(className, L"ScrollBar") == 0)
	{
		LONG styles = GetWindowLong(hwnd, GWL_STYLE);
		if(styles & SBS_VERT)
			handles->vertScroll = hwnd;
		else
			handles->horizScroll = hwnd;
	}
	else if(_wcsicmp(className, L"VsTextEditPane") == 0)
		handles->editor = hwnd;

	return TRUE;
}

static LRESULT FAR PASCAL ScrollBarProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	MetalBar* bar = (MetalBar*)GetWindowLongPtr(hwnd, GWL_USERDATA);
	if(!bar)
		return 0;

	return bar->WndProc(hwnd, message, wparam, lparam);
}

static BOOL CALLBACK CheckEditorPanels(HWND hwnd, LPARAM param)
{
	wchar_t className[64];
	GetClassNameW(hwnd, className, _countof(className));
	if(_wcsicmp(className, L"VsEditPane") != 0)
		return TRUE;

	// Find the vertical scrollbar.
	EditorHandles handles = { 0 };
	EnumChildWindows(hwnd, FindEditorAndScrollBar, (LPARAM)&handles);
	if(!handles.editor || !handles.vertScroll || !handles.horizScroll)
		return TRUE;

	// Check if it's already subclassed.
	::LONG_PTR userData = GetWindowLongPtr(handles.vertScroll, GWL_USERDATA);
	if(userData)
		return TRUE;

	// Install our own window procedure.
	WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(handles.vertScroll, GWLP_WNDPROC, (::LONG_PTR)ScrollBarProc);
	MetalBar* newBar = new MetalBar(handles.vertScroll, handles.editor, handles.horizScroll, oldProc, (TextDocument*)param);
	SetWindowLongPtr(handles.vertScroll, GWL_USERDATA, (::LONG_PTR)newBar);

	// Continue walking the child list.
	return TRUE;
}

void CConnect::CheckWindow(Window* window)
{
	// See if it's a code window.
	CComBSTR kind;
	HRESULT hr = window->get_ObjectKind(&kind);
	if(FAILED(hr))
		return;

	if(kind != vsDocumentKindText)
		return;

	// The the TextDocument object from it.
	CComPtr<Document> doc;
	hr = window->get_Document(&doc);
	if(FAILED(hr) || !doc)
		return;

	CComPtr<IDispatch> disp;
	hr = doc->Object(L"TextDocument", &disp);
	if(FAILED(hr) || !disp)
		return;

	TextDocument* textDoc = 0;
	hr = disp->QueryInterface(__uuidof(TextDocument), (void**)&textDoc);
	if(FAILED(hr) || !textDoc)
		return;

	// The little shit won't give up its HWND the nice way (via get_HWnd()). Alt+8 to the rescue!
	// Luckily, get_HWnd() is a tiny function. Right at the start, it checks if an int at offset
	// 0x4c in the object is 0. If it's 0, it returns the handle, which is at offset 0x48 in the object.
	// For the code window, the int at 0x4c is 16, so it returns 0. However, the handle at 0x48 is still
	// valid even for the code window, so we'll just fetch it with a bit of pointer math.
	HWND paneHwnd = *(HWND*)((unsigned char*)window + 0x48);
	if(!paneHwnd)
		return;

	// The handle we've got is for the entire panel. The scrollbar will be a number of levels down in the
	// hierarchy, but first make sure the window is what we expect it to be.
	wchar_t className[64];
	GetClassNameW(paneHwnd, className, _countof(className));
	if(_wcsicmp(className, L"GenericPane") != 0)
		return;

	// The pane normally has a single child, but Visual Assist inserts its go button and the context and
	// definition dropdowns inside the pane when it's active. Therefore, we need to search for a window
	// with class "VsSplitterRoot" instead of just getting the first child.
	HWND splitterHwnd = 0;
	EnumChildWindows(paneHwnd, FindSplitterRoot, (LPARAM)&splitterHwnd);
	if(!splitterHwnd)
		return;

	// Now check all the children with class "VsEditPane".
	EnumChildWindows(splitterHwnd, CheckEditorPanels, (LPARAM)textDoc);
}
