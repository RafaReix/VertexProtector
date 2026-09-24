#pragma once

#include <imgui.h>
#include <ImGui/imgui_internal.h>

#include "FontManager.hpp"

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

	inline bool Tab(const char* TabName, bool TabSelected)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID thisId = window->GetID(TabName);

		const ImVec2 labelSize = FontManager::Get()[FontID::Title]->CalcTextSizeA(FontManager::Get()[FontID::Title]->LegacySize, FLT_MAX, NULL, TabName);

		const ImVec2 cursorPos = window->DC.CursorPos;
		const ImVec2 size = ImVec2(180, labelSize.y + 15);
		const ImRect totalRect(cursorPos, cursorPos + size);
		ImGui::ItemSize(totalRect, style.FramePadding.y);
		if (!ImGui::ItemAdd(totalRect, thisId))
			return false;

		bool Hovered = totalRect.Contains(g.IO.MousePos);
		bool Pressed = Hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !TabSelected;

		ImColor NormalColor = ImColor(0, 0, 0, 0);         // Transparent
		ImColor HoveredColor = ImColor(0, 120, 255, 80);    // Transparent blue
		ImColor SelectedColor = ImColor(0, 120, 255, 255);   // Solid blue

		window->DrawList->AddRectFilled(totalRect.Min, totalRect.Max, TabSelected ? SelectedColor : Hovered ? HoveredColor : NormalColor, 5.0f);
		window->DrawList->AddText(FontManager::Get()[FontID::Title], FontManager::Get()[FontID::Title]->LegacySize, totalRect.Min + ImVec2((size.x - labelSize.x) * 0.5f, (size.y - labelSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), TabName);

		return Pressed;
	}

	inline void PageHeader(const char* eyebrow, const char* title, const char* description)
	{
		ImGui::TextColored(ImVec4(0.0f, 0.47f, 1.0f, 1.0f), "%s", eyebrow);
		ImGui::PushFont(FontManager::Get()[FontID::Title]);
		ImGui::TextUnformatted(title);
		ImGui::PopFont();
		ImGui::TextDisabled("%s", description);
	}

	inline bool SubTab(const char* label, bool selected, const ImVec2& size = ImVec2(150.0f, 34.0f))
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return false;

		const ImGuiID id = window->GetID(label);
		const ImVec2 position = window->DC.CursorPos;
		const ImRect bounds(position, position + size);
		ImGui::ItemSize(bounds);
		if (!ImGui::ItemAdd(bounds, id))
			return false;

		bool hovered = false;
		bool held = false;
		const bool pressed = ImGui::ButtonBehavior(bounds, id, &hovered, &held);
		const ImU32 background = ImGui::GetColorU32(selected
			? ImVec4(0.0f, 0.47f, 1.0f, 0.24f)
			: hovered ? ImVec4(1.0f, 1.0f, 1.0f, 0.07f) : ImVec4(1.0f, 1.0f, 1.0f, 0.03f));

		window->DrawList->AddRectFilled(bounds.Min, bounds.Max, background, 5.0f);
		if (selected)
			window->DrawList->AddRectFilled(ImVec2(bounds.Min.x, bounds.Max.y - 2.0f), bounds.Max, ImColor(0, 120, 255, 255), 2.0f);

		const ImVec2 textSize = ImGui::CalcTextSize(label);
		const ImU32 textColor = selected ? IM_COL32(255, 255, 255, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
		window->DrawList->AddText(bounds.GetCenter() - textSize * 0.5f, textColor, label);
		return pressed;
	}

	inline void SectionHeader(const char* label)
	{
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.0f, 0.47f, 1.0f, 0.55f));
		ImGui::SeparatorText(label);
		ImGui::PopStyleColor();
	}

	inline void SummaryCard(const char* value, const char* label)
	{
		const ImVec2 position = ImGui::GetCursorScreenPos();
		const ImVec2 size(ImGui::GetContentRegionAvail().x, 58.0f);
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(position, position + size, ImColor(255, 255, 255, 9), 5.0f);
		drawList->AddRect(position, position + size, ImColor(255, 255, 255, 24), 5.0f);
		drawList->AddText(position + ImVec2(12.0f, 8.0f), ImColor(0, 140, 255, 255), value);
		drawList->AddText(position + ImVec2(12.0f, 31.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), label);
		ImGui::Dummy(size);
	}

	inline bool BeginPropertyTable(const char* id, float labelWidth = 0.32f)
	{
		constexpr ImGuiTableFlags flags =
			ImGuiTableFlags_BordersOuter |
			ImGuiTableFlags_BordersInnerH |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_SizingStretchProp;

		if (!ImGui::BeginTable(id, 2, flags))
			return false;

		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, labelWidth);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f - labelWidth);
		return true;
	}

	inline void PropertyRow(const char* label, const std::string& value)
	{
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeight() + 10.0f);
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", label);
		ImGui::TableSetColumnIndex(1);
		ImGui::AlignTextToFramePadding();
		ImGui::TextWrapped("%s", value.c_str());
	}

	inline void StatusRow(const char* label, bool enabled)
	{
		ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeight() + 10.0f);
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(enabled ? ImVec4(0.25f, 0.85f, 0.45f, 1.0f) : ImVec4(0.95f, 0.45f, 0.35f, 1.0f),
			enabled ? "Enabled" : "Disabled");
	}

	inline bool ModuleNode(const char* name, size_t functionCount, size_t referenceCount)
	{
		return ImGui::TreeNodeEx("##Module", ImGuiTreeNodeFlags_SpanAvailWidth,
			"%s  (%zu functions, %zu references)", name, functionCount, referenceCount);
	}

	inline void EmptyState(const char* message)
	{
		const ImVec2 position = ImGui::GetCursorScreenPos();
		const ImVec2 size(ImGui::GetContentRegionAvail().x, 48.0f);
		ImGui::GetWindowDrawList()->AddRectFilled(position, position + size, ImColor(255, 255, 255, 7), 5.0f);
		ImGui::SetCursorScreenPos(position + ImVec2(12.0f, 15.0f));
		ImGui::TextDisabled("%s", message);
		ImGui::SetCursorScreenPos(position + ImVec2(0.0f, size.y));
		ImGui::Dummy(ImVec2(0.0f, 0.0f));
	}
}
