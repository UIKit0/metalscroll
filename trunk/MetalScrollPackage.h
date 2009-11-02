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

class ATL_NO_VTABLE CMetalScrollPackage :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CMetalScrollPackage, &CLSID_MetalScrollPackage>,
	public IVsPackage,
	public IServiceProvider,
	public IVsTextMarkerTypeProvider,
	public IVsPackageDefinedTextMarkerType,
	public IVsMergeableUIItem
{
public:
	CMetalScrollPackage();

	DECLARE_REGISTRY_RESOURCEID(IDR_METALSCROLLPACKAGE)

	BEGIN_COM_MAP(CMetalScrollPackage)
		COM_INTERFACE_ENTRY(IVsPackage)
		COM_INTERFACE_ENTRY(IServiceProvider)
		COM_INTERFACE_ENTRY(IVsTextMarkerTypeProvider)
		COM_INTERFACE_ENTRY(IVsPackageDefinedTextMarkerType)
		COM_INTERFACE_ENTRY(IVsMergeableUIItem)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

	// IVsPackage implementation.
	STDMETHOD(SetSite)(IServiceProvider* serviceProvider);
	STDMETHOD(QueryClose)(BOOL* canClose) { if(canClose) { *canClose = TRUE; return S_OK; } else return E_POINTER; }
	STDMETHOD(Close)(void){ return S_OK; }
	STDMETHOD(GetAutomationObject)(LPCOLESTR /*propName*/, IDispatch** /*disp*/) { return E_NOTIMPL; }
	STDMETHOD(CreateTool)(REFGUID /*persistenceSlot*/){ return E_NOTIMPL; }
	STDMETHOD(ResetDefaults)(VSPKGRESETFLAGS /*flags*/){ return S_OK; }
	STDMETHOD(GetPropertyPage)(REFGUID /*page*/, VSPROPSHEETPAGE* /*page*/) { return E_NOTIMPL; }

	// IServiceProvider implementation.
	STDMETHOD(QueryService)(REFGUID guidService, REFIID riid, void** object);

	// IVsTextMarkerTypeProvider implementation.
	STDMETHOD(GetTextMarkerType)(GUID* guidMarker, IVsPackageDefinedTextMarkerType** markerType);

	// IVsPackageDefinedTextMarkerType implementation.
	STDMETHOD(GetVisualStyle)(DWORD* visualFlags);
	STDMETHOD(GetDefaultColors)(COLORINDEX* foreground, COLORINDEX* background);
	STDMETHOD(GetDefaultLineStyle)(COLORINDEX* lineColor, LINESTYLE* lineIndex);
	STDMETHOD(GetDefaultFontFlags)(DWORD* fontFlags);
	STDMETHOD(DrawGlyphWithColors)(HDC hdc, RECT* rect, long markerType, IVsTextMarkerColorSet* markerColors, DWORD glyphDrawFlags, long lineHeight);
	STDMETHOD(GetBehaviorFlags)(DWORD* flags);
	STDMETHOD(GetPriorityIndex)(long* priorityIndex);

	// IVsMergeableUIItem implementation.
	STDMETHOD(GetCanonicalName)(BSTR* nonLocalizeName);
	STDMETHOD(GetDisplayName)(BSTR* displayName);
	STDMETHOD(GetMergingPriority)(long* mergingPriority);
	STDMETHOD(GetDescription)(BSTR* desc);

};

OBJECT_ENTRY_AUTO(__uuidof(MetalScrollPackage), CMetalScrollPackage)
