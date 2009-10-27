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

using namespace AddInDesignerObjects;
using namespace EnvDTE;
using namespace EnvDTE80;

class ATL_NO_VTABLE CConnect : 
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CConnect, &CLSID_Connect>,
	public IDispatchImpl<_IDTExtensibility2, &IID__IDTExtensibility2, &LIBID_AddInDesignerObjects, 1, 0>,
	public IDispEventImpl<1, CConnect, &__uuidof(_dispWindowEvents), &LIBID_EnvDTE, 8, 0>,
	public IDispEventImpl<1, CConnect, &__uuidof(_dispTextEditorEvents), &LIBID_EnvDTE, 8, 0>
{
public:
	CConnect()
	{
	}

DECLARE_REGISTRY_RESOURCEID(IDR_ADDIN)
DECLARE_NOT_AGGREGATABLE(CConnect)

BEGIN_COM_MAP(CConnect)
	COM_INTERFACE_ENTRY(IDispatch)
	COM_INTERFACE_ENTRY(IDTExtensibility2)
END_COM_MAP()

BEGIN_SINK_MAP(CConnect)
	SINK_ENTRY_EX(1, __uuidof(_dispWindowEvents), 3, OnWindowActivated)
	SINK_ENTRY_EX(1, __uuidof(_dispWindowEvents), 4, OnWindowCreated)
	SINK_ENTRY_EX(1, __uuidof(_dispTextEditorEvents), 1, OnLineChanged)
END_SINK_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}
	
	void FinalRelease() 
	{
	}

	STDMETHOD(OnConnection)(IDispatch* application, ext_ConnectMode connectMode, IDispatch* addInInst, SAFEARRAY** custom);
	STDMETHOD(OnDisconnection)(ext_DisconnectMode removeMode, SAFEARRAY** custom);
	STDMETHOD(OnAddInsUpdate)(SAFEARRAY** custom);
	STDMETHOD(OnStartupComplete)(SAFEARRAY** custom);
	STDMETHOD(OnBeginShutdown)(SAFEARRAY** custom);

	void __stdcall				OnWindowActivated(Window* gotFocus, Window* lostFocus);
	void __stdcall				OnWindowCreated(Window* window);
	void __stdcall				OnLineChanged(TextPoint* startPoint, TextPoint* endPoint, long hint);

private:
	CComPtr<DTE2>				m_dte;
	CComPtr<AddIn>				m_addInInstance;

	CComPtr<_WindowEvents>		m_wndEvents;
	CComPtr<_TextEditorEvents>	m_textEditorEvents;

	void						CreateOrUpdateScrollBars(Window* window, TextDocument* textDoc, TextPoint* changeStartPoint, TextPoint* changeEndPoint);
	void						CheckWindow(Window* window);
};

OBJECT_ENTRY_AUTO(__uuidof(Connect), CConnect)
