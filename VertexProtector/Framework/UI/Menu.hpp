#pragma once

#include <Framework/UI/Widgets.hpp>
#include <Framework/UI/FontManager.hpp>
#include <Framework/UI/Notifications.hpp>

#include <imgui.h>

#include <Windows.h>

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
			ImGui::Begin("VertexProtector", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
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
					if (!AnalisysThreadRunning)
					{
						AnalisysThreadRunning = true;
						std::thread([this]() {
							if (!Protector::FileInfo::LoadFile(DroppedFilePath, AnalyzedFileInfo))
							{
							}
							
						}).detach();
					}

					ImGui::SetCursorPosY(WindowSize.y - 350);
					Widgets::TextCentered(FontManager::Get()[FontID::BIG], ICON_FA_FILE);
					ImGui::SetCursorPosY(WindowSize.y - 250);
					Widgets::TextCentered(FontManager::Get()[FontID::Regular], "Analyzing: " + DroppedFileName);

					static float progress = 0.0f;
					progress += ImGui::GetIO().DeltaTime * 0.1f; // Simulate progress over time
					if (progress >= 1.0f)
					{
						progress = 1.0f;
						NotifySuccess("Analysis completed for: " + DroppedFileName);
						RenderingPage = 2;
					}
					
					drawList->AddRectFilled(WindowPos + ImVec2(0, WindowSize.y - 20), WindowPos + ImVec2(WindowSize.x * progress, WindowSize.y), IM_COL32(0, 120, 255, 255));
					int percentage = static_cast<int>(progress * 100.0f);

					drawList->AddText(FontManager::Get()[FontID::Regular], FontManager::Get()[FontID::Regular]->LegacySize, WindowPos + ImVec2(10, WindowSize.y - 20), IM_COL32(255, 255, 255, 255), (std::to_string(percentage) + "%").c_str());
				}
			}
			ImGui::End();
		}

	private:
		int RenderingPage = 0;
		std::string DroppedFilePath;
		std::string DroppedFileName;

		Protector::FileInfo::FileInfo_t AnalyzedFileInfo;

		bool AnalisysThreadRunning = false;
	};
}
