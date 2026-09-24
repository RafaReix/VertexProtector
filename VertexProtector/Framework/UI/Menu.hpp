#pragma once

#include <Framework/UI/Widgets.hpp>
#include <Framework/UI/FontManager.hpp>
#include <Framework/UI/Notifications.hpp>

#include <imgui.h>

#include <Windows.h>

#include <cstdio>
#include <string>
#include <vector>
#include <thread>

#include <Protector/FileInfo.hpp>

namespace Framework::UI
{
	class Menu
	{
	public:
		void Render()
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
			ImGui::Begin("VertexProtector", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
			{
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				ImVec2 WindowSize = ImGui::GetWindowSize();
				ImVec2 WindowPos = ImGui::GetWindowPos();

				if (RenderingPage == 0)
				{
					ImGui::SetCursorPosY(WindowSize.y - 350);
					Widgets::TextCentered(FontManager::Get()[FontID::BIG], ICON_FA_CLOUD_ARROW_UP);

					ImGui::SetCursorPosY(WindowSize.y - 250);
					Widgets::TextCentered(FontManager::Get()[FontID::Regular], "Drop the binary you want to protect.");

					if (ImGui::GetIO().droppedFiles.size() > 0)
					{
						if (ImGui::GetIO().droppedFiles.size() == 1)
						{
							const auto& dropped = ImGui::GetIO().droppedFiles[0];
							const std::string fullPath(dropped.begin(), dropped.end());
							const std::string fileName = fullPath.substr(fullPath.find_last_of("/\\") + 1);

							NotifyInfo("File dropped: " + fileName + " | Analysis started.");

							DEBUG_PRINT("File dropped: %s | Analysis started.\n", fileName.c_str());

							DroppedFilePath = fullPath;
							DroppedFileName = fileName;
							RenderingPage = 1;
						}
						else
						{
							NotifyError("Please drop only one file at a time.");
						}

						ImGui::GetIO().droppedFiles.clear();
					}
				}
				else if (RenderingPage == 1)
				{
					static float progress = 0.0f;

					if (!AnalisysThreadRunning)
					{
						AnalisysThreadRunning = true;
						std::thread([this]() {
							if (!Protector::FileInfo::LoadFile(DroppedFilePath, AnalyzedFileInfo))
							{
								RenderingPage = 0;
								AnalisysThreadRunning = false;
							}

							progress = 1.f;
							
						}).detach();
					}

					ImGui::SetCursorPosY(WindowSize.y - 350);
					Widgets::TextCentered(FontManager::Get()[FontID::BIG], ICON_FA_FILE);
					ImGui::SetCursorPosY(WindowSize.y - 250);
					Widgets::TextCentered(FontManager::Get()[FontID::Regular], "Analyzing: " + DroppedFileName);

					progress += ImGui::GetIO().DeltaTime * 0.1f; // Simulate progress over time
					if (progress >= 1.0f)
					{
						progress = 1.0f;
						NotifySuccess("Analysis completed for: " + DroppedFileName);
						RenderingPage = 2;
					}
					
					drawList->AddRectFilled(WindowPos + ImVec2(0, WindowSize.y - 20), WindowPos + ImVec2(WindowSize.x * progress, WindowSize.y), ImColor(0, 120, 255, 255));
					int percentage = static_cast<int>(progress * 100.0f);

					drawList->AddText(FontManager::Get()[FontID::Regular], FontManager::Get()[FontID::Regular]->LegacySize, WindowPos + ImVec2(10, WindowSize.y - 20), ImColor(255, 255, 255, 255), (std::to_string(percentage) + "%").c_str());
				}
				else if (RenderingPage == 2)
				{
					drawList->AddRectFilled(WindowPos, WindowPos + ImVec2(200, WindowSize.y), ImColor(30, 30, 30, 255));
					drawList->AddText(FontManager::Get()[FontID::BIG], FontManager::Get()[FontID::BIG]->LegacySize, WindowPos + ImVec2(50, 20), ImColor(255, 255, 255, 255), ICON_FA_FILE);
					drawList->AddLine(WindowPos + ImVec2(0, 140), WindowPos + ImVec2(200, 140), ImColor(0, 120, 255, 255), 1.0f);

					ImGui::SetCursorPos(ImVec2(10, 150));
					ImGui::BeginGroup();
					{
						if (Widgets::Tab("Information", ProtectorTab == 0))
							ProtectorTab = 0;
						if (Widgets::Tab("Protects", ProtectorTab == 1))
							ProtectorTab = 1;
					}
					ImGui::EndGroup();

					ImGui::SetCursorPos(ImVec2(200, 0));
					ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 22.0f));
					ImGui::BeginChild("Content", ImVec2(WindowSize.x - 200, WindowSize.y), ImGuiChildFlags_AlwaysUseWindowPadding);
					{
						if (ProtectorTab == 0)
							RenderInformationTab();
					}
					ImGui::EndChild();
					ImGui::PopStyleVar();
				}
			}
			ImGui::End();
		}

	private:
		static std::string FormatHex(uint64_t value, int width)
		{
			char buffer[32]{};
			std::snprintf(buffer, sizeof(buffer), "0x%0*llX", width, static_cast<unsigned long long>(value));
			return buffer;
		}

		static std::string FormatSize(uint64_t bytes)
		{
			char buffer[64]{};
			if (bytes >= 1024ull * 1024ull)
				std::snprintf(buffer, sizeof(buffer), "%.2f MB (%llu bytes)", static_cast<double>(bytes) / (1024.0 * 1024.0), static_cast<unsigned long long>(bytes));
			else if (bytes >= 1024ull)
				std::snprintf(buffer, sizeof(buffer), "%.2f KB (%llu bytes)", static_cast<double>(bytes) / 1024.0, static_cast<unsigned long long>(bytes));
			else
				std::snprintf(buffer, sizeof(buffer), "%llu bytes", static_cast<unsigned long long>(bytes));
			return buffer;
		}

		static const char* SubsystemName(WORD subsystem)
		{
			switch (subsystem)
			{
			case IMAGE_SUBSYSTEM_NATIVE: return "Native";
			case IMAGE_SUBSYSTEM_WINDOWS_GUI: return "Windows GUI";
			case IMAGE_SUBSYSTEM_WINDOWS_CUI: return "Windows Console";
			case IMAGE_SUBSYSTEM_EFI_APPLICATION: return "EFI Application";
			case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER: return "EFI Boot Driver";
			case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER: return "EFI Runtime Driver";
			default: return "Unknown";
			}
		}

		size_t CountModuleReferences(const std::string& moduleName) const
		{
			size_t count = 0;
			for (const auto& reference : AnalyzedFileInfo.importsInfo.references)
				if (reference.moduleName == moduleName)
					++count;
			return count;
		}

		void RenderInformationTab()
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));
			Widgets::PageHeader("PE FILE ANALYSIS", AnalyzedFileInfo.name.c_str(), "Portable Executable overview and import analysis");
			ImGui::Spacing();

			if (!AnalyzedFileInfo.ntHeaders)
			{
				Widgets::EmptyState("The PE headers are not available.");
				ImGui::PopStyleVar();
				return;
			}

			if (Widgets::SubTab("Overview", InformationTab == 0))
				InformationTab = 0;
			ImGui::SameLine(0.0f, 6.0f);
			if (Widgets::SubTab("IAT References", InformationTab == 1))
				InformationTab = 1;

			ImGui::Spacing();
			if (InformationTab == 0)
				RenderOverviewTab();
			else
				RenderIATReferencesTab();

			ImGui::Dummy(ImVec2(0.0f, 16.0f));
			ImGui::PopStyleVar();
		}

		void RenderOverviewTab()
		{
			const auto& fileHeader = AnalyzedFileInfo.ntHeaders->FileHeader;
			const auto& optionalHeader = AnalyzedFileInfo.ntHeaders->OptionalHeader;
			const auto& imports = AnalyzedFileInfo.importsInfo;

			if (ImGui::BeginTable("##AnalysisSummary", 4, ImGuiTableFlags_SizingStretchSame))
			{
				ImGui::TableNextColumn(); Widgets::SummaryCard(std::to_string(fileHeader.NumberOfSections).c_str(), "Sections");
				ImGui::TableNextColumn(); Widgets::SummaryCard(std::to_string(imports.totalModules).c_str(), "Import modules");
				ImGui::TableNextColumn(); Widgets::SummaryCard(std::to_string(imports.totalImports).c_str(), "Imported functions");
				ImGui::TableNextColumn(); Widgets::SummaryCard(std::to_string(imports.references.size()).c_str(), "IAT references");
				ImGui::EndTable();
			}

			Widgets::SectionHeader("File");
			if (Widgets::BeginPropertyTable("##FileProperties"))
			{
				Widgets::PropertyRow("Name", AnalyzedFileInfo.name);
				Widgets::PropertyRow("Path", AnalyzedFileInfo.path);
				Widgets::PropertyRow("Extension", AnalyzedFileInfo.extension.empty() ? "None" : AnalyzedFileInfo.extension);
				Widgets::PropertyRow("Size", FormatSize(AnalyzedFileInfo.size));
				Widgets::PropertyRow("Format", "PE32+ (64-bit)");
				Widgets::PropertyRow("Machine", fileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 ? "AMD64 / x86-64" : FormatHex(fileHeader.Machine, 4));
				ImGui::EndTable();
			}

			Widgets::SectionHeader("PE headers");
			if (Widgets::BeginPropertyTable("##PeProperties"))
			{
				Widgets::PropertyRow("Entry point (RVA)", FormatHex(optionalHeader.AddressOfEntryPoint, 8));
				Widgets::PropertyRow("Entry point (VA)", FormatHex(optionalHeader.ImageBase + optionalHeader.AddressOfEntryPoint, 16));
				Widgets::PropertyRow("Image base", FormatHex(optionalHeader.ImageBase, 16));
				Widgets::PropertyRow("Image size", FormatSize(optionalHeader.SizeOfImage));
				Widgets::PropertyRow("Header size", FormatSize(optionalHeader.SizeOfHeaders));
				Widgets::PropertyRow("Section alignment", FormatHex(optionalHeader.SectionAlignment, 8));
				Widgets::PropertyRow("File alignment", FormatHex(optionalHeader.FileAlignment, 8));
				Widgets::PropertyRow("Subsystem", SubsystemName(optionalHeader.Subsystem));
				Widgets::PropertyRow("Linker version", std::to_string(optionalHeader.MajorLinkerVersion) + "." + std::to_string(optionalHeader.MinorLinkerVersion));
				Widgets::PropertyRow("Timestamp", FormatHex(fileHeader.TimeDateStamp, 8));
				Widgets::PropertyRow("Checksum", FormatHex(optionalHeader.CheckSum, 8));
				ImGui::EndTable();
			}

			Widgets::SectionHeader("Mitigations");
			if (Widgets::BeginPropertyTable("##Mitigations", 0.68f))
			{
				Widgets::StatusRow("ASLR (dynamic base)", (optionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) != 0);
				Widgets::StatusRow("High-entropy ASLR", (optionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA) != 0);
				Widgets::StatusRow("DEP / NX compatible", (optionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_NX_COMPAT) != 0);
				Widgets::StatusRow("Control Flow Guard", (optionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_GUARD_CF) != 0);
				Widgets::StatusRow("Terminal Server aware", (optionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE) != 0);
				ImGui::EndTable();
			}

			Widgets::SectionHeader("Imports");
			if (!imports.hasImports)
			{
				Widgets::EmptyState("No import directory was found in this image.");
				return;
			}

			if (Widgets::BeginPropertyTable("##ImportDirectory"))
			{
				Widgets::PropertyRow("Directory RVA", FormatHex(imports.directoryRva, 8));
				Widgets::PropertyRow("Directory size", FormatSize(imports.directorySize));
				Widgets::PropertyRow("Modules", std::to_string(imports.totalModules));
				Widgets::PropertyRow("Functions", std::to_string(imports.totalImports));
				Widgets::PropertyRow("CALL/JMP references", std::to_string(imports.references.size()));
				ImGui::EndTable();
			}

			RenderImportModules(true);
		}

		void RenderImportModules(bool includeFunctions)
		{
			const auto& imports = AnalyzedFileInfo.importsInfo;
			Widgets::SectionHeader(includeFunctions ? "Imported modules" : "References by module");

			bool displayedModule = false;
			for (size_t moduleIndex = 0; moduleIndex < imports.modules.size(); ++moduleIndex)
			{
				const auto& module = imports.modules[moduleIndex];
				const size_t referenceCount = CountModuleReferences(module.name);
				if (!includeFunctions && referenceCount == 0)
					continue;

				displayedModule = true;
				ImGui::PushID(static_cast<int>(moduleIndex));
				if (Widgets::ModuleNode(module.name.c_str(), module.functions.size(), referenceCount))
				{
					ImGui::TextDisabled("Original thunk: %s    First thunk / IAT: %s",
						FormatHex(module.originalFirstThunk, 8).c_str(), FormatHex(module.firstThunk, 8).c_str());

					if (includeFunctions)
					{
						Widgets::SectionHeader("Imported functions");
						RenderFunctionTable(module);
					}

					Widgets::SectionHeader("IAT references");
					if (referenceCount == 0)
						Widgets::EmptyState("No CALL or JMP instructions reference this module's IAT entries.");
					else
						RenderReferenceTable("##ModuleReferences", &module.name);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			if (!displayedModule)
				Widgets::EmptyState("No imported module has an IAT code reference.");
		}

		void RenderFunctionTable(const Protector::FileInfo::ImportModule& module)
		{
			if (!ImGui::BeginTable("##Functions", 4,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
				return;

			ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthStretch, 0.46f);
			ImGui::TableSetupColumn("Hint / ordinal", ImGuiTableColumnFlags_WidthStretch, 0.18f);
			ImGui::TableSetupColumn("Thunk RVA", ImGuiTableColumnFlags_WidthStretch, 0.18f);
			ImGui::TableSetupColumn("IAT RVA", ImGuiTableColumnFlags_WidthStretch, 0.18f);
			ImGui::TableHeadersRow();

			for (const auto& function : module.functions)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				if (function.importedByOrdinal)
					ImGui::Text("Ordinal #%u", function.ordinal);
				else
					ImGui::TextUnformatted(function.name.c_str());
				ImGui::TableSetColumnIndex(1);
				ImGui::Text(function.importedByOrdinal ? "#%u" : "%u", function.importedByOrdinal ? function.ordinal : function.hint);
				ImGui::TableSetColumnIndex(2); ImGui::Text("%s", FormatHex(function.thunkRva, 8).c_str());
				ImGui::TableSetColumnIndex(3); ImGui::Text("%s", FormatHex(function.iatRva, 8).c_str());
			}
			ImGui::EndTable();
		}

		void RenderIATReferencesTab()
		{
			const auto& references = AnalyzedFileInfo.importsInfo.references;
			size_t calls = 0;
			size_t referencedModules = 0;
			for (const auto& reference : references)
				if (reference.isCall)
					++calls;
			for (const auto& module : AnalyzedFileInfo.importsInfo.modules)
				if (CountModuleReferences(module.name) != 0)
					++referencedModules;

			if (ImGui::BeginTable("##ReferenceSummary", 4, ImGuiTableFlags_SizingStretchSame))
			{
				ImGui::TableNextColumn(); Widgets::SummaryCard(std::to_string(references.size()).c_str(), "Total references");
				ImGui::TableNextColumn(); Widgets::SummaryCard(std::to_string(calls).c_str(), "CALL instructions");
				ImGui::TableNextColumn(); Widgets::SummaryCard(std::to_string(references.size() - calls).c_str(), "JMP instructions");
				ImGui::TableNextColumn(); Widgets::SummaryCard(std::to_string(referencedModules).c_str(), "Referenced modules");
				ImGui::EndTable();
			}

			if (references.empty())
			{
				Widgets::SectionHeader("IAT references");
				Widgets::EmptyState("No CALL or JMP instructions targeting imported functions were detected.");
				return;
			}

			RenderImportModules(false);
		}

		void RenderReferenceTable(const char* id, const std::string* moduleName)
		{
			if (!ImGui::BeginTable(id, 4,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
				return;

			ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch, 0.20f);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.12f);
			ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 0.48f);
			ImGui::TableSetupColumn("IAT RVA", ImGuiTableColumnFlags_WidthStretch, 0.20f);
			ImGui::TableHeadersRow();

			for (const auto& reference : AnalyzedFileInfo.importsInfo.references)
			{
				if (moduleName && reference.moduleName != *moduleName)
					continue;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::Text("%s", FormatHex(reference.instructionRva, 8).c_str());
				ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(reference.isCall ? "CALL" : "JMP");
				ImGui::TableSetColumnIndex(2);
				if (reference.importedByOrdinal)
					ImGui::Text("%s!#%u", reference.moduleName.c_str(), reference.ordinal);
				else
					ImGui::Text("%s!%s", reference.moduleName.c_str(), reference.functionName.c_str());
				ImGui::TableSetColumnIndex(3); ImGui::Text("%s", FormatHex(reference.iatRva, 8).c_str());
			}
			ImGui::EndTable();
		}

		int RenderingPage = 0;
		std::string DroppedFilePath;
		std::string DroppedFileName;

		Protector::FileInfo::FileInfo_t AnalyzedFileInfo;

		bool AnalisysThreadRunning = false;

		int ProtectorTab = 0;
		int InformationTab = 0;
	};
}
