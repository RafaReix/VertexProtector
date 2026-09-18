#pragma once

#include <Framework/Framework.hpp>

#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

namespace Framework::Utils
{
	class DirectX11
	{
	public:
		DirectX11() = default;
		~DirectX11()
		{
			Shutdown();
		}

		bool Initialize(HWND hWindow)
		{
			DXGI_SWAP_CHAIN_DESC sd = {};
			sd.BufferCount = 2;
			sd.BufferDesc.Width = 0;
			sd.BufferDesc.Height = 0;
			sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sd.BufferDesc.RefreshRate.Numerator = 60;
			sd.BufferDesc.RefreshRate.Denominator = 1;
			sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			sd.OutputWindow = hWindow;
			sd.SampleDesc.Count = 1;
			sd.SampleDesc.Quality = 0;
			sd.Windowed = TRUE;
			sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

			UINT createDeviceFlags = 0;
			D3D_FEATURE_LEVEL featureLevel;
			const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

			HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_SwapChain, &m_Device, &featureLevel, &m_DeviceContext);

			if (hr == DXGI_ERROR_UNSUPPORTED)
				hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_SwapChain, &m_Device, &featureLevel, &m_DeviceContext);

			if (FAILED(hr))
				return false;

			CreateRenderTarget();
			return true;
		}

		void Shutdown()
		{
			CleanupRenderTarget();

			if (m_SwapChain) { m_SwapChain->Release(); m_SwapChain = nullptr; }
			if (m_DeviceContext) { m_DeviceContext->Release(); m_DeviceContext = nullptr; }
			if (m_Device) { m_Device->Release(); m_Device = nullptr; }
		}

		void Resize(UINT width, UINT height)
		{
			if (!m_SwapChain)
				return;

			CleanupRenderTarget();
			m_SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}

		void BeginFrame(const float clearColor[4])
		{
			m_DeviceContext->OMSetRenderTargets(1, &m_RenderTargetView, nullptr);
			m_DeviceContext->ClearRenderTargetView(m_RenderTargetView, clearColor);
		}

		void Present(bool vsync = true)
		{
			m_SwapChain->Present(vsync ? 1 : 0, 0);
		}

		ID3D11Device* GetDevice() const { return m_Device; }
		ID3D11DeviceContext* GetDeviceContext() const { return m_DeviceContext; }
		bool IsValid() const { return m_Device != nullptr; }

	private:
		void CreateRenderTarget()
		{
			ID3D11Texture2D* backBuffer = nullptr;
			m_SwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
			if (backBuffer)
			{
				m_Device->CreateRenderTargetView(backBuffer, nullptr, &m_RenderTargetView);
				backBuffer->Release();
			}
		}

		void CleanupRenderTarget()
		{
			if (m_RenderTargetView)
			{
				m_RenderTargetView->Release();
				m_RenderTargetView = nullptr;
			}
		}

	private:
		ID3D11Device* m_Device = nullptr;
		ID3D11DeviceContext* m_DeviceContext = nullptr;
		IDXGISwapChain* m_SwapChain = nullptr;
		ID3D11RenderTargetView* m_RenderTargetView = nullptr;
	};
}
