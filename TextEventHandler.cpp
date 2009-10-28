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
#include "TextEventHandler.h"
#include "MetalBar.h"

CTextEventHandler::CTextEventHandler()
{
	m_cookie = 0;
	m_bar = 0;
}

CTextEventHandler::~CTextEventHandler()
{
	assert(!m_cookie);
	assert(!m_bar);
}

bool CTextEventHandler::GetConnectionPoint(const MetalBar* bar, IConnectionPoint** connPt)
{
	IVsTextView* view = bar->GetView();
	assert(view);

	CComPtr<IVsTextLines> lines;
	HRESULT hr = view->GetBuffer(&lines);
	if(FAILED(hr) || !lines)
		return false;

	CComQIPtr<IConnectionPointContainer> connPoints = lines;
	if(!connPoints)
		return false;

	hr = connPoints->FindConnectionPoint(__uuidof(IVsTextLinesEvents), connPt);
	return (SUCCEEDED(hr) && *connPt);
}

CTextEventHandler* CTextEventHandler::CreateHandler(MetalBar* bar)
{
	CComPtr<IConnectionPoint> connPt;
	if(!GetConnectionPoint(bar, &connPt))
		return 0;

	CTextEventHandler* handler;
	HRESULT hr = CoCreateInstance(__uuidof(TextEventHandler), 0, CLSCTX_INPROC_SERVER, IID_ITextEventHandler, (void**)&handler);
	if(FAILED(hr) || !handler)
		return 0;

	hr = connPt->Advise((IVsTextLinesEvents*)handler, &handler->m_cookie);
	if(FAILED(hr))
	{
		handler->Release();
		return 0;
	}

	handler->m_bar = bar;
	return handler;
}

void CTextEventHandler::RemoveHandler()
{
	if(!m_cookie || !m_bar)
	{
		m_cookie = 0;
		m_bar = 0;
		return;
	}

	CComPtr<IConnectionPoint> connPt;
	if(GetConnectionPoint(m_bar, &connPt))
		connPt->Unadvise(m_cookie);

	m_cookie = 0;
	m_bar = 0;
}

void STDMETHODCALLTYPE CTextEventHandler::OnChangeLineText(const TextLineChange* textLineChange, BOOL /*last*/)
{
	assert(m_bar);
	m_bar->OnCodeChanged(textLineChange);
}
