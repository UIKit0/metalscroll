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
#include "MetalScrollPackage.h"
#include "MarkerGUID.h"

// {454DB430-837D-4435-B7C1-F503B93EDA04} is the GUID for the marker service we register in CMetalScrollPackage.rgs.
const GUID g_markerServGUID = { 0x454DB430, 0x837D, 0x4435, { 0xB7, 0xC1, 0xF5, 0x03, 0xB9, 0x3E, 0xDA, 0x04 } };
// {FF447164-72F9-42ed-A767-C737B86BBD0B} is the GUID for the marker type.
const GUID g_markerTypeGUID = { 0xFF447164, 0x72F9, 0x42ed, { 0xA7, 0x67, 0xC7, 0x37, 0xB8, 0x6B, 0xBD, 0x0B } };

STDMETHODIMP CMetalScrollPackage::SetSite(IServiceProvider* serviceProvider)
{
	if(!serviceProvider)
		return S_OK;

	CComPtr<IProfferService> profferServ;
	HRESULT hr = serviceProvider->QueryService(SID_SProfferService, IID_IProfferService, (void**)&profferServ);
	if(FAILED(hr) || !profferServ)
		return E_NOINTERFACE;

	DWORD cookie;
	hr = profferServ->ProfferService(g_markerServGUID, (IServiceProvider*)this, &cookie);
	if(FAILED(hr))
		return hr;

	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::QueryService(REFGUID guidService, REFIID riid, void** object)
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

STDMETHODIMP CMetalScrollPackage::GetTextMarkerType(GUID* guidMarker, IVsPackageDefinedTextMarkerType** markerType)
{
	if(!markerType)
		return E_POINTER;

	if(!InlineIsEqualGUID(*guidMarker, g_markerTypeGUID))
		return E_NOINTERFACE;

	*markerType = (IVsPackageDefinedTextMarkerType*)this;
	(*markerType)->AddRef();
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::GetVisualStyle(DWORD* visualFlags)
{
	if(!visualFlags)
		return E_POINTER;

	*visualFlags = MV_COLOR_ALWAYS;
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::GetDefaultColors(COLORINDEX* foreground, COLORINDEX* background)
{
	if(!foreground || !background)
		return E_POINTER;

	*foreground = CI_BLACK;
	*background = CI_CYAN;
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::GetDefaultLineStyle(COLORINDEX* lineColor, LINESTYLE* lineIndex)
{
	// This shouldn't be called because we haven't requested MV_LINE or MV_BORDER.
	if(!lineColor || !lineIndex)
		return E_POINTER;

	*lineColor = CI_BLACK;
	*lineIndex = LI_SOLID;
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::GetDefaultFontFlags(DWORD* fontFlags)
{
	if(!fontFlags)
		return E_POINTER;

	*fontFlags = FF_DEFAULT;
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::DrawGlyphWithColors(HDC /*hdc*/, RECT* /*rect*/, long /*markerType*/, IVsTextMarkerColorSet* /*markerColors*/, DWORD /*glyphDrawFlags*/, long /*lineHeight*/)
{
	// This shouldn't be called because we haven't requested MV_GLYPH.
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::GetBehaviorFlags(DWORD* flags)
{
	if(!flags)
		return E_POINTER;

	*flags = MB_DEFAULT;
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::GetPriorityIndex(long* priorityIndex)
{
	if(!priorityIndex)
		return E_POINTER;

	*priorityIndex = 100;
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::GetCanonicalName(BSTR* nonLocalizeName)
{
	if(!nonLocalizeName)
		return E_POINTER;

	*nonLocalizeName = ::SysAllocString(L"MetalScroll");
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::GetDisplayName(BSTR* displayName)
{
	if(!displayName)
		return E_POINTER;

	*displayName = ::SysAllocString(L"MetalScroll");
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::GetMergingPriority(long* mergingPriority)
{
	if(!mergingPriority)
		return E_POINTER;

	*mergingPriority = 0x2000;
	return S_OK;
}

STDMETHODIMP CMetalScrollPackage::GetDescription(BSTR* desc)
{
	if(!desc)
		return E_POINTER;

	*desc = ::SysAllocString(L"MetalScroll");
	return S_OK;
}
