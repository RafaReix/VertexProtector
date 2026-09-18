#pragma once

#include <imgui.h>

#include <string>
#include <vector>

namespace Framework::UI::Widgets
{
	inline void TextCentered(ImFont* font, const std::string& text, const ImColor& color = ImColor(1.f, 1.f, 1.f, 1.f))
	{
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImVec2 textSize = font->CalcTextSizeA(font->LegacySize, FLT_MAX, 0.0f, text.c_str());
		ImVec2 windowPos = ImGui::GetWindowPos();

		float textX = (windowSize.x - textSize.x) * 0.5f;

		ImGui::GetWindowDrawList()->AddText(font, font->LegacySize, windowPos + ImVec2(textX, ImGui::GetCursorPosY()), color, text.c_str());
		ImGui::Dummy(textSize);
	}
}
