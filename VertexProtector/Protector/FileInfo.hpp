#pragma once

#include <Framework/Framework.hpp>

#include <Windows.h>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace Protector::FileInfo
{
	struct FileInfo_t
	{
		std::string path;
		std::string name;
		std::string extension;

		std::vector<uint8_t> data;
		uint64_t size;

		IMAGE_DOS_HEADER* dosHeader;
		IMAGE_NT_HEADERS64* ntHeaders;
	};

	static bool LoadFile(const std::string& filePath, FileInfo_t& outFileInfo)
	{
		outFileInfo.name = filePath.substr(filePath.find_last_of("/\\") + 1);
		outFileInfo.extension = filePath.substr(filePath.find_last_of(".") + 1);
		outFileInfo.path = filePath;

		std::ifstream file(filePath, std::ios::binary | std::ios::ate);
		if (!file)
			return false;

		outFileInfo.size = file.tellg();
		file.close();

		if (!file.read(reinterpret_cast<char*>(outFileInfo.data.data()), outFileInfo.size))
			return false;

		outFileInfo.dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(outFileInfo.data.data());
		if (outFileInfo.dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		{
			DEBUG_PRINT("Invalid DOS signature");
			return false;
		}

		outFileInfo.ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(outFileInfo.data.data() + outFileInfo.dosHeader->e_lfanew);

		if (outFileInfo.ntHeaders->Signature != IMAGE_NT_SIGNATURE)
		{
			DEBUG_PRINT("Invalid PE signature");
			return false;
		}

		DEBUG_PRINT("File size: %llu KB\n", outFileInfo.size / 1024);
		DEBUG_PRINT("Machine: %X\n", outFileInfo.ntHeaders->FileHeader.Machine);
		DEBUG_PRINT("Sections: %u\n", outFileInfo.ntHeaders->FileHeader.NumberOfSections);
		DEBUG_PRINT("Entry Point: %X\n", outFileInfo.ntHeaders->OptionalHeader.AddressOfEntryPoint);
		DEBUG_PRINT("Image Base: %llX\n", outFileInfo.ntHeaders->OptionalHeader.ImageBase);

		const auto& debugDir = outFileInfo.ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];

		if (debugDir.VirtualAddress != 0 && debugDir.Size != 0)
		{

		}

		return true;
	}
}