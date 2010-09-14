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
#include "OptionsDialog.h"
#include "resource.h"
#include "MetalBar.h"

extern HWND		g_mainVSHwnd;

void OptionsDialog::InitDialog(HWND hwnd)
{
	SetWindowLongPtr(hwnd, GWL_USERDATA, (::LONG_PTR)this);

	m_whiteSpace.Init(GetDlgItem(hwnd, IDC_WHITESPACE), MetalBar::s_whitespaceColor);
	m_comments.Init(GetDlgItem(hwnd, IDC_COMMENTS), MetalBar::s_commentColor);
	m_uppercase.Init(GetDlgItem(hwnd, IDC_UPPERCASE), MetalBar::s_upperCaseColor);
	m_otherChars.Init(GetDlgItem(hwnd, IDC_CHARACTERS), MetalBar::s_characterColor);
	m_matchingWord.Init(GetDlgItem(hwnd, IDC_MATCHING_WORD), MetalBar::s_matchColor);
	m_modifLineSaved.Init(GetDlgItem(hwnd, IDC_MODIF_LINE_SAVED), MetalBar::s_modifiedLineColor);
	m_modifLineUnsaved.Init(GetDlgItem(hwnd, IDC_MODIF_LINE_UNSAVED), MetalBar::s_unsavedLineColor);
	m_breakpoints.Init(GetDlgItem(hwnd, IDC_BREAKPOINTS), MetalBar::s_breakpointColor);
	m_bookmarks.Init(GetDlgItem(hwnd, IDC_BOOKMARKS), MetalBar::s_bookmarkColor);
	m_cursorColor.Init(GetDlgItem(hwnd, IDC_CURSOR_COLOR), MetalBar::s_cursorColor);
	m_previewBg.Init(GetDlgItem(hwnd, IDC_PREVIEW_BG), MetalBar::s_codePreviewBg);
	m_previewFg.Init(GetDlgItem(hwnd, IDC_PREVIEW_FG), MetalBar::s_codePreviewFg);

	int cursorTrans = MetalBar::s_cursorColor >> 24;
	SetDlgItemInt(hwnd, IDC_CURSOR_TRANS, cursorTrans, FALSE);
	SetDlgItemInt(hwnd, IDC_BAR_WIDTH, MetalBar::s_barWidth, FALSE);
	SetDlgItemInt(hwnd, IDC_PREVIEW_WIDTH, MetalBar::s_codePreviewWidth, FALSE);
	SetDlgItemInt(hwnd, IDC_PREVIEW_HEIGHT, MetalBar::s_codePreviewHeight, FALSE);

	int state = MetalBar::s_requireAltForHighlight ? BST_CHECKED : BST_UNCHECKED;
	CheckDlgButton(hwnd, IDC_REQUIRE_ALT, state);
}

int OptionsDialog::GetInt(HWND hwnd, int dlgItem, int defVal)
{
	BOOL ok;
	int val = GetDlgItemInt(hwnd, dlgItem, &ok, FALSE);
	return ok ? val : defVal;
}

void OptionsDialog::OnOK(HWND hwnd)
{
	// Read the integers.
	m_cursorTrans = GetInt(hwnd, IDC_CURSOR_TRANS, MetalBar::s_cursorColor >> 24);
	m_barWidth = GetInt(hwnd, IDC_BAR_WIDTH, MetalBar::s_barWidth);
	m_codePreviewWidth = GetInt(hwnd, IDC_PREVIEW_WIDTH, MetalBar::s_codePreviewWidth);
	m_codePreviewHeight = GetInt(hwnd, IDC_PREVIEW_HEIGHT, MetalBar::s_codePreviewHeight);

	m_requireALT = (IsDlgButtonChecked(hwnd, IDC_REQUIRE_ALT) == BST_CHECKED);
}

INT_PTR CALLBACK OptionsDialog::DlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if(msg == WM_INITDIALOG)
	{
		OptionsDialog* dlg = (OptionsDialog*)lparam;
		dlg->InitDialog(hwnd);
		return TRUE;
	}

	switch(msg)
	{
		case WM_CLOSE:
			EndDialog(hwnd, DlgRet_Cancel);
			break;

		case WM_COMMAND:
			switch(wparam)
			{
				case IDOK:
				{
					OptionsDialog* dlg = (OptionsDialog*)GetWindowLongPtr(hwnd, GWL_USERDATA);
					dlg->OnOK(hwnd);

					// Close the dialog.
					EndDialog(hwnd, DlgRet_Ok);
					break;
				}

				case IDCANCEL:
					EndDialog(hwnd, DlgRet_Cancel);
					break;

				case IDC_RESET:
				{
					if(MessageBoxA(hwnd, "Are you sure you want to reset all settings?", "MetalScroll", MB_YESNO|MB_ICONQUESTION|MB_APPLMODAL) == IDYES)
						EndDialog(hwnd, DlgRet_Reset);
				}
			}
			break;

		default:
			return 0;
	}

	return 1;
}

void OptionsDialog::Execute()
{
	int ret = DialogBoxParamA(_AtlModule.GetResourceInstance(), (LPSTR)IDD_OPTIONS, g_mainVSHwnd, DlgProc, (LPARAM)this);
	if(ret == DlgRet_Cancel)
		return;

	if(ret == DlgRet_Reset)
	{
		MetalBar::ResetSettings();
		MetalBar::SaveSettings();
		return;
	}

	MetalBar::s_whitespaceColor = m_whiteSpace.GetColor();
	MetalBar::s_commentColor = m_comments.GetColor();
	MetalBar::s_upperCaseColor = m_uppercase.GetColor();
	MetalBar::s_characterColor = m_otherChars.GetColor();
	MetalBar::s_matchColor = m_matchingWord.GetColor();
	MetalBar::s_modifiedLineColor = m_modifLineSaved.GetColor();
	MetalBar::s_unsavedLineColor = m_modifLineUnsaved.GetColor();
	MetalBar::s_breakpointColor = m_breakpoints.GetColor();
	MetalBar::s_bookmarkColor = m_bookmarks.GetColor();
	MetalBar::s_codePreviewBg = m_previewBg.GetColor();
	MetalBar::s_codePreviewFg = m_previewFg.GetColor();
	MetalBar::s_cursorColor = (m_cursorColor.GetColor() & 0xffffff) | ((m_cursorTrans & 0xff) << 24);
	MetalBar::s_barWidth = std::max((int)m_barWidth, 8);
	MetalBar::s_codePreviewWidth = m_codePreviewWidth;
	MetalBar::s_codePreviewHeight = m_codePreviewHeight;
	MetalBar::s_requireAltForHighlight = m_requireALT;

	MetalBar::SaveSettings();
}

void OptionsDialog::Init()
{
	ColorChip::Register();
}

void OptionsDialog::Uninit()
{
	ColorChip::Unregister();
}
