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

#include "resource.h"
#include "AddIn.h"

using namespace AddInDesignerObjects;

class ATL_NO_VTABLE CConnect : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CConnect, &CLSID_Connect>,
	public IDispatchImpl<_IDTExtensibility2, &IID__IDTExtensibility2, &LIBID_AddInDesignerObjects, 1, 0>,
	public IDispatchImpl<EnvDTE::IDTCommandTarget, &EnvDTE::IID_IDTCommandTarget, &EnvDTE::LIBID_EnvDTE, 7, 0>,
	public IVsTextManagerEvents,
	public IVsTextViewEvents
{
public:
	CConnect();

	DECLARE_REGISTRY_RESOURCEID(IDR_ADDIN)
	DECLARE_NOT_AGGREGATABLE(CConnect)

	BEGIN_COM_MAP(CConnect)
		COM_INTERFACE_ENTRY(IDTExtensibility2)
		COM_INTERFACE_ENTRY(IDTCommandTarget)
		COM_INTERFACE_ENTRY2(IDispatch, IDTExtensibility2)
		COM_INTERFACE_ENTRY(IVsTextManagerEvents)
		COM_INTERFACE_ENTRY(IVsTextViewEvents)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease() 
	{
	}

	// _IDTExtensibility2 implementation.
	STDMETHOD(OnConnection)(IDispatch* application, ext_ConnectMode connectMode, IDispatch* addInInst, SAFEARRAY** custom);
	STDMETHOD(OnDisconnection)(ext_DisconnectMode removeMode, SAFEARRAY** custom);
	STDMETHOD(OnAddInsUpdate)(SAFEARRAY** /*custom*/) { return S_OK; }
	STDMETHOD(OnStartupComplete)(SAFEARRAY** /*custom*/) { return S_OK; }
	STDMETHOD(OnBeginShutdown)(SAFEARRAY** /*custom*/) { return S_OK; }

	// IDTCommandTarget implementation.
	STDMETHOD(QueryStatus)(BSTR CmdName, EnvDTE::vsCommandStatusTextWanted NeededText, EnvDTE::vsCommandStatus* StatusOption, VARIANT* CommandText);
	STDMETHOD(Exec)(BSTR CmdName, EnvDTE::vsCommandExecOption ExecuteOption, VARIANT* VariantIn, VARIANT* VariantOut, VARIANT_BOOL* Handled);

	// IVsTextManagerEvents implementation.
	void STDMETHODCALLTYPE		OnRegisterMarkerType(long /*markerType*/) {}
	void STDMETHODCALLTYPE		OnRegisterView(IVsTextView* view);
	void STDMETHODCALLTYPE		OnUnregisterView(IVsTextView* /*view*/) {}
	void STDMETHODCALLTYPE		OnUserPreferencesChanged(const VIEWPREFERENCES* /*viewPrefs*/, const FRAMEPREFERENCES* /*framePrefs*/, const LANGPREFERENCES* /*langPrefs*/, const FONTCOLORPREFERENCES* /*colorPrefs*/) {}

	// IVsTextViewEvents implementation.
	void STDMETHODCALLTYPE		OnSetFocus(IVsTextView* view);
	void STDMETHODCALLTYPE		OnKillFocus(IVsTextView* /*view*/) {}
	void STDMETHODCALLTYPE		OnSetBuffer(IVsTextView* /*view*/, IVsTextLines* /*buffer*/) {}
	void STDMETHODCALLTYPE		OnChangeScrollInfo(IVsTextView* /*view*/, long /*bar*/, long /*minUnit*/, long /*maxUnits*/, long /*visibleUnits*/, long /*firstVisibleUnit*/) {}
	void STDMETHODCALLTYPE		OnChangeCaretLine(IVsTextView* /*view*/, long /*newLine*/, long /*oldLine*/) {}

private:
	typedef void (CConnect::*CmdTriggerFunc)();
	typedef std::map<std::wstring, CmdTriggerFunc>	CmdHandlerMap;
	typedef CmdHandlerMap::iterator					CmdHandlerMapIt;

	DWORD						m_textMgrEventsCookie;
	CmdHandlerMap				m_commandHandlers;

	bool						GetTextManagerEventsPlug(IConnectionPoint** connPt);
	bool						GetTextViewEventsPlug(IConnectionPoint** connPt, IVsTextView* view);
	void						HookScrollbar(IVsTextView* view);
	void						RegisterCommand(EnvDTE80::Commands2* cmdInterface, const wchar_t* name, const wchar_t* caption, const wchar_t* descr, CmdTriggerFunc handler);
	void						HookAllScrollbars();
	void						OnToggle();
	bool						FindRockScroll();
};

OBJECT_ENTRY_AUTO(__uuidof(Connect), CConnect)
