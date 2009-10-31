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

class ATL_NO_VTABLE CEditCmdFilter :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CEditCmdFilter, &CLSID_EditCmdFilter>,
	public IOleCommandTarget
{
public:
	CEditCmdFilter();

	DECLARE_REGISTRY_RESOURCEID(IDR_EDITCMDFILTER)

	DECLARE_NOT_AGGREGATABLE(CEditCmdFilter)

	BEGIN_COM_MAP(CEditCmdFilter)
		COM_INTERFACE_ENTRY(IOleCommandTarget)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

	static CEditCmdFilter*		AttachFilter(MetalBar* bar);
	void						RemoveFilter();

	// IOleCommandTarget implementation.
	STDMETHOD(QueryStatus)(const GUID* cmdGroup, ULONG numCmds, OLECMD cmds[], OLECMDTEXT* cmdText);
	STDMETHOD(Exec)(const GUID* cmdGroup, DWORD cmdID, DWORD cmdexecopt, VARIANT* in, VARIANT* out);

private:
	MetalBar*					m_bar;
	IOleCommandTarget*			m_nextHandler;
};

OBJECT_ENTRY_AUTO(__uuidof(EditCmdFilter), CEditCmdFilter)
