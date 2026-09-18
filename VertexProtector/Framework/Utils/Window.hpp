#pragma once

#include <Framework/Framework.hpp>
#include <Framework/Utils/DirectX11.hpp>
#include <Framework/UI/Menu.hpp>
#include <Framework/UI/Notifications.hpp>

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Framework::Utils
{
	class Window
	{
	public:
		Window() = default;
		~Window()
		{
			Destroy();
		}

		bool Initialize(const wchar_t* title = L"VertexProtector")
		{
			m_WindowClass.cbSize = sizeof(WNDCLASSEX);
			m_WindowClass.style = CS_HREDRAW | CS_VREDRAW;
			m_WindowClass.lpfnWndProc = WindowProc;
			m_WindowClass.cbClsExtra = 0;
			m_WindowClass.cbWndExtra = 0;
			m_WindowClass.hInstance = GetModuleHandle(NULL);
			m_WindowClass.hIcon = NULL;
			m_WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
			m_WindowClass.hbrBackground = HBRUSH(GetStockObject(BLACK_BRUSH));
			m_WindowClass.lpszMenuName = NULL;
			m_WindowClass.lpszClassName = L"WindowClass";
			m_WindowClass.hIconSm = NULL;

			if (!RegisterClassEx(&m_WindowClass))
				return false;

			RECT rect = { 0, 0, m_Width, m_Height };
			AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

			const int windowWidth = rect.right - rect.left;
			const int windowHeight = rect.bottom - rect.top;

			const int x = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
			const int y = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;

			m_hWindow = CreateWindowEx(0, m_WindowClass.lpszClassName, title, WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX), x, y, windowWidth, windowHeight, NULL, NULL, m_WindowClass.hInstance, this);

			if (!m_hWindow)
			{
				UnregisterClass(m_WindowClass.lpszClassName, m_WindowClass.hInstance);
				return false;
			}

			if (!m_DirectX.Initialize(m_hWindow))
			{
				Destroy();
				return false;
			}

			DragAcceptFiles(m_hWindow, TRUE);

			InitializeImGui();

			ShowWindow(m_hWindow, SW_SHOW);
			UpdateWindow(m_hWindow);

			return true;
		}

		void Run()
		{
			bool done = false;
			while (!done)
			{
				MSG msg;
				while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
					if (msg.message == WM_QUIT)
						done = true;
				}

				if (done)
					break;

				if (m_ResizeWidth != 0 && m_ResizeHeight != 0)
				{
					m_DirectX.Resize(m_ResizeWidth, m_ResizeHeight);
					m_ResizeWidth = m_ResizeHeight = 0;
				}

				ImGui_ImplDX11_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();

				m_Menu.Render();

				UI::Notifications::Get().Render();

				ImGui::Render();
				const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
				m_DirectX.BeginFrame(clearColor);
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
				m_DirectX.Present();
			}
		}

		void Destroy()
		{
			ShutdownImGui();
			m_DirectX.Shutdown();

			if (m_hWindow)
			{
				DestroyWindow(m_hWindow);
				m_hWindow = NULL;
			}

			if (m_WindowClass.hInstance)
			{
				UnregisterClass(m_WindowClass.lpszClassName, m_WindowClass.hInstance);
				m_WindowClass = {};
			}
		}

		HWND GetHandle() const { return m_hWindow; }
		int GetWidth() const { return m_Width; }
		int GetHeight() const { return m_Height; }

	private:
		void InitializeImGui()
		{
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();

			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

			io.IniFilename = nullptr;
			io.WantSaveIniSettings = false;

			ImGui::StyleColorsDark();

			UI::FontManager::Get().Load();

			UI::Notifications::Get().SetFonts(UI::FontManager::Get()[UI::FontID::Bold], UI::FontManager::Get()[UI::FontID::Regular]);

			ImGui_ImplWin32_Init(m_hWindow);
			ImGui_ImplDX11_Init(m_DirectX.GetDevice(), m_DirectX.GetDeviceContext());

			m_ImGuiInitialized = true;
		}

		void ShutdownImGui()
		{
			if (!m_ImGuiInitialized)
				return;

			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();

			m_ImGuiInitialized = false;
		}

		static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
				return true;

			Window* self = nullptr;

			if (msg == WM_NCCREATE)
			{
				auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
				self = reinterpret_cast<Window*>(create->lpCreateParams);
				SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
				self->m_hWindow = hWnd;
			}
			else
			{
				self = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
			}

			if (self)
				return self->HandleMessage(hWnd, msg, wParam, lParam);

			return DefWindowProc(hWnd, msg, wParam, lParam);
		}

		LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			switch (msg)
			{
			case WM_SIZE:
				if (wParam == SIZE_MINIMIZED)
					return 0;

				m_Width = m_ResizeWidth = LOWORD(lParam);
				m_Height = m_ResizeHeight = HIWORD(lParam);
				return 0;

			case WM_SYSCOMMAND:
				if ((wParam & 0xFFF0) == SC_KEYMENU)
					return 0;
				break;

			case WM_DROPFILES:
			{
				HDROP hDrop = reinterpret_cast<HDROP>(wParam);
				const UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

				auto& droppedFiles = ImGui::GetIO().droppedFiles;
				droppedFiles.reserve(droppedFiles.size() + count);

				for (UINT i = 0; i < count; ++i)
				{
					const UINT length = DragQueryFileW(hDrop, i, nullptr, 0);
					std::wstring path(length, L'\0');
					DragQueryFileW(hDrop, i, path.data(), length + 1);
					droppedFiles.emplace_back(std::move(path));
				}

				DragFinish(hDrop);
				return 0;
			}

			case WM_DESTROY:
				PostQuitMessage(0);
				return 0;
			}

			return DefWindowProc(hWnd, msg, wParam, lParam);
		}

	private:
		int m_Width = 800;
		int m_Height = 600;
		UINT m_ResizeWidth = 0;
		UINT m_ResizeHeight = 0;
		HWND m_hWindow = NULL;
		WNDCLASSEX m_WindowClass = {};

		DirectX11 m_DirectX;
		UI::Menu m_Menu;
		bool m_ImGuiInitialized = false;
	};
}
