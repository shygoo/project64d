#include <stdafx.h>
#include "DisplayListParser.h"

#include "DisplayListOps.h"
#include "DisplayListDecode.h"

////////////

uint32_t CHleDmemState::SegmentedToPhysical(uint32_t address)
{
	uint32_t segment = (address >> 24) & 0x0F;
	uint32_t offset = address & 0x00FFFFFF;
	return segments[segment] + offset;
}

uint32_t CHleDmemState::SegmentedToVirtual(uint32_t address)
{
    return SegmentedToPhysical(address) | 0x80000000;
}

/************/

CDisplayListParser::CDisplayListParser(void) :
	m_UCodeChecksum(0),
	m_UCodeVersion(UCODE_UNKNOWN),
	m_UCodeName(NULL),
	m_CommandTable(NULL),
	m_RootDListSize(0),
	m_RootDListEndAddress(0),
	m_VertexBufferSize(16),
	m_RamSnapshot(NULL),
    m_nCommand(0)
{
}

uint8_t* CDisplayListParser::GetRamSnapshot(void)
{
	return m_RamSnapshot;
}

CHleDmemState* CDisplayListParser::GetLoggedState(size_t index)
{
    if (index >= m_StateLog.size())
    {
        return NULL;
    }

    return &m_StateLog[index];
}

void CDisplayListParser::Reset(uint32_t ucodeAddr, uint32_t dlistAddr, uint32_t dlistSize)
{
	m_UCodeChecksum = 0;
	m_UCodeVersion = UCODE_UNKNOWN;
	m_UCodeName = "unknown microcode";
	m_CommandTable = NULL;
	m_RootDListSize = dlistSize;
	m_RootDListEndAddress = dlistAddr + dlistSize; // use as endpoint if ucode is unknown
	m_VertexBufferSize = 16;
    m_nCommand = 0;

	if (m_RamSnapshot != NULL)
	{
		free(m_RamSnapshot);
	}

	uint32_t ramSize = g_MMU->RdramSize();
	m_RamSnapshot = (uint8_t*)malloc(ramSize);
	memcpy(m_RamSnapshot, g_MMU->Rdram(), ramSize);

	memset(&m_State, 0, sizeof(m_State));
	m_State.address = dlistAddr;

    m_StateLog.clear();

    uint8_t* ucode = m_RamSnapshot + ucodeAddr;

    for (int i = 0; i < 3072; i += sizeof(uint32_t))
    {
        m_UCodeChecksum += *(uint32_t*)&ucode[i];
    }

    const ucode_version_info_t* info = GetUCodeVersionInfo(m_UCodeChecksum);

    if (info != NULL)
    {
		m_UCodeVersion = info->version;
		m_UCodeName = info->name;
		m_CommandTable = info->commandTable;
    }
}

void CDisplayListParser::Run(void)
{
    while (!m_State.bDone)
    {
        StepDecode();
    }
}

// decode and execute command
void CDisplayListParser::StepDecode(void)
{
    uint32_t physAddress = m_State.SegmentedToPhysical(m_State.address);

    g_MMU->LW_PAddr(physAddress, m_State.command.w0);
    g_MMU->LW_PAddr(physAddress + 4, m_State.command.w1);

    m_StateLog.push_back(m_State);
    m_State.address += 8;

    uint8_t commandByte = m_State.command.w0 >> 24;

    const dl_cmd_info_t *commandInfo;
    decode_context_t dc;

	memset(&dc, 0, sizeof(decode_context_t));
	dc.name = "?";
	sprintf(dc.params, "?");
    dc.dramResource.nCommand = m_nCommand;

    commandInfo = LookupCommand(Commands_Global, commandByte);

    if (commandInfo == NULL && m_CommandTable != NULL)
    {
        commandInfo = LookupCommand(m_CommandTable, commandByte);
    }

    if (commandInfo != NULL)
    {
		dc.name = commandInfo->commandName;

		// disassemble command
		if (commandInfo->decodeFunc != NULL)
		{
			commandInfo->decodeFunc(&m_State, &dc);
		}

		// execute command
		if (commandInfo->opFunc != NULL)
		{
			commandInfo->opFunc(&m_State);
		}
    }
	
	if (m_UCodeVersion == UCODE_UNKNOWN && m_State.address >= m_RootDListEndAddress)
	{
		m_State.bDone = true;
	}
    
    m_DecodedCommands.push_back(dc);
    m_nCommand++;
}

const ucode_version_info_t* CDisplayListParser::GetUCodeVersionInfo(uint32_t checksum)
{
    for (int i = 0; UCodeVersions[i].name != NULL; i++)
    {
        if (UCodeVersions[i].checksum == checksum)
        {
            return &UCodeVersions[i];
        }
    }

    return NULL;
}

ucode_version_t CDisplayListParser::GetUCodeVersion(void)
{
	return m_UCodeVersion;
}

const char* CDisplayListParser::GetUCodeName(void)
{
	return m_UCodeName;
}

uint32_t CDisplayListParser::GetUCodeChecksum(void)
{
	return m_UCodeChecksum;
}

//bool CDisplayListParser::IsDone(void)
//{
//    return m_State.bDone;
//}

uint32_t CDisplayListParser::GetCommandAddress(void)
{
    return m_State.address;
}

uint32_t CDisplayListParser::GetCommandVirtualAddress(void)
{
    return m_State.SegmentedToVirtual(m_State.address);
}

dl_cmd_t CDisplayListParser::GetRawCommand(void)
{
    uint32_t physAddress = m_State.SegmentedToPhysical(m_State.address);

    dl_cmd_t cmd;
    g_MMU->LW_PAddr(physAddress, cmd.w0);
    g_MMU->LW_PAddr(physAddress + 4, cmd.w1);

    return cmd;
}

int CDisplayListParser::GetStackIndex(void)
{
    return m_State.stackIndex;
}

dl_cmd_info_t* CDisplayListParser::LookupCommand(dl_cmd_info_t* commands, uint8_t cmdByte)
{
	for (int i = 0; commands[i].commandName != NULL; i++)
	{
		if (commands[i].commandByte == cmdByte)
		{
			return &commands[i];
		}
	}

	return NULL;
}

const char* CDisplayListParser::LookupName(name_lut_entry_t* set, uint32_t value)
{
    for (int i = 0; set[i].name != NULL; i++)
    {
        if (set[i].value == value)
        {
            return set[i].name;
        }
    }

    return NULL;
}

bool CDisplayListParser::ConvertImage(uint32_t* dst, uint8_t* src, im_fmt_t fmt, im_siz_t siz, int numTexels)
{
	if (fmt == G_IM_FMT_RGBA && siz == G_IM_SIZ_16b)
	{
		for (int i = 0; i < numTexels; i++)
		{
			uint16_t px = *(uint16_t*)&src[(i * 2) ^ 2];
			uint8_t r = ((px >> 11) & 0x1F) * (255.0f / 32.0f);
			uint8_t g = ((px >> 6) & 0x1F) * (255.0f / 32.0f);
			uint8_t b = ((px >> 1) & 0x1F) * (255.0f / 32.0f);
			uint8_t alpha = (px & 1) * 255;
			dst[i] = alpha << 24 | (r << 16) | (g << 8) | (b << 0);
		}
	}
	else if (fmt == G_IM_FMT_IA && siz == G_IM_SIZ_16b)
	{
		for (int i = 0; i < numTexels; i++)
		{
			uint16_t px = *(uint16_t*)&src[(i * 2) ^ 2];
			uint8_t intensity = (px >> 8);
			uint8_t alpha = px & 0xFF;
			dst[i] = (alpha << 24) | (intensity << 16) | (intensity << 8) | (intensity << 0);
		}
	}
	else if (fmt == G_IM_FMT_IA && siz == G_IM_SIZ_8b)
	{
		for (int i = 0; i < numTexels; i++)
		{
			uint8_t px = src[i ^ 3];
			uint8_t intensity = (px >> 4) * 0x11;
			uint8_t alpha = (px & 0xF) * 0x11;
			dst[i] = (alpha << 24) | (intensity << 16) | (intensity << 8) | (intensity << 0);
		}
	}
	else if (fmt == G_IM_FMT_I && siz == G_IM_SIZ_8b)
	{
		for (int i = 0; i < numTexels; i++)
		{
			uint8_t intensity = src[i ^ 3];
			dst[i] = (0xFF << 24) | (intensity << 16) | (intensity << 8) | (intensity << 0);
		}
	}
	else
	{
		return false;
		//MessageBox(NULL, stdstr_f("unhandled image texel/fmt combo fmt:%d siz:%d", res->imageFmt, res->imageSiz).c_str(), MB_OK);
	}

	return true;
}