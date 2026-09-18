#pragma once

// Toast notifications — rewrite of imgui-notify (patrickcjk) for VertexProtector.
//
// Changes from the original:
//   * No bundled Font Awesome 5 headers / no AddFontFromMemoryTTF — your FontManager
//     already merges FA6 into every face, so the glyphs are in the atlas already.
//   * Fonts are injected once (SetFonts) instead of #including FontManager here, so
//     this header stays decoupled and doesn't drag the font blobs into another TU.
//   * Type-colored accent bar, rounded card, bold title + body in your Asap faces,
//     slide-in/out with the fade, and a draining progress bar.
//
// Setup (once, right after FontManager::Get().Load()):
//     Framework::UI::Notifications::Get().SetFonts(
//         FontManager::Get()[FontID::Bold],       // title
//         FontManager::Get()[FontID::Regular]);   // body
//
// Fire a toast from anywhere:
//     Framework::UI::NotifySuccess("Dumped 3 sections");
//     Framework::UI::NotifyError("Injection failed", "Loader");   // content, title
//     Framework::UI::Notifications::Get().AddFormat(ToastType::Info, "%d imports resolved", n);
//
// Draw them at the very end of your frame, after the rest of the UI:
//     Framework::UI::Notifications::Get().Render();
// 
// By Claude Opus 4.8

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace Framework::UI
{
	inline constexpr float           NOTIFY_PADDING_X = 20.0f;   // gap from the right screen edge
	inline constexpr float           NOTIFY_PADDING_Y = 20.0f;   // gap from the bottom screen edge
	inline constexpr float           NOTIFY_STACK_GAP = 10.0f;   // vertical gap between toasts
	inline constexpr float           NOTIFY_FADE_TIME = 150.0f;  // fade in / out duration (ms)
	inline constexpr float           NOTIFY_SLIDE = 45.0f;   // horizontal slide distance (px)
	inline constexpr float           NOTIFY_ROUNDING = 8.0f;
	inline constexpr float           NOTIFY_ACCENT_W = 4.0f;    // left accent bar width
	inline constexpr int             NOTIFY_DEFAULT_DISMISS = 3000;    // visible time excluding fades (ms)
	inline constexpr float           NOTIFY_OPACITY = 1.0f;
	inline constexpr ImGuiWindowFlags NOTIFY_TOAST_FLAGS = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings;

	enum class ToastType
	{
		None,
		Success,
		Warning,
		Error,
		Info,
	};

	inline uint64_t NotifyNowMs()
	{
		using namespace std::chrono;
		return static_cast<uint64_t>(
			duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
	}

	inline float NotifyClamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
	inline float NotifySmooth(float t) { return t * t * (3.0f - 2.0f * t); } // smoothstep ease

	inline const char* NotifyIcon(ToastType t)
	{
		switch (t)
		{
		case ToastType::Success: return "\xef\x81\x98"; // U+f058 circle-check
		case ToastType::Warning: return "\xef\x81\xb1"; // U+f071 triangle-exclamation
		case ToastType::Error:   return "\xef\x81\x97"; // U+f057 circle-xmark
		case ToastType::Info:    return "\xef\x81\x9a"; // U+f05a circle-info
		default:                 return nullptr;
		}
	}

	inline const char* NotifyDefaultTitle(ToastType t)
	{
		switch (t)
		{
		case ToastType::Success: return "Success";
		case ToastType::Warning: return "Warning";
		case ToastType::Error:   return "Error";
		case ToastType::Info:    return "Info";
		default:                 return nullptr;
		}
	}

	inline ImVec4 NotifyAccent(ToastType t)
	{
		switch (t)
		{
		case ToastType::Success: return ImVec4(0.16f, 0.78f, 0.45f, 1.0f);
		case ToastType::Warning: return ImVec4(0.98f, 0.73f, 0.20f, 1.0f);
		case ToastType::Error:   return ImVec4(0.94f, 0.32f, 0.31f, 1.0f);
		case ToastType::Info:    return ImVec4(0.30f, 0.62f, 0.95f, 1.0f);
		default:                 return ImVec4(0.62f, 0.64f, 0.70f, 1.0f);
		}
	}

	inline std::string NotifyFormatV(const char* fmt, va_list args)
	{
		if (!fmt)
			return {};

		va_list copy;
		va_copy(copy, args);
		const int len = std::vsnprintf(nullptr, 0, fmt, copy);
		va_end(copy);

		if (len <= 0)
			return {};

		std::string out(static_cast<size_t>(len), '\0');
		std::vsnprintf(out.data(), static_cast<size_t>(len) + 1, fmt, args);
		return out;
	}

	struct Toast
	{
		ToastType   type = ToastType::None;
		std::string title;
		std::string content;
		int         duration = NOTIFY_DEFAULT_DISMISS;   // visible ms, excluding fades
		uint64_t    created = NotifyNowMs();

		uint64_t Elapsed() const { return NotifyNowMs() - created; }

		bool Expired() const
		{
			return static_cast<float>(Elapsed()) > NOTIFY_FADE_TIME * 2.0f + static_cast<float>(duration);
		}

		float Opacity() const
		{
			const float e = static_cast<float>(Elapsed());
			if (e < NOTIFY_FADE_TIME)                                   // fading in
				return (e / NOTIFY_FADE_TIME) * NOTIFY_OPACITY;
			if (e > NOTIFY_FADE_TIME + static_cast<float>(duration))    // fading out
				return NotifyClamp(1.0f - (e - NOTIFY_FADE_TIME - duration) / NOTIFY_FADE_TIME, 0.0f, 1.0f) * NOTIFY_OPACITY;
			return NOTIFY_OPACITY;                                      // holding
		}

		float Slide() const
		{
			const float e = static_cast<float>(Elapsed());
			if (e < NOTIFY_FADE_TIME)
				return NotifySmooth(1.0f - e / NOTIFY_FADE_TIME) * NOTIFY_SLIDE;
			if (e > NOTIFY_FADE_TIME + static_cast<float>(duration))
				return NotifySmooth(NotifyClamp((e - NOTIFY_FADE_TIME - duration) / NOTIFY_FADE_TIME, 0.0f, 1.0f)) * NOTIFY_SLIDE;
			return 0.0f;
		}

		float Progress() const
		{
			const float active = static_cast<float>(Elapsed()) - NOTIFY_FADE_TIME;
			return NotifyClamp(1.0f - active / static_cast<float>(duration), 0.0f, 1.0f);
		}
	};

	class Notifications
	{
	public:
		static Notifications& Get()
		{
			static Notifications instance;
			return instance;
		}

		void SetFonts(ImFont* title, ImFont* body)
		{
			m_titleFont = title;
			m_bodyFont = body;
		}

		void Add(ToastType type, std::string title, std::string content = {}, int durationMs = NOTIFY_DEFAULT_DISMISS)
		{
			Toast t;
			t.type = type;
			t.title = std::move(title);
			t.content = std::move(content);
			t.duration = durationMs;
			t.created = NotifyNowMs();
			m_toasts.push_back(std::move(t));
		}

		void AddFormat(ToastType type, const char* fmt, ...)
		{
			va_list args;
			va_start(args, fmt);
			std::string content = NotifyFormatV(fmt, args);
			va_end(args);
			Add(type, {}, std::move(content));
		}

		void Clear() { m_toasts.clear(); }

		void Render()
		{
			if (m_toasts.empty())
				return;

			m_toasts.erase(std::remove_if(m_toasts.begin(), m_toasts.end(), [](const Toast& t) { return t.Expired(); }), m_toasts.end());

			ImFont* titleFont = m_titleFont ? m_titleFont : ImGui::GetFont();
			ImFont* bodyFont = m_bodyFont ? m_bodyFont : ImGui::GetFont();
			const float titleSize = titleFont->LegacySize;
			const float bodySize = bodyFont->LegacySize;

			const ImGuiViewport* vp = ImGui::GetMainViewport();
			const ImVec2 vpPos = vp->Pos;
			const ImVec2 vpSize = vp->Size;

			ImDrawList* dl = ImGui::GetForegroundDrawList();

			const float padL = NOTIFY_ACCENT_W + 12.0f;
			const float padR = 14.0f;
			const float padT = 12.0f;
			const float padB = 12.0f;
			const float iconGap = 8.0f;
			const float dividerGap = 8.0f;
			const float progH = 3.0f;
			const float progReserve = 10.0f;   // space between content and the progress bar
			const float maxContentW = NotifyClamp(vpSize.x * 0.28f, 220.0f, 380.0f);

			float bottomY = vpPos.y + vpSize.y - NOTIFY_PADDING_Y; // stack grows upward

			for (const Toast& toast : m_toasts)
			{
				const float  opacity = toast.Opacity();
				const float  slide = toast.Slide();
				const ImVec4 accent = NotifyAccent(toast.type);
				const auto   fade = [opacity](ImVec4 c) { c.w *= opacity; return c; };

				const char* icon = NotifyIcon(toast.type);
				const char* title = !toast.title.empty() ? toast.title.c_str() : NotifyDefaultTitle(toast.type);
				const bool hasTitle = title && title[0];
				const bool hasBody = !toast.content.empty();
				const bool hasHeader = icon || hasTitle;

				const float iconW = icon ? titleFont->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, icon).x : 0.0f;
				const float titleW = hasTitle ? titleFont->CalcTextSizeA(titleSize, FLT_MAX, 0.0f, title).x : 0.0f;
				const float headerW = iconW + (icon && hasTitle ? iconGap : 0.0f) + titleW;

				ImVec2 bodySz(0.0f, 0.0f);
				if (hasBody)
					bodySz = bodyFont->CalcTextSizeA(bodySize, FLT_MAX, maxContentW, toast.content.c_str());

				float innerW = (headerW > bodySz.x) ? headerW : bodySz.x;
				if (innerW > maxContentW) innerW = maxContentW;
				if (innerW < 140.0f)      innerW = 140.0f;

				float innerH = 0.0f;
				if (hasHeader) innerH += titleSize;
				if (hasBody)
				{
					if (hasHeader) innerH += dividerGap;
					innerH += bodySz.y;
				}
				innerH += progReserve + progH;

				const float cardW = padL + innerW + padR;
				const float cardH = padT + innerH + padB;

				const float  right = vpPos.x + vpSize.x - NOTIFY_PADDING_X + slide;
				const ImVec2 cmin(right - cardW, bottomY - cardH);
				const ImVec2 cmax(right, bottomY);

				const ImU32 bgU = ImGui::GetColorU32(fade(ImVec4(0.10f, 0.10f, 0.12f, 1.0f)));
				const ImU32 borderU = ImGui::GetColorU32(fade(ImVec4(1.0f, 1.0f, 1.0f, 0.08f)));
				const ImU32 accentU = ImGui::GetColorU32(fade(accent));
				const ImU32 titleU = ImGui::GetColorU32(fade(ImVec4(0.96f, 0.96f, 0.98f, 1.0f)));
				const ImU32 bodyU = ImGui::GetColorU32(fade(ImVec4(0.76f, 0.77f, 0.82f, 1.0f)));
				const ImU32 dividerU = ImGui::GetColorU32(fade(ImVec4(1.0f, 1.0f, 1.0f, 0.08f)));
				const ImU32 trackU = ImGui::GetColorU32(fade(ImVec4(1.0f, 1.0f, 1.0f, 0.10f)));

				dl->AddRectFilled(cmin, cmax, bgU, NOTIFY_ROUNDING);
				dl->AddRect(cmin, cmax, borderU, NOTIFY_ROUNDING, 0, 1.0f);
				dl->AddRectFilled(cmin, ImVec2(cmin.x + NOTIFY_ACCENT_W, cmax.y), accentU, NOTIFY_ROUNDING, ImDrawFlags_RoundCornersLeft);

				dl->PushClipRect(cmin, cmax, true);

				const float x0 = cmin.x + padL;
				float y = cmin.y + padT;

				if (hasHeader)
				{
					float x = x0;
					if (icon)
					{
						dl->AddText(titleFont, titleSize, ImVec2(x, y), accentU, icon);
						x += iconW + (hasTitle ? iconGap : 0.0f);
					}
					if (hasTitle)
						dl->AddText(titleFont, titleSize, ImVec2(x, y), titleU, title);
					y += titleSize;
				}

				if (hasBody)
				{
					if (hasHeader)
					{
						y += dividerGap * 0.5f;
						dl->AddLine(ImVec2(x0, y), ImVec2(cmax.x - padR, y), dividerU);
						y += dividerGap * 0.5f;
					}
					dl->AddText(bodyFont, bodySize, ImVec2(x0, y), bodyU,
						toast.content.c_str(), nullptr, maxContentW);
					y += bodySz.y;
				}

				const float py = cmax.y - padB - progH;
				const float px0 = x0;
				const float px1 = cmax.x - padR;
				if (px1 > px0)
				{
					dl->AddRectFilled(ImVec2(px0, py), ImVec2(px1, py + progH), trackU, progH * 0.5f);
					const float fillX = px0 + (px1 - px0) * toast.Progress();
					dl->AddRectFilled(ImVec2(px0, py), ImVec2(fillX, py + progH), accentU, progH * 0.5f);
				}

				dl->PopClipRect();

				bottomY = cmin.y - NOTIFY_STACK_GAP; // next toast stacks above this one
			}
		}

	private:
		Notifications() = default;

		std::vector<Toast> m_toasts;
		ImFont* m_titleFont = nullptr;
		ImFont* m_bodyFont = nullptr;
	};

	inline void Notify(ToastType type, std::string content, std::string title = {})
	{
		Notifications::Get().Add(type, std::move(title), std::move(content));
	}

	inline void NotifySuccess(std::string content, std::string title = {})
	{
		Notifications::Get().Add(ToastType::Success, std::move(title), std::move(content));
	}

	inline void NotifyWarning(std::string content, std::string title = {})
	{
		Notifications::Get().Add(ToastType::Warning, std::move(title), std::move(content));
	}

	inline void NotifyError(std::string content, std::string title = {})
	{
		Notifications::Get().Add(ToastType::Error, std::move(title), std::move(content));
	}

	inline void NotifyInfo(std::string content, std::string title = {})
	{
		Notifications::Get().Add(ToastType::Info, std::move(title), std::move(content));
	}
}