#pragma once
#include "TeslaWin.h"
#include "TeslaException.h"
#include "DxgiInfoManager.h"
#include "Surface.h"
#include <d3d11.h>
#include <wrl.h>
#include <sstream>

class Graphics
{
public:
	// Graphics exception handling
	class Exception : public TeslaException
	{
		using TeslaException::TeslaException;
	};
	class HrException : public Exception
	{
	public:
		HrException(int line, const char* file, HRESULT hr, std::vector<std::string> infoMsgs = {}) noexcept;
		const char* what() const noexcept override;
		const char* GetType() const noexcept override;
		HRESULT GetErrorCode() const noexcept;
		std::string GetErrorString() const noexcept;
		std::string GetErrorDescription() const noexcept;
		std::string GetErrorInfo() const noexcept;
	private:
		HRESULT hr;
		std::string info;
	};
	class InfoException : public Exception
	{
	public:
		InfoException(int line, const char* file, std::vector<std::string> infoMsgs = {}) noexcept;
		virtual const char* what() const noexcept override;
		virtual const char* GetType() const noexcept override;
		std::string GetErrorInfo() const noexcept;
	private:
		std::string info;
	};
	class DeviceRemovedException : public HrException
	{
		using HrException::HrException;
	public:
		const char* GetType() const noexcept override;
	private:
		std::string reason;
	};
public:
	Graphics(HWND hWnd);
	Graphics(const Graphics&) = delete;
	Graphics& operator = (const Graphics&) = delete;
	~Graphics();
public:
	void BeginFrame(bool clear = true, Color clearColor = Color::Black);
	void EndFrame();
	void Clear(Color fillColor) noexcept;
	void EnableVSync() noexcept;
	void DisableVSync() noexcept;
	void SetVSyncInterval(const UINT verticalSyncInterval) noexcept;
	bool IsVSyncEnabled() const noexcept;
	void EnableImGui() noexcept;
	void DisableImGui() noexcept;
	bool IsImGuiEnabled() const noexcept;
	void PutPixel(const std::pair<unsigned int, unsigned int>& p, Color c);
	void PutPixel(unsigned int x, unsigned int y, Color c);
	void PutPixel(unsigned int x, unsigned int y, unsigned int r, unsigned int g, unsigned int b);
	std::string GetFrameStatistics() const noexcept;
private:
	void UpdateFrameStatistics() noexcept;
private:
	bool imGuiEnabled = true;
	UINT syncInterval = 1u;
	std::string statsInfo;
	std::string title = "Adrian Tesla DirectX Framework";
private:
	Microsoft::WRL::ComPtr<ID3D11Device>           pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>    pContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>         pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pTargetView;
	Microsoft::WRL::ComPtr<ID3D11Texture2D>        pTexture;
	D3D11_MAPPED_SUBRESOURCE msr;
private:
#ifndef NDEBUG
	DxgiInfoManager infoManager;
#endif
public:
	Surface pBuffer;
public:
	static constexpr unsigned int PixelSize    = 1u;
	static constexpr unsigned int ScreenWidth  = 800u;
	static constexpr unsigned int ScreenHeight = 600u;
};