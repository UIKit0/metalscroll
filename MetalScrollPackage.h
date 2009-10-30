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
	public VSL::IVsPackageImpl<CMetalScrollPackage, &CLSID_MetalScrollPackage>,
	public ATL::ISupportErrorInfoImpl<&__uuidof(IVsPackage)>
{
public:
	CMetalScrollPackage()
	{
	}

	DECLARE_REGISTRY_RESOURCEID(IDR_METALSCROLLPACKAGE)

	BEGIN_COM_MAP(CMetalScrollPackage)
		COM_INTERFACE_ENTRY(IVsPackage)
		COM_INTERFACE_ENTRY(ISupportErrorInfo)
	END_COM_MAP()

	DECLARE_PROTECT_FINAL_CONSTRUCT()

	HRESULT FinalConstruct()
	{
		return S_OK;
	}

	void FinalRelease()
	{
	}

	static const LoadUILibrary::ExtendedErrorInfo& GetLoadUILibraryErrorInfo()
	{
		static LoadUILibrary::ExtendedErrorInfo errorInfo(L"The product is not installed properly. Please reinstall.");
		return errorInfo;
	}
};

OBJECT_ENTRY_AUTO(__uuidof(MetalScrollPackage), CMetalScrollPackage)
