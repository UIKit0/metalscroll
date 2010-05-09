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
#include "Connect.h"
#include "MetalBar.h"
#include "MarkerGUID.h"
#include "Utils.h"

extern CAddInModule _AtlModule;

CComPtr<EnvDTE80::DTE2>				g_dte;
CComPtr<EnvDTE::AddIn>				g_addInInstance;
CComPtr<IVsTextManager>				g_textMgr;
long								g_highlightMarkerType;
HWND								g_mainVSHwnd = 0;

CConnect::CConnect()
{
	m_textMgrEventsCookie = 0;
}

bool CConnect::GetTextManagerEventsPlug(IConnectionPoint** connPt)
{
	CComQIPtr<IConnectionPointContainer> connPoints = g_textMgr;
	if(!connPoints)
		return false;

	HRESULT hr = connPoints->FindConnectionPoint(__uuidof(IVsTextManagerEvents), connPt);
	return SUCCEEDED(hr) && *connPt;
}

bool CConnect::GetTextViewEventsPlug(IConnectionPoint** connPt, IVsTextView* view)
{
	CComQIPtr<IConnectionPointContainer> connPoints = view;
	if(!connPoints)
		return false;

	HRESULT hr = connPoints->FindConnectionPoint(__uuidof(IVsTextViewEvents), connPt);
	return SUCCEEDED(hr) && *connPt;
}

STDMETHODIMP CConnect::OnConnection(IDispatch* application, ext_ConnectMode /*connectMode*/, IDispatch* addInInst, SAFEARRAY** /*custom*/)
{
	Log("MetalScroll: OnConnection()\n");

	HRESULT hr = application->QueryInterface(__uuidof(EnvDTE80::DTE2), (void**)&g_dte);
	if(FAILED(hr))
		return hr;

	hr = addInInst->QueryInterface(__uuidof(EnvDTE::AddIn), (void**)&g_addInInstance);
	if(FAILED(hr))
		return hr;

	CComQIPtr<IServiceProvider> sp = g_dte;
	if(!sp)
		return E_NOINTERFACE;

	hr = sp->QueryService(SID_SVsTextManager, IID_IVsTextManager, (void**)&g_textMgr);
	if(FAILED(hr) || !g_textMgr)
		return E_NOINTERFACE;

	CComPtr<IConnectionPoint> connPt;
	if(!GetTextManagerEventsPlug(&connPt))
		return E_NOINTERFACE;

	hr = connPt->Advise((IVsTextManagerEvents*)this, &m_textMgrEventsCookie);
	if(FAILED(hr))
		return hr;

	hr = g_textMgr->GetRegisteredMarkerTypeID(&g_markerTypeGUID, &g_highlightMarkerType);
	if(FAILED(hr))
	{
		Log("MetalScroll: No highlight marker.\n");
		g_highlightMarkerType = 0;
	}

	CComPtr<EnvDTE::Window> mainWnd;
	hr = g_dte->get_MainWindow(&mainWnd);
	if(FAILED(hr) || !mainWnd)
	{
		Log("MetalScroll: Can't get main VS window.\n");
		return E_NOINTERFACE;
	}

	long mainWndHandle;
	hr = mainWnd->get_HWnd(&mainWndHandle);
	if(FAILED(hr) || !mainWndHandle)
	{
		Log("MetalScroll: Can't get main VS window handle.\n");
		return E_NOINTERFACE;
	}

	g_mainVSHwnd = (HWND)mainWndHandle;

	MetalBar::Init();

	Log("MetalScroll: OnConnection() done.\n");
	return S_OK;
}

STDMETHODIMP CConnect::OnDisconnection(ext_DisconnectMode /*removeMode*/, SAFEARRAY** /*custom*/)
{
	CComPtr<IConnectionPoint> textMgrEventsPlug;
	if(m_textMgrEventsCookie && GetTextManagerEventsPlug(&textMgrEventsPlug))
	{
		textMgrEventsPlug->Unadvise(m_textMgrEventsCookie);
		m_textMgrEventsCookie = 0;
	}

	MetalBar::Uninit();

	// Give up the global pointers.
	g_textMgr = 0;
	g_addInInstance = 0;
	g_dte = 0;
	return S_OK;
}

void STDMETHODCALLTYPE CConnect::OnRegisterView(IVsTextView* view)
{
	Log("MetalScroll: New view registered: 0x%p.\n", view);

	// Unfortunately, the window hasn't been created at this point yet, so we can't get the HWND
	// here. Register an even handler to catch SetFocus(), and get the HWND from there. We'll remove
	// the handler after the first SetFocus() as we don't care about getting more events once we
	// have the HWND.
	CComPtr<IConnectionPoint> connPt;
	if(!GetTextViewEventsPlug(&connPt, view))
		return;
	DWORD cookie;
	HRESULT hr = connPt->Advise((IVsTextViewEvents*)this, &cookie);
	if(FAILED(hr))
		return;

	Log("MetalScroll: Subscribed to view events.\n");
}

static BOOL CALLBACK FindScrollbars(HWND hwnd, LPARAM param)
{
	MetalBar::ScrollbarHandles* handles = (MetalBar::ScrollbarHandles*)param;

	char className[64];
	GetClassNameA(hwnd, className, _countof(className));
	if(strcmp(className, "ScrollBar") == 0)
	{
		LONG styles = GetWindowLong(hwnd, GWL_STYLE);
		if(styles & SBS_VERT)
			handles->vert = hwnd;
		else
			handles->horiz = hwnd;

		return TRUE;
	}

	// ReSharper adds a thing over the VS scrollbar.
	GetWindowTextA(hwnd, className, _countof(className));
	if(strcmp(className, "Error Stripe") == 0)
	{
		handles->resharper = hwnd;
		return TRUE;
	}

	return TRUE;
}

void CConnect::HookScrollbar(IVsTextView* view)
{
	MetalBar::ScrollbarHandles handles = { 0 };

	handles.editor = view->GetWindowHandle();
	if(!handles.editor)
		return;

	EnumChildWindows(GetParent(handles.editor), FindScrollbars, (LPARAM)&handles);
	if(!handles.vert)
	{
		Log("MetalScroll: Vertical scrollbar not found.\n");
		return;
	}

	if(GetWindowLongPtr(handles.vert, GWL_USERDATA) != 0)
	{
		Log("MetalScroll: Scrollbar has non-null user data.\n");
		return;
	}

	MetalBar* bar = new MetalBar(handles, view);
	Log("MetalScroll: Hooked view 0x%p. Scrollbars: %x, %x. Editor: %x. ReSharper: %x. MetalBar: 0x%p.\n", view, handles.vert, handles.horiz, handles.editor, handles.resharper, bar);
}

void STDMETHODCALLTYPE CConnect::OnSetFocus(IVsTextView* view)
{
	Log("MetalScroll: OnSetFocus(0x%p).\n", view);

	HookScrollbar(view);

	// Remove ourselves from the event list. Since we can't store the cookie we got
	// when we registered the event handler, we'll have to scan the event list looking
	// for our pointer.
	CComPtr<IConnectionPoint> connPt;
	if(!GetTextViewEventsPlug(&connPt, view))
		return;

	CComPtr<IEnumConnections> enumerator;
	HRESULT hr = connPt->EnumConnections(&enumerator);
	if(FAILED(hr) || !enumerator)
		return;

	CONNECTDATA connData;
	ULONG numRet;
	bool found = false;
	while( SUCCEEDED(enumerator->Next(1, &connData, &numRet)) && (numRet > 0) )
	{
		if(connData.pUnk == (IVsTextViewEvents*)this)
		{
			connPt->Unadvise(connData.dwCookie);
			found = true;
		}
		connData.pUnk->Release();
	}
}
