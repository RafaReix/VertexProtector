#pragma once

#include <Framework/Framework.hpp>

#include <Windows.h>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <Zydis/Zydis.h>

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

	struct IATReference
	{
		uint32_t instructionRva;
		uint32_t iatRva;
		bool isCall;

		std::string moduleName;
		std::string functionName;

		bool importedByOrdinal = false;
		uint16_t ordinal = 0;
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

			uint32_t totalImports = 0;
			uint32_t totalModules = 0;

			uint32_t directoryRva = 0;
			uint32_t directorySize = 0;

			std::vector<ImportModule> modules;
			std::vector<IATReference> references;
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

							DEBUG_PRINT("Import: %s!#%u | Thunk RVA: %08llX | IAT RVA: %08llX\n", module.name.c_str(), function.ordinal, function.thunkRva, function.iatRva);
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

							DEBUG_PRINT("Import: %s!%s | Hint: %u | Thunk RVA: %08llX | IAT RVA: %08llX\n", module.name.c_str(), function.name.c_str(), function.hint, function.thunkRva, function.iatRva);
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

		struct ImportLookup
		{
			const ImportModule* module;
			const ImportFunction* function;
		};

		std::unordered_map<uint32_t, ImportLookup> iatLookup;

		for (const auto& module : outFileInfo.importsInfo.modules)
		{
			for (const auto& function : module.functions)
			{
				iatLookup.emplace(static_cast<uint32_t>(function.iatRva), ImportLookup{ &module, &function });
			}
		}

		ZydisDecoder decoder;
		ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

		auto section = IMAGE_FIRST_SECTION(outFileInfo.ntHeaders);

		// Code Scan
		for (uint16_t i = 0; i < outFileInfo.ntHeaders->FileHeader.NumberOfSections; ++i, ++section)
		{
			if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE))
				continue;

			uint32_t rawStart = section->PointerToRawData;
			uint32_t rawSize = section->SizeOfRawData;

			if (rawStart >= outFileInfo.data.size())
				continue;

			if (rawStart + rawSize > outFileInfo.data.size())
				rawSize = static_cast<uint32_t>(outFileInfo.data.size() - rawStart);

			uint32_t offset = 0;

			while (offset < rawSize)
			{
				const uint8_t* instructionData = outFileInfo.data.data() + rawStart + offset;
				const size_t bytesRemaining = rawSize - offset;

				ZydisDecodedInstruction instruction;
				ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

				ZyanStatus status = ZydisDecoderDecodeFull(&decoder, instructionData, bytesRemaining, &instruction, operands);

				if (!ZYAN_SUCCESS(status))
				{
					++offset;
					continue;
				}

				const uint32_t instructionRva = section->VirtualAddress + offset;

				if (instruction.mnemonic == ZYDIS_MNEMONIC_CALL || instruction.mnemonic == ZYDIS_MNEMONIC_JMP)
				{
					for (uint8_t opIndex = 0; opIndex < instruction.operand_count_visible; ++opIndex)
					{
						const auto& op = operands[opIndex];

						if (op.type != ZYDIS_OPERAND_TYPE_MEMORY)
							continue;

						if (op.mem.base != ZYDIS_REGISTER_RIP)
							continue;

						const int64_t displacement = op.mem.disp.value;

						const int64_t targetRva = static_cast<int64_t>(instructionRva) + instruction.length + displacement;

						if (targetRva >= 0 && targetRva <= UINT32_MAX)
						{
							auto it = iatLookup.find(static_cast<uint32_t>(targetRva));
							if (it != iatLookup.end())
							{
								const auto* module = it->second.module;
								const auto* function = it->second.function;

								IATReference reference;

								reference.instructionRva = instructionRva;
								reference.iatRva = static_cast<uint32_t>(targetRva);

								reference.isCall = instruction.mnemonic == ZYDIS_MNEMONIC_CALL;

								reference.moduleName = module->name;

								reference.importedByOrdinal = function->importedByOrdinal;

								reference.ordinal = function->ordinal;

								if (!function->importedByOrdinal)
									reference.functionName = function->name;

								if (reference.importedByOrdinal)
								{
									DEBUG_PRINT("%s RVA %08X -> %s!#%u [IAT %08X]\n", reference.isCall ? "CALL" : "JMP", reference.instructionRva, reference.moduleName.c_str(), reference.ordinal, reference.iatRva);
								}
								else
								{
									DEBUG_PRINT("%s RVA %08X -> %s!%s [IAT %08X]\n", reference.isCall ? "CALL" : "JMP", reference.instructionRva, reference.moduleName.c_str(), reference.functionName.c_str(), reference.iatRva);
								}

								outFileInfo.importsInfo.references.push_back(std::move(reference));
							}
						}
					}
				}

				offset += instruction.length;
			}
		}

		return true;
	}
}