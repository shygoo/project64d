/****************************************************************************
*                                                                           *
* Project64 - A Nintendo 64 emulator.                                      *
* http://www.pj64-emu.com/                                                  *
* Copyright (C) 2012 Project64. All rights reserved.                        *
*                                                                           *
* License:                                                                  *
* GNU/GPLv2 http://www.gnu.org/licenses/gpl-2.0.html                        *
*                                                                           *
****************************************************************************/

#include "stdafx.h"
#include "DebuggerUI.h"

#include <Project64/UserInterface/resource.h>
#include <Project64-core/N64System/Mips/OpCodeName.h>
#include <Project64-core/N64System/Interpreter/InterpreterDBG.h>

const int CDebugCommandsView::listLength = 36;

CDebugCommandsView::CDebugCommandsView(CDebuggerUI * debugger) :
CDebugDialog<CDebugCommandsView>(debugger)
{
	//m_RegisterTabs = new CRegisterTabs();
	//m_CommandList = new CCommandsList();
	//m_AddBreakpointDlg = new CAddBreakpointDlg();

	m_StartAddress = 0x80000000;
}

CDebugCommandsView::~CDebugCommandsView(void)
{

}

void CDebugCommandsView::ShowAddress(DWORD address, BOOL top)
{
	if (top == TRUE || address < m_StartAddress || address > m_StartAddress + (listLength-1) * 4)
	{
		m_StartAddress = address;
	}
	
	m_CommandList.SetRedraw(FALSE);
	m_CommandList.DeleteAllItems();

	char addrStr[9];

	for (int i = 0; i < listLength; i++)
	{	
		sprintf(addrStr, "%08X", m_StartAddress + i * 4);
		
		m_CommandList.AddItem(i, 0, addrStr);

		if (g_MMU == NULL)
		{
			m_CommandList.AddItem(i, 1, "???");
			m_CommandList.AddItem(i, 1, "???");
			continue;
		}
		
		OPCODE& OpCode = OPCODE();
		g_MMU->LW_VAddr(m_StartAddress + i * 4, OpCode.Hex);

		char* command = (char*)R4300iOpcodeName(OpCode.Hex, m_StartAddress + i * 4);
		char* cmdName = strtok((char*)command, "\t");
		char* cmdArgs = strtok(NULL, "\t");

		m_CommandList.AddItem(i, 1, cmdName);
		m_CommandList.AddItem(i, 2, cmdArgs);
	}

	m_CommandList.SetRedraw(TRUE);
}

void CDebugCommandsView::RefreshBreakpointList() {
	m_BreakpointList.ResetContent();
	char rowStr[16];
	for (int i = 0; i < CInterpreterDBG::m_nEBP; i++) {
		sprintf(rowStr, "E %08X", CInterpreterDBG::m_EBP[i]);
		m_BreakpointList.AddString(rowStr);
	}
	for (int i = 0; i < CInterpreterDBG::m_nWBP; i++) {
		sprintf(rowStr, "R %08X", CInterpreterDBG::m_WBP[i]);
		m_BreakpointList.AddString(rowStr);
	}
	for (int i = 0; i < CInterpreterDBG::m_nRBP; i++) {
		sprintf(rowStr, "W %08X", CInterpreterDBG::m_RBP[i]);
		m_BreakpointList.AddString(rowStr);
	}
}

LRESULT	CDebugCommandsView::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_RegisterTabs.Attach(GetDlgItem(IDC_CMD_REGTABS));
	m_CommandList.Attach(GetDlgItem(IDC_CMD_LIST));
	
	m_EditAddress.Attach(GetDlgItem(IDC_CMD_ADDR));
	m_BreakpointList.Attach(GetDlgItem(IDC_CMD_BPLIST));

	ShowAddress(0x80000000, TRUE);

	uint32_t cpuType = (CPU_TYPE)g_Settings->LoadDword(Game_CpuType);

	if (cpuType != CPU_TYPE::CPU_Interpreter)
	{
		MessageBox("Interpreter mode required", "Invalid CPU Type", MB_OK);
	}

	WindowCreated();

	return TRUE;
}

LRESULT CDebugCommandsView::OnDestroy(void)
{
	return 0;
}

LRESULT CDebugCommandsView::OnCustomDrawList(NMHDR* pNMHDR) {
	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
	DWORD drawStage = pLVCD->nmcd.dwDrawStage;
	
	if (drawStage == CDDS_PREPAINT)
	{
		return CDRF_NOTIFYITEMDRAW;
	}

	if (drawStage == CDDS_ITEMPREPAINT)
	{
		return CDRF_NOTIFYSUBITEMDRAW;
	}

	if(drawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
	{
		DWORD nItem = pLVCD->nmcd.dwItemSpec;
		DWORD nSubItem = pLVCD->iSubItem;
		
		COLORREF clrText;
		uint32_t address = m_StartAddress + (nItem * 4);
		uint32_t pc = (g_Reg != NULL) ? g_Reg->m_PROGRAM_COUNTER : 0;

		if (nSubItem == 0) { // addr
			if (CInterpreterDBG::EBPExists(address))
			{
				// pc breakpoint
				pLVCD->clrTextBk = RGB(0x44, 0x00, 0x00);
				pLVCD->clrText = (pc == address) ? RGB(0xFF, 0xFF, 0x00) : RGB(0xFF, 0xCC, 0xCC);
			}
			else if (pc == address)
			{
				// pc
				pLVCD->clrTextBk = RGB(0x88, 0x88, 0x88);
				pLVCD->clrText = RGB(0xFF, 0xFF, 0);
			}
			else
			{
				//default
				pLVCD->clrTextBk = RGB(0xEE, 0xEE, 0xEE);
				pLVCD->clrText = RGB(0x44, 0x44, 0x44);
			}
		}
		else if (nSubItem == 1 || nSubItem == 2) // cmd & args
		{
			OPCODE Opcode = OPCODE();
			if (g_MMU != NULL) {
				g_MMU->LW_VAddr(address, Opcode.Hex);
			}
			else {
				Opcode.Hex = 0x00000000;
			}
			
			if (pc == address)
			{
				//pc
				pLVCD->clrTextBk = RGB(0xFF, 0xFF, 0xAA);
				pLVCD->clrText = RGB(0x22, 0x22, 0);
			}
			else if (Opcode.op == R4300i_ADDIU && Opcode.rt == 29) // stack shift
			{
				if ((short)Opcode.immediate < 0) // alloc
				{
					// sky blue bg, dark blue fg
					pLVCD->clrTextBk = RGB(0xDD, 0xDD, 0xFF);
					pLVCD->clrText = RGB(0x00, 0x00, 0x44);
				}
				else // free
				{
					// salmon bg, dark red fg
					pLVCD->clrTextBk = RGB(0xFF, 0xDD, 0xDD);
					pLVCD->clrText = RGB(0x44, 0x00, 0x00);
				}
			}
			else if (Opcode.Hex == 0x00000000) // nop
			{
				// gray fg
				//pLVCD->clrTextBk = RGB(0xEE, 0xEE, 0xEE);
				pLVCD->clrTextBk = RGB(0xFF, 0xFF, 0xFF);
				pLVCD->clrText = RGB(0x88, 0x88, 0x88);
			}
			else if (nSubItem == 1 && (Opcode.op == R4300i_J || Opcode.op == R4300i_JAL || (Opcode.op == 0 && Opcode.funct == R4300i_SPECIAL_JR)))
			{
				// jumps
				pLVCD->clrText = RGB(0x00, 0x88, 0x00);
				pLVCD->clrTextBk = RGB(0xFF, 0xFF, 0xFF);
			}
			else {
				pLVCD->clrTextBk = RGB(0xFF, 0xFF, 0xFF);
				pLVCD->clrText = RGB(0x00, 0x00, 0x00);
			}
		}
		/*
		else if (nSubItem == 2) // args
		{

			pLVCD->clrTextBk = RGB(0xFF, 0xFF, 0xFF);
			pLVCD->clrText = RGB(0x00, 0x00, 0x00);
		}*/

	}

	//m_CommandList.RedrawWindow();
	return CDRF_DODEFAULT;
}

LRESULT CDebugCommandsView::OnClicked(WORD wNotifyCode, WORD wID, HWND, BOOL & bHandled)
{
	switch (wID)
	{
	case IDC_CMD_BTN_BPCLEAR:
		CInterpreterDBG::BPClear();
		RefreshBreakpointList();
		ShowAddress(m_StartAddress, TRUE);
		break;
	case IDC_CMD_BTN_GO:
		CInterpreterDBG::m_Debugging = FALSE;
		CInterpreterDBG::Resume();
		break;
	case IDC_CMD_BTN_STEP:
		CInterpreterDBG::KeepDebugging();
		CInterpreterDBG::Resume();
		break;
	case IDCANCEL:
		EndDialog(0);
		break;
	case IDC_CMD_ADDBP:
		m_AddBreakpointDlg.DoModal();
		RefreshBreakpointList();
		ShowAddress(m_StartAddress, TRUE);
		break;
	}
	return FALSE;
}

LRESULT CDebugCommandsView::OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_StartAddress -= (short)HIWORD(wParam) / 30;
	ShowAddress(m_StartAddress, TRUE);

	char addrStr[9];
	snprintf(addrStr, 9, "%08X", m_StartAddress);
	GetDlgItem(IDC_CMD_ADDR).SetWindowText(addrStr);

	return TRUE;
}

void CDebugCommandsView::GotoEnteredAddress()
{
	char text[9];
	text[8] = '\0';
	GetDlgItemText(IDC_CMD_ADDR, text, 9);
	DWORD address = strtoul(text, NULL, 16);
	address &= 0x003FFFFF;
	address |= 0x80000000;
	address = address - address % 4;
	ShowAddress(address, TRUE);
}

LRESULT CDebugCommandsView::OnAddrChanged(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	GotoEnteredAddress();
	return 0;
}


LRESULT	CDebugCommandsView::OnListClicked(NMHDR* pNMHDR)
{
	NMITEMACTIVATE* pIA = reinterpret_cast<NMITEMACTIVATE*>(pNMHDR);
	int nItem = pIA->iItem;

	uint32_t address = m_StartAddress + nItem * 4;

	if (CInterpreterDBG::EBPExists(address))
	{
		CInterpreterDBG::EBPRemove(address);
	}
	else
	{
		CInterpreterDBG::EBPAdd(address);
	}

	// cancel blue highlight
	m_EditAddress.SetFocus();

	RefreshBreakpointList();

	return CDRF_DODEFAULT;
}

// Add breakpoint dialog

LRESULT CAddBreakpointDlg::OnClicked(WORD wNotifyCode, WORD wID, HWND, BOOL & bHandled) {
	switch (wID)
	{
	case IDOK: {
		char addrStr[9];
		GetDlgItemText(IDC_ABP_ADDR, addrStr, 9);

		uint32_t address = strtoul(addrStr, NULL, 16);
		CInterpreterDBG::EBPAdd(address);
		
		EndDialog(0);
		break;
	}
	case IDCANCEL:
		EndDialog(0);
		break;
	}

	return FALSE;
}



LRESULT CAddBreakpointDlg::OnDestroy(void)
{
	return 0;
}

// Register tabs
void CRegisterTabs::Attach(HWND hWnd)
{
	CTabCtrl::Attach(hWnd);
	DeleteAllItems();
	AddItem("GPR");
	AddItem("COP1");
}

// commands list
void CCommandsList::Attach(HWND hWnd)
{
	CListViewCtrl::Attach(hWnd);
	AddColumn("Address", 0);
	AddColumn("Command", 1);
	AddColumn("Parameters", 2);
	SetColumnWidth(0, 60);
	SetColumnWidth(1, 60);
	SetColumnWidth(2, 120);
	SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
	//SetExtendedListViewStyle();
}

/*
LRESULT CCommandsList::OnMouseWheel(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	MessageBox("wheel used", "aaaa", MB_OK);
	// forward scroll event to parent window
	SendMessage(m_ParentWindow, uMsg, wParam, lParam);
	return TRUE;
}
*/

