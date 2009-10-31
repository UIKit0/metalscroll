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

#ifndef STRICT
#define STRICT
#endif

#define WINVER						0x0500
#define _WIN32_WINNT				0x0500
#define _WIN32_IE					0x0500
#define _SECURE_SCL					0
#define _CRT_SECURE_NO_DEPRECATE
#define NOMINMAX

#define _ATL_APARTMENT_THREADED
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _ATL_ALL_WARNINGS

#pragma warning(push)
#pragma warning(disable: 4278)
#pragma warning(disable: 4146)

#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include <commctrl.h>

// IDTExtensibility2
#import "libid:AC0714F2-3D04-11D1-AE7D-00A0C90F26F4" version("1.0") lcid("0")  raw_interfaces_only named_guids

// VS Command Bars based on it's LIBID
#import "libid:1CBA492E-7263-47BB-87FE-639000619B15" version("8.0") lcid("0") raw_interfaces_only named_guids

// DTE based
#import "libid:80cc9f66-e7d8-4ddd-85b6-d9e6cd0e93e2" version("8.0") lcid("0") raw_interfaces_only named_guids

// DTE80
#import "libid:1A31287A-4D7D-413e-8E32-3B374931BD89" version("8.0") lcid("0") raw_interfaces_only named_guids

#pragma warning(pop)

#include <textmgr.h>
#include <vsshell.h>
#include <proffserv.h>

class DECLSPEC_UUID("694BCCDA-B2B0-4A97-BB4B-E7B128B05D76") MetalScrollLib;

class CAddInModule : public CAtlDllModuleT< CAddInModule >
{
public:
	CAddInModule()
	{
		m_hInstance = NULL;
	}

	DECLARE_LIBID(__uuidof(MetalScrollLib))

	inline HINSTANCE GetResourceInstance()
	{
		return m_hInstance;
	}

	inline void SetResourceInstance(HINSTANCE hInstance)
	{
		m_hInstance = hInstance;
	}

private:
	HINSTANCE m_hInstance;
};

#define RGB_TO_COLORREF(rgb) ((rgb & 0xff00) | ((rgb & 0xff) << 16) | ((rgb & 0xff0000) >> 16))
#define COLORREF_TO_RGB(cr) ((cr & 0xff00) | ((cr & 0xff) << 16) | ((cr & 0xff0000) >> 16) | 0xff000000)

extern CAddInModule _AtlModule;

#include <assert.h>
#include <vector>
#include <set>
#include <algorithm>
