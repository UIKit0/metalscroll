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
#include "ColorChip.h"

extern CAddInModule _AtlModule;

CComPtr<EnvDTE80::DTE2>				g_dte;
CComPtr<EnvDTE::AddIn>				g_addInInstance;
CComPtr<IVsTextManager>				g_textMgr;

// {454DB430-837D-4435-B7C1-F503B93EDA04} is the GUID for the marker service we register in MetalScrollPackage.rgs.
static const GUID g_markerServGUID = { 0x454DB430, 0x837D, 0x4435, { 0xB7, 0xC1, 0xF5, 0x03, 0xB9, 0x3E, 0xDA, 0x04 } };
// {FF447164-72F9-42ed-A767-C737B86BBD0B} is the GUID for the marker type.
static const GUID g_markerTypeGUID = { 0xFF447164, 0x72F9, 0x42ed, { 0xA7, 0x67, 0xC7, 0x37, 0xB8, 0x6B, 0xBD, 0x0B } };

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

	CComPtr<IProfferService> profferServ;
	hr = sp->QueryService(SID_SProfferService, IID_IProfferService, (void**)&profferServ);
	if(FAILED(hr) || !profferServ)
		return E_NOINTERFACE;

	DWORD cookie;
	hr = profferServ->ProfferService(g_markerServGUID, (IServiceProvider*)this, &cookie);
	if(FAILED(hr))
		return hr;

	MetalBar::ReadSettings();
	ColorChip::Register();

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

	MetalBar::RemoveAllBars();
	ColorChip::Unregister();

	// Give up the global pointers.
	g_textMgr = 0;
	g_addInInstance = 0;
	g_dte = 0;
	return S_OK;
}

void STDMETHODCALLTYPE CConnect::OnRegisterView(IVsTextView* view)
{
	// Unfortunately, the window hasn't been created at this point yet, so we can't get the HWND
	// here. Register an even handler to catch SetFocus(), and get the HWND from there. We'll remove
	// the handler after the first SetFocus() as we don't care about getting more events once we
	// have the HWND.
	CComPtr<IConnectionPoint> connPt;
	if(!GetTextViewEventsPlug(&connPt, view))
		return;
	DWORD cookie;
	connPt->Advise((IVsTextViewEvents*)this, &cookie);
}

struct ScrollbarHandles
{
	HWND		vert;
	HWND		horiz;
};

static BOOL CALLBACK FindScrollbars(HWND hwnd, LPARAM param)
{
	ScrollbarHandles* handles = (ScrollbarHandles*)param;

	char className[64];
	GetClassNameA(hwnd, className, _countof(className));
	if(strcmp(className, "ScrollBar") == 0)
	{
		LONG styles = GetWindowLong(hwnd, GWL_STYLE);
		if(styles & SBS_VERT)
			handles->vert = hwnd;
		else
			handles->horiz = hwnd;
	}

	return TRUE;
}

static LRESULT FAR PASCAL ScrollBarProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	MetalBar* bar = (MetalBar*)GetWindowLongPtr(hwnd, GWL_USERDATA);
	if(!bar)
		return 0;

	return bar->WndProc(hwnd, message, wparam, lparam);
}

void CConnect::HookScrollbar(IVsTextView* view)
{
	HWND editorHwnd = view->GetWindowHandle();
	if(!editorHwnd)
		return;

	ScrollbarHandles bars = { 0 };
	EnumChildWindows(GetParent(editorHwnd), FindScrollbars, (LPARAM)&bars);
	if(!bars.horiz || !bars.vert)
		return;

	if(GetWindowLongPtr(bars.vert, GWL_USERDATA) != 0)
		return;

	WNDPROC oldProc = (WNDPROC)SetWindowLongPtr(bars.vert, GWLP_WNDPROC, (::LONG_PTR)ScrollBarProc);
	MetalBar* bar = new MetalBar(bars.vert, editorHwnd, bars.horiz, oldProc, view);
	SetWindowLongPtr(bars.vert, GWL_USERDATA, (::LONG_PTR)bar);
}

void STDMETHODCALLTYPE CConnect::OnSetFocus(IVsTextView* view)
{
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

STDMETHODIMP CConnect::QueryService(REFGUID guidService, REFIID riid, void** object)
{
	if(!object)
		return E_POINTER;

	if(!InlineIsEqualGUID(guidService, g_markerServGUID))
		return E_NOINTERFACE;

	if(!IsEqualIID(riid, __uuidof(IVsTextMarkerTypeProvider)))
		return E_NOINTERFACE;

	*object = (IVsTextMarkerTypeProvider*)this;
	((IVsTextMarkerTypeProvider*)this)->AddRef();
	return S_OK;
}

STDMETHODIMP CConnect::GetTextMarkerType(GUID* guidMarker, IVsPackageDefinedTextMarkerType** markerType)
{
	if(!markerType)
		return E_POINTER;

	if(!InlineIsEqualGUID(*guidMarker, g_markerTypeGUID))
		return E_NOINTERFACE;

	*markerType = (IVsPackageDefinedTextMarkerType*)this;
	(*markerType)->AddRef();
	return S_OK;
}

STDMETHODIMP CConnect::GetVisualStyle(DWORD* visualFlags)
{
	if(!visualFlags)
		return E_POINTER;

	*visualFlags = MV_COLOR_ALWAYS;
	return S_OK;
}

STDMETHODIMP CConnect::GetDefaultColors(COLORINDEX* foreground, COLORINDEX* background)
{
	if(!foreground || !background)
		return E_POINTER;

	*foreground = CI_BLACK;
	*background = CI_CYAN;
	return S_OK;
}

STDMETHODIMP CConnect::GetDefaultLineStyle(COLORINDEX* lineColor, LINESTYLE* lineIndex)
{
	// This shouldn't be called because we haven't requested MV_LINE or MV_BORDER.
	if(!lineColor || !lineIndex)
		return E_POINTER;

	*lineColor = CI_BLACK;
	*lineIndex = LI_SOLID;
	return S_OK;
}

STDMETHODIMP CConnect::GetDefaultFontFlags(DWORD* fontFlags)
{
	if(!fontFlags)
		return E_POINTER;

	*fontFlags = FF_DEFAULT;
	return S_OK;
}

STDMETHODIMP CConnect::DrawGlyphWithColors(HDC /*hdc*/, RECT* /*rect*/, long /*markerType*/, IVsTextMarkerColorSet* /*markerColors*/, DWORD /*glyphDrawFlags*/, long /*lineHeight*/)
{
	// This shouldn't be called because we haven't requested MV_GLYPH.
	return S_OK;
}

STDMETHODIMP CConnect::GetBehaviorFlags(DWORD* flags)
{
	if(!flags)
		return E_POINTER;

	*flags = MB_DEFAULT;
	return S_OK;
}

STDMETHODIMP CConnect::GetPriorityIndex(long* priorityIndex)
{
	if(!priorityIndex)
		return E_POINTER;

	*priorityIndex = 100;
	return S_OK;
}
