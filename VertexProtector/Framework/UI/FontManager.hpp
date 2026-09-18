#pragma once

#include <Framework/UI/Fonts.hpp>
#include <imgui.h>
#include <unordered_map>

namespace Framework::UI
{
	enum class FontID
	{
		Regular,   // Asap Medium, 16px  (also the ImGui default, since it's added first)
		Bold,      // Asap Bold,   16px
		Title,     // Asap Bold,   26px
		BIG,     // Asap Bold,   100px
	};

	class FontManager
	{
	public:
		static FontManager& Get()
		{
			static FontManager instance;
			return instance;
		}

		void Load()
		{
			if (m_loaded)
				return;

			m_fonts[FontID::Regular] = AddFace(AsapMedium_compressed_data_base85, 16.0f);
			m_fonts[FontID::Bold] = AddFace(AsapBold_compressed_data_base85, 16.0f);
			m_fonts[FontID::Title] = AddFace(AsapBold_compressed_data_base85, 26.0f);
			m_fonts[FontID::BIG] = AddFace(AsapBold_compressed_data_base85, 100.0f);

			m_loaded = true;
		}

		ImFont* operator[](FontID id) const
		{
			const auto it = m_fonts.find(id);
			return it != m_fonts.end() ? it->second : ImGui::GetIO().FontDefault;
		}

	private:
		FontManager() = default;

		ImFont* AddFace(const char* asapBase85, float sizePx)
		{
			ImFontAtlas* atlas = ImGui::GetIO().Fonts;

			ImFont* face = atlas->AddFontFromMemoryCompressedBase85TTF(asapBase85, sizePx);

			static const ImWchar kIconRange[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

			ImFontConfig cfg;
			cfg.MergeMode = true;
			cfg.PixelSnapH = true;
			cfg.GlyphMinAdvanceX = sizePx;
			cfg.GlyphOffset = ImVec2(0.0f, 1.0f);

			atlas->AddFontFromMemoryCompressedTTF(FontAwesome6Solid_compressed_data, FontAwesome6Solid_compressed_size, sizePx, &cfg, kIconRange);

			return face;
		}

		bool m_loaded = false;
		std::unordered_map<FontID, ImFont*> m_fonts;
	};
}