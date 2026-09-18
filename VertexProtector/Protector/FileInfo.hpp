#pragma once

#include <Framework/Framework.hpp>

#include <Windows.h>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace Protector::FileInfo
{
	struct ImportFunction
	{
		std::string name;
		uint16_t hint = 0;

		uint64_t thunkRva = 0;
		uint64_t iatRva = 0;

		bool importedByOrdinal = false;
		uint16_t ordinal = 0;
	};

	struct ImportModule
	{
		std::string name;

		uint32_t originalFirstThunk = 0;
		uint32_t firstThunk = 0;

		std::vector<ImportFunction> functions;
	};

	struct FileInfo_t
	{
		std::string path;
		std::string name;
		std::string extension;

		std::vector<uint8_t> data;
		uint64_t size;

		IMAGE_DOS_HEADER* dosHeader;
		IMAGE_NT_HEADERS64* ntHeaders;

		IMAGE_IMPORT_DESCRIPTOR* importDescriptor;

		struct ImportsInfo
		{
			bool hasImports = false;

			uint32_t totalImports;
			uint32_t totalModules;

			uint32_t directoryRva = 0;
			uint32_t directorySize = 0;

			std::vector<ImportModule> modules;
		}importsInfo;
	};

	static DWORD RvaToOffset(const FileInfo_t& file, DWORD rva)
	{
		const auto* nt = file.ntHeaders;
		const auto* section = IMAGE_FIRST_SECTION(nt);

		for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
		{
			DWORD sectionVA = section[i].VirtualAddress;
			DWORD sectionSize = section[i].SizeOfRawData;

			if (rva >= sectionVA &&
				rva < sectionVA + sectionSize)
			{
				return section[i].PointerToRawData +
					(rva - sectionVA);
			}
		}

		return 0;
	}

	static bool LoadFile(const std::string& filePath, FileInfo_t& outFileInfo)
	{
		outFileInfo.name = filePath.substr(filePath.find_last_of("/\\") + 1);
		outFileInfo.extension = filePath.substr(filePath.find_last_of(".") + 1);
		outFileInfo.path = filePath;

		std::ifstream file(filePath, std::ios::binary | std::ios::ate);
		if (!file)
			return false;

		outFileInfo.size = static_cast<uint64_t>(file.tellg());
		outFileInfo.data.resize(outFileInfo.size);
		file.seekg(0, std::ios::beg);

		if (!file.read(reinterpret_cast<char*>(outFileInfo.data.data()), outFileInfo.size))
			return false;

		file.close();

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

		if (outFileInfo.ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		{
			DEBUG_PRINT("x64 binaries not supported");
			return false;
		}

		DEBUG_PRINT("File size: %llu KB\n", outFileInfo.size / 1024);
		DEBUG_PRINT("Sections: %u\n", outFileInfo.ntHeaders->FileHeader.NumberOfSections);
		DEBUG_PRINT("Entry Point: %X\n", outFileInfo.ntHeaders->OptionalHeader.AddressOfEntryPoint);
		DEBUG_PRINT("Image Base: %llX\n", outFileInfo.ntHeaders->OptionalHeader.ImageBase);

		const auto& debugDir = outFileInfo.ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];

		if (debugDir.VirtualAddress != 0 && debugDir.Size != 0)
		{

		}

		auto& directoryEntryImport = outFileInfo.ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (directoryEntryImport.VirtualAddress != 0 && directoryEntryImport.Size != 0)
		{
			DWORD offset = RvaToOffset(outFileInfo, directoryEntryImport.VirtualAddress);
			if (offset)
			{
				outFileInfo.importDescriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(outFileInfo.data.data() + offset);

				outFileInfo.importsInfo.hasImports = true;
				outFileInfo.importsInfo.directoryRva = directoryEntryImport.VirtualAddress;
				outFileInfo.importsInfo.directorySize = directoryEntryImport.Size;

				for (auto* desc = outFileInfo.importDescriptor; desc->Name != 0; ++desc)
				{
					DWORD nameOffset = RvaToOffset(outFileInfo, desc->Name);
					if (!nameOffset)
						continue;

					ImportModule module;

					module.name = reinterpret_cast<const char*>(outFileInfo.data.data() + nameOffset);
					module.originalFirstThunk = desc->OriginalFirstThunk;
					module.firstThunk = desc->FirstThunk;

					DWORD thunkRva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;

					DWORD thunkOffset = RvaToOffset(outFileInfo, thunkRva);

					DWORD iatOffset = RvaToOffset(outFileInfo, desc->FirstThunk);

					if (!thunkOffset || !iatOffset)
						continue;

					auto* thunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(outFileInfo.data.data() + thunkOffset);
					auto* iat = reinterpret_cast<IMAGE_THUNK_DATA64*>(outFileInfo.data.data() + iatOffset);

					for (size_t i = 0; thunk[i].u1.AddressOfData != 0; ++i)
					{
						ImportFunction function;

						function.thunkRva = thunkRva + static_cast<uint32_t>(i * sizeof(IMAGE_THUNK_DATA64));
						function.iatRva = desc->FirstThunk + static_cast<uint32_t>(i * sizeof(IMAGE_THUNK_DATA64));

						if (IMAGE_SNAP_BY_ORDINAL64(thunk[i].u1.Ordinal))
						{
							function.importedByOrdinal = true;
							function.ordinal = static_cast<uint16_t>(IMAGE_ORDINAL64(thunk[i].u1.Ordinal));
						}
						else
						{
							DWORD importByNameRva = static_cast<DWORD>(thunk[i].u1.AddressOfData);
							DWORD importByNameOffset = RvaToOffset(outFileInfo, importByNameRva);

							if (!importByNameOffset)
								continue;

							auto* importByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(outFileInfo.data.data() + importByNameOffset);

							function.hint = importByName->Hint;
							function.name = reinterpret_cast<const char*>(importByName->Name);
						}

						module.functions.push_back(std::move(function));
						outFileInfo.importsInfo.totalImports++;
					}

					outFileInfo.importsInfo.modules.push_back(std::move(module));
					outFileInfo.importsInfo.totalModules++;
				}

				DEBUG_PRINT("Total Imported Modules: %u\n", outFileInfo.importsInfo.totalModules);
				DEBUG_PRINT("Total Imported Functions: %u\n", outFileInfo.importsInfo.totalImports);
			}
		}

		return true;
	}
}