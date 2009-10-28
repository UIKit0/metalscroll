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

class MetalBar;

class ATL_NO_VTABLE CTextEventHandler :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CTextEventHandler, &CLSID_TextEventHandler>,
	public IDispatchImpl<ITextEventHandler, &IID_ITextEventHandler, &LIBID_MetalScrollLib>,
	public IDispatchImpl<IVsTextLinesEvents, &IID_IVsTextLinesEvents, &LIBID_TextManagerInternal>
{
public:
	CTextEventHandler();
	~CTextEventHandler();

	DECLARE_REGISTRY_RESOURCEID(IDR_TEXTEVENTHANDLER)

	DECLARE_NOT_AGGREGATABLE(CTextEventHandler)

	BEGIN_COM_MAP(CTextEventHandler)
		COM_INTERFACE_ENTRY(ITextEventHandler)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY(IVsTextLinesEvents)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

	void STDMETHODCALLTYPE OnChangeLineText(const TextLineChange* textLineChange, BOOL last);
	void STDMETHODCALLTYPE OnChangeLineAttributes(long /*firstLine*/, long /*lastLine*/) {}

	static CTextEventHandler*		CreateHandler(MetalBar* bar);
	void							RemoveHandler();

private:
	MetalBar*		m_bar;
	DWORD			m_cookie;

	static bool		GetConnectionPoint(const MetalBar* bar, IConnectionPoint** connPt);
};

OBJECT_ENTRY_AUTO(__uuidof(TextEventHandler), CTextEventHandler)
