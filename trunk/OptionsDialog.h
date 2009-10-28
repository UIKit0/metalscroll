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

#include "ColorChip.h"

class OptionsDialog
{
public:
	void						Execute(EnvDTE::_DTE* dte);

private:
	enum DlgRetCode
	{
		DlgRet_Cancel,
		DlgRet_Ok,
		DlgRet_Reset
	};

	static INT_PTR CALLBACK		DlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	ColorChip					m_whiteSpace;
	ColorChip					m_comments;
	ColorChip					m_uppercase;
	ColorChip					m_otherChars;
	ColorChip					m_matchingWord;
	ColorChip					m_modifLineSaved;
	ColorChip					m_modifLineUnsaved;
	ColorChip					m_cursorColor;
	unsigned int				m_cursorTrans;
	unsigned int				m_barWidth;

	void						InitDialog(HWND hwnd);
};
