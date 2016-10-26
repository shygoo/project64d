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

#include <stdio.h>
#include <Common/path.h>

CDebugSymbols::CDebugSymbols(CDebuggerUI * debugger) :
	CDebugDialog<CDebugSymbols>(debugger)
{
	m_SymFileBuffer = NULL;
}
/*

type,address,name,description

u32,80370000,variable1
code,80370000,variable1

*/

int CDebugSymbols::GetTypeNumber(const char* typeName)
{
	const char* name;
	for (int i = 0; (name = symbolTypes[i]) != NULL; i++)
	{	
		if (strcmp(typeName, name) == 0)
		{
			return i;
		}
	}
	return -1;
}

// Open symbols file for game and parse into list

void CDebugSymbols::LoadGameSymbols()
{
	if (g_Settings->LoadStringVal(Game_GameName).length() == 0)
	{
		// no game is loaded
		return;
	}

	stdstr winTitle;
	winTitle.Format("Symbols - %s", g_Settings->LoadStringVal(Game_GameName).c_str());

	SetWindowTextA(winTitle.c_str());

	stdstr symFileName;
	symFileName.Format("%s.sym", g_Settings->LoadStringVal(Game_GameName).c_str());

	CPath symFilePath(g_Settings->LoadStringVal(Directory_NativeSave).c_str(), symFileName.c_str());
	
	if (g_Settings->LoadBool(Setting_UniqueSaveDir))
	{
		symFilePath.AppendDirectory(g_Settings->LoadStringVal(Game_UniqueSaveDir).c_str());
	}
	if (!symFilePath.DirectoryExists())
	{
		symFilePath.DirectoryCreate();
	}

	m_SymFileHandle.Open(symFilePath, CFileBase::modeReadWrite);
	m_SymFileHandle.SeekToBegin();

	if (m_SymFileBuffer != NULL)
	{
		free(m_SymFileBuffer);
	}

	uint32_t symFileSize = m_SymFileHandle.GetLength();
	m_SymFileBuffer = (char*)malloc(symFileSize + 1);
	m_SymFileBuffer[symFileSize] = '\0';
	m_SymFileHandle.Read(m_SymFileBuffer, symFileSize);

	char* bufCopy = (char*)malloc(symFileSize + 1);
	strcpy(bufCopy, m_SymFileBuffer);
	
	char* curToken = strtok(m_SymFileBuffer, ",");
	
	SYMBOLENTRY curSymbolEntry;

	int listIndex = 0;
	int lineNumber = 1;
	int fieldPhase = 0;
	m_NextSymbolId = 0;
	
	ParseError errorCode = ERR_SUCCESS;

	while (curToken != NULL)
	{
		char usedDelimeter = bufCopy[curToken - m_SymFileBuffer + strlen(curToken)];
		
		switch (fieldPhase)
		{
			case 0: {// address
				memset(&curSymbolEntry, 0x00, sizeof(SYMBOLENTRY));
				char* endptr;
				curSymbolEntry.address = strtoull(curToken, &endptr, 16);
				curSymbolEntry.address_str = curToken;
				if (*endptr != NULL)
				{
					errorCode = ERR_INVALID_ADDR;
					goto error_check;
				}
				break;
			}
			case 1: { // type
				int typeNum = GetTypeNumber(curToken);
				if (typeNum == -1)
				{
					errorCode = ERR_INVALID_TYPE;
					goto error_check;
				}
				curSymbolEntry.type = typeNum;
				curSymbolEntry.type_str = curToken;
				break;
			}
			case 2: { // name
				curSymbolEntry.name = curToken;
				if (strlen(curSymbolEntry.name) == 0)
				{
					errorCode = ERR_INVALID_ADDR;
					goto error_check;
				}
				break;
			}
			case 3: { // desc
				curSymbolEntry.description = curToken;
				break;
			}
		}

		// Check if line has ended
		if (usedDelimeter == '\n' || usedDelimeter == '\0')
		{
			if (fieldPhase < 2)
			{
				errorCode = ERR_MISSING_FIELDS;
				goto error_check;
			}

			curSymbolEntry.id = m_NextSymbolId++;
			m_Symbols.push_back(curSymbolEntry);
			
			m_SymbolsListView.AddItem(listIndex, 0, curSymbolEntry.address_str);
			m_SymbolsListView.AddItem(listIndex, 1, curSymbolEntry.type_str);
			m_SymbolsListView.AddItem(listIndex, 2, curSymbolEntry.name);
			m_SymbolsListView.AddItem(listIndex, 3, curSymbolEntry.description);
			m_SymbolsListView.SetItemData(listIndex, curSymbolEntry.id);

			lineNumber++;
			listIndex++;
			fieldPhase = 0;
			curToken = strtok(NULL, ",\n");
			continue;
		}

		// Line hasn't ended
		if (fieldPhase + 1 == 3)
		{
			curToken = strtok(NULL, "\n");
		}
		else
		{
			curToken = strtok(NULL, ",\n");
		}
		fieldPhase++;
	}

error_check:
	switch (errorCode)
	{
	case ERR_SUCCESS:
		break;
	case ERR_INVALID_ADDR:
		ParseErrorAlert("Invalid address", lineNumber);
		break;
	case ERR_INVALID_TYPE:
		ParseErrorAlert("Invalid type", lineNumber);
		break;
	case ERR_INVALID_NAME:
		ParseErrorAlert("Invalid name", lineNumber);
		break;
	case ERR_MISSING_FIELDS:
		ParseErrorAlert("Missing required field(s)", lineNumber);
		break;
	}

	free(bufCopy);
}

void CDebugSymbols::ParseErrorAlert(char* message, int lineNumber)
{
	stdstr messageFormatted = stdstr_f("%s\nLine %d", message, lineNumber);
	MessageBox(messageFormatted.c_str(), "Parse error", MB_OK | MB_ICONWARNING);
}

SYMBOLENTRY* CDebugSymbols::GetSymbolEntryById(int id)
{
	int len = m_Symbols.size();
	for (int i = 0; i < len; i++)
	{
		if(m_Symbols[i].id == id)
		{
			return &m_Symbols[i];
		}
	}
}

LRESULT CDebugSymbols::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	m_SymbolsListView.Attach(GetDlgItem(IDC_SYMBOLS_LIST));
	m_SymbolsListView.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

	m_SymbolsListView.AddColumn("Address", 0);
	m_SymbolsListView.AddColumn("Type", 1);
	m_SymbolsListView.AddColumn("Name", 2);
	m_SymbolsListView.AddColumn("Description", 3);
	
	m_SymbolsListView.SetColumnWidth(0, 70);
	m_SymbolsListView.SetColumnWidth(1, 40);
	m_SymbolsListView.SetColumnWidth(2, 100);
	m_SymbolsListView.SetColumnWidth(3, 120);
	
	LoadGameSymbols();

	WindowCreated();
	return 0;
}

LRESULT CDebugSymbols::OnClicked(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
	case IDCANCEL:
		EndDialog(0);
		break;
	}
	return FALSE;
}