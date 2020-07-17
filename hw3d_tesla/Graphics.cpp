#include "Graphics.h"
#include "dxerr.h"
#include "imgui\imgui_impl_dx11.h"
#include "imgui\imgui_impl_win32.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// graphics exception checking/throwing macros (some with dxgi infos)
#define GFX_EXCEPT_NOINFO(hr) Graphics::HrException( __LINE__,__FILE__,(hr) )
#define GFX_THROW_NOINFO(hrcall) if( FAILED( hr = (hrcall) ) ) throw Graphics::HrException( __LINE__,__FILE__,hr )

#ifndef NDEBUG
#define GFX_EXCEPT(hr) Graphics::HrException( __LINE__,__FILE__,(hr),infoManager.GetMessages() )
#define GFX_THROW_INFO(hrcall) infoManager.Set(); if( FAILED( hr = (hrcall) ) ) throw GFX_EXCEPT(hr)
#define GFX_DEVICE_REMOVED_EXCEPT(hr) Graphics::DeviceRemovedException( __LINE__,__FILE__,(hr),infoManager.GetMessages() )
#define GFX_THROW_INFO_ONLY(call) infoManager.Set(); (call); {auto v = infoManager.GetMessages(); if(!v.empty()) {throw Graphics::InfoException(__LINE__, __FILE__, v);}}
#else
#define GFX_EXCEPT(hr) Graphics::HrException( __LINE__,__FILE__,(hr) )
#define GFX_THROW_INFO(hrcall) GFX_THROW_NOINFO(hrcall)
#define GFX_DEVICE_REMOVED_EXCEPT(hr) Graphics::DeviceRemovedException( __LINE__,__FILE__,(hr) )
#define GFX_THROW_INFO_ONLY(call) (call)
#endif

Graphics::Graphics(HWND hWnd)
	:
	pBuffer(ScreenWidth, ScreenHeight),
	msr({})
{	
	// The graphics is initialized filling the pDevice, pContext and pSwapChain pointers.
	// First we configure the Swap Chain descriptor, passing also the handle to the window
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferDesc.Width                   = ScreenWidth  * PixelSize;
	swapDesc.BufferDesc.Height                  = ScreenHeight * PixelSize;
	swapDesc.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.BufferDesc.RefreshRate.Numerator   = 0u;
	swapDesc.BufferDesc.RefreshRate.Denominator = 0u;
	swapDesc.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
	swapDesc.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapDesc.SampleDesc.Count                   = 1u;
	swapDesc.SampleDesc.Quality                 = 0u;
	swapDesc.Windowed                           = TRUE;
	swapDesc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;	
	swapDesc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
	swapDesc.BufferCount                        = 1u;
	swapDesc.OutputWindow                       = hWnd;
	swapDesc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// We create the Device and the SwapChain on the DEBUG LAYER if we are in DEBUG for 
	// the crucial error information diagnostics
	UINT swapCreateFlags = 0u;
#ifndef NDEBUG
	swapCreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	// Error codes are stored in this type
	HRESULT hr;

	/******************** D3D11 DEVICE AND SWAPCHAIN *********************/
	// And we fill the pipi: this is the official birth of the D3D11 Device
	GFX_THROW_INFO(D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE, // (use D3D_DRIVER_TYPE_WARP for software implementation)
		nullptr,
		swapCreateFlags,
		nullptr,
		0u,
		D3D11_SDK_VERSION,
		&swapDesc,
		&pSwapChain,
		&pDevice,
		nullptr,
		&pContext
	));

	// In order to clear the back buffer (a texture subresource), we need a view on that (pTargetView will be filled)
	Microsoft::WRL::ComPtr<ID3D11Resource> pBackBuffer;
	GFX_THROW_INFO(pSwapChain->GetBuffer(0u, __uuidof(ID3D11Texture2D), &pBackBuffer));
	GFX_THROW_INFO(pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &pTargetView));

	// Give ImGui the pDevice and the pContext pointers to allow it to draw on the screen
	ImGui_ImplDX11_Init(pDevice.Get(), pContext.Get());

	/**************************************************************/
	/******************* SETTING THE PIPELINE *********************/

	// The screen consists of a textured quad on which the pBuffer texture is applied
	struct Vertex
	{
		float x;
		float y;
		float u;
		float v;
	};

	// Textured quad: two triangles that cover the screen, with proper texture coordinates
	static constexpr Vertex vertices[] =
	{
		{-1.0f, 1.0f, 0.0f, 0.0f },
		{ 1.0f, 1.0f, 1.0f, 0.0f },
		{-1.0f,-1.0f, 0.0f, 1.0f },

		{-1.0f,-1.0f, 0.0f, 1.0f },
		{ 1.0f, 1.0f, 1.0f, 0.0f },
		{ 1.0f,-1.0f, 1.0f, 1.0f },
	};

	/*****************************************************/
	/***************** RESOURCE CREATION *****************/

	/*****************************************************/
	/************** Create the Vertex Buffer *************/
	Microsoft::WRL::ComPtr<ID3D11Buffer> pVertexBuffer;
	D3D11_BUFFER_DESC bd   = {};
	bd.Usage               = D3D11_USAGE_DEFAULT;
	bd.CPUAccessFlags      = 0u;
	bd.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
	bd.ByteWidth           = sizeof(vertices);
	bd.StructureByteStride = sizeof(Vertex);
	bd.MiscFlags           = 0u;
	D3D11_SUBRESOURCE_DATA sd = {};
	sd.pSysMem                = vertices;
	GFX_THROW_INFO(pDevice->CreateBuffer(&bd, &sd, &pVertexBuffer));
	UINT strides = sizeof(Vertex);
	UINT offsets = 0u;
	
	/**********************************************************************/
	/************** Create the Vertex Shader and Pixel Shader *************/

	// We compile the shaders at runtime with D3DCompile so the final .exe doesn't need the shaders binary dependencies

	// Vertex Shader HLSL (High Level Shader Language) code
	static const char* vertexShader =
"\
struct VSOut\
{\
	float2 tc : TexCoord;\
	float4 pos : SV_Position;\
};\
\
VSOut main(float2 pos : Position, float2 tc : TexCoord)\
{\
	VSOut v;\
	\
	v.pos = float4(pos.x, pos.y, 0.0f, 1.0f);\
	v.tc = tc;\
	\
	return v;\
};\
";

	// Pixel Shader HLSL (High Level Shader Language) code
	static const char* pixelShader = 
"\
Texture2D tex : register(t0);\
SamplerState splr;\
\
float4 main(float2 tc : TexCoord) : SV_Target\
{\
	return tex.Sample(splr, tc);\
}\
";

	Microsoft::WRL::ComPtr<ID3DBlob> pBlob;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pPixelShader;
	GFX_THROW_INFO(D3DCompile(pixelShader, strlen(pixelShader), nullptr, nullptr, nullptr, "main", "ps_4_0", 0u, 0u, &pBlob, nullptr));
	GFX_THROW_INFO(pDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &pPixelShader));

	Microsoft::WRL::ComPtr<ID3D11VertexShader> pVertexShader;
	GFX_THROW_INFO(D3DCompile(vertexShader, strlen(vertexShader), nullptr, nullptr, nullptr, "main", "vs_4_0", 0u, 0u, &pBlob, nullptr));
	GFX_THROW_INFO(pDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &pVertexShader));

	/****************************************************/
	/************** Create the Input Layout *************/
	Microsoft::WRL::ComPtr<ID3D11InputLayout> pInputLayout;
	D3D11_INPUT_ELEMENT_DESC ied[] =
	{
		{ "Position", 0u, DXGI_FORMAT_R32G32_FLOAT, 0u, 0u, D3D11_INPUT_PER_VERTEX_DATA, 0u },
		{ "TexCoord", 0u, DXGI_FORMAT_R32G32_FLOAT, 0u, 8u, D3D11_INPUT_PER_VERTEX_DATA, 0u },
	};
	GFX_THROW_INFO(pDevice->CreateInputLayout(ied, (UINT)std::size(ied), pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &pInputLayout));
	
	/*************************************************/
	/************** Create the View Port *************/
	D3D11_VIEWPORT vp = {};
	vp.Width          = ScreenWidth * PixelSize;
	vp.Height         = ScreenHeight * PixelSize;
	vp.TopLeftX       = 0.0f;
	vp.TopLeftY       = 0.0f;
	vp.MaxDepth       = 1.0f;
	vp.MinDepth       = 0.0f;

	/*********************************************************/
	/***************** FRAMEBUFFER TEXTURE *******************/
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Format               = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.MipLevels            = 1u;
	texDesc.ArraySize            = 1u;
	texDesc.BindFlags            = D3D11_BIND_SHADER_RESOURCE;
	texDesc.Width                = ScreenWidth;
	texDesc.Height               = ScreenHeight;
	texDesc.Usage                = D3D11_USAGE_DEFAULT;
	texDesc.CPUAccessFlags       = 0u;
	texDesc.SampleDesc.Count     = 1u;
	texDesc.SampleDesc.Quality   = 0u;
	texDesc.MiscFlags            = 0u;
	D3D11_SUBRESOURCE_DATA srd   = {};
	srd.pSysMem                  = pBuffer.GetBufferPtrConst();
	srd.SysMemPitch              = ScreenWidth * sizeof(Color);
	GFX_THROW_INFO(pDevice->CreateTexture2D(&texDesc, &srd, &pTexture));

	// Creation of the view on the texture
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pTextureView;
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                          = texDesc.Format;
	srvDesc.Texture2D.MipLevels             = 1u;
	srvDesc.Texture2D.MostDetailedMip       = 0u;
	srvDesc.ViewDimension                   = D3D11_SRV_DIMENSION_TEXTURE2D;
	GFX_THROW_INFO(pDevice->CreateShaderResourceView(pTexture.Get(), &srvDesc, &pTextureView));
	/*********************************************************/

	/*********************************************************/
	/**************** Create the Sampler State ***************/
	Microsoft::WRL::ComPtr<ID3D11SamplerState> pSamplerState;
	D3D11_SAMPLER_DESC samDesc = {};
	samDesc.AddressU           = D3D11_TEXTURE_ADDRESS_WRAP;
	samDesc.AddressV           = D3D11_TEXTURE_ADDRESS_WRAP;
	samDesc.AddressW           = D3D11_TEXTURE_ADDRESS_WRAP;
	samDesc.ComparisonFunc     = D3D11_COMPARISON_NEVER;
	samDesc.Filter             = D3D11_FILTER_MIN_MAG_MIP_POINT;
	GFX_THROW_INFO(pDevice->CreateSamplerState(&samDesc, &pSamplerState));
	/*********************************************************/

	/*******************************************************************************************/
	/******** Bind all the created resources and configurations to the Graphics Pipeline *******/
	GFX_THROW_INFO_ONLY(pContext->OMSetRenderTargets(1u, pTargetView.GetAddressOf(), nullptr));
	GFX_THROW_INFO_ONLY(pContext->IASetVertexBuffers(0u, 1u, pVertexBuffer.GetAddressOf(), &strides, &offsets));
	GFX_THROW_INFO_ONLY(pContext->PSSetShader(pPixelShader.Get(), nullptr, 0u));
	GFX_THROW_INFO_ONLY(pContext->VSSetShader(pVertexShader.Get(), nullptr, 0u));
	GFX_THROW_INFO_ONLY(pContext->IASetInputLayout(pInputLayout.Get()));
	GFX_THROW_INFO_ONLY(pContext->RSSetViewports(1u, &vp));
	GFX_THROW_INFO_ONLY(pContext->PSSetShaderResources(0u, 1u, pTextureView.GetAddressOf()));
	GFX_THROW_INFO_ONLY(pContext->PSSetSamplers(0u, 1u, pSamplerState.GetAddressOf()));
	GFX_THROW_INFO_ONLY(pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
	/*******************************************************************************************/
}

void Graphics::UpdateFrameStatistics() noexcept
{
	const auto ImGuiIO    = ImGui::GetIO();
	const auto frame_rate = ImGuiIO.Framerate;
	std::stringstream ss;
	ss.precision(3);
	ss << std::fixed << 1000.0f / frame_rate;
	ss.precision(0);
	ss << std::fixed << " ms/frame (" << frame_rate << " FPS)] (" << ScreenWidth << "x" << ScreenHeight << ")";
	statsInfo = ss.str();
}

std::string Graphics::GetFrameStatistics() const noexcept
{
	return statsInfo;
}

Graphics::~Graphics()
{
	ImGui_ImplDX11_Shutdown();
}

void Graphics::BeginFrame(bool clear, Color clearColor)
{
	if (clear)
	{
		Clear(clearColor);
	}
	// We always do an ImGui NewFrame because of the useful framerate counter 
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void Graphics::EndFrame() 
{
	HRESULT hr;

	UpdateFrameStatistics();

	// Update the framebuffer stored in the GPU memory with our color pBuffer 
	GFX_THROW_INFO_ONLY(pContext->UpdateSubresource(pTexture.Get(), 0u, nullptr, pBuffer.GetBufferPtrConst(), (UINT)pBuffer.GetRowPitch(), 0u));

	// Draw the CPU Frame Buffer
	GFX_THROW_INFO_ONLY(pContext->Draw(6u, 0u));

	// Render ImGui data on the screen only if it's enabled
	ImGui::Render();
	if (imGuiEnabled)
	{
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

#ifndef NDEBUG
	infoManager.Set();
#endif
	if (FAILED(hr = pSwapChain->Present(syncInterval, 0u)))
	{
		if (hr == DXGI_ERROR_DEVICE_REMOVED)
		{
			throw GFX_DEVICE_REMOVED_EXCEPT(hr);
		}
		else
		{
			throw GFX_EXCEPT(hr);
		}
	}
}

void Graphics::Clear(Color c) noexcept
{
	pBuffer.Clear(c);
}

void Graphics::EnableVSync() noexcept
{
	syncInterval = 1u;
}

void Graphics::DisableVSync() noexcept
{
	syncInterval = 0u;
}

void Graphics::SetVSyncInterval(const UINT verticalSyncInterval) noexcept
{
	syncInterval = verticalSyncInterval;
}

bool Graphics::IsVSyncEnabled() const noexcept
{
	return syncInterval != 0u;
}

void Graphics::EnableImGui() noexcept
{
	imGuiEnabled = true;
}

void Graphics::DisableImGui() noexcept
{
	imGuiEnabled = false;
}

bool Graphics::IsImGuiEnabled() const noexcept
{
	return imGuiEnabled;
}

void Graphics::PutPixel(const std::pair<unsigned int, unsigned int>& p, Color c)
{
	PutPixel(p.first, p.second, c);
}

void Graphics::PutPixel(unsigned int x, unsigned int y, Color c)
{
	pBuffer.PutPixel(x, y, c);
}

void Graphics::PutPixel(unsigned int x, unsigned int y, unsigned int r, unsigned int g, unsigned int b)
{
	PutPixel(x, y, Color(r, g, b));
}

Graphics::HrException::HrException(int line, const char* file, HRESULT hr, std::vector<std::string> infoMsgs) noexcept
	:
	Exception(line, file),
	hr(hr)
{
	// Join all info messages with newlines into single string
	for (const auto& m : infoMsgs)
	{
		info += m;
		info.push_back('\n');
	}
	// Remove final newline if exists
	if (!info.empty())
	{
		info.pop_back();
	}
}

const char* Graphics::HrException::what() const noexcept
{
	std::ostringstream oss;
	oss << GetType() << std::endl
		<< "[Error Code] 0x" << std::hex << std::uppercase << GetErrorCode()
		<< std::dec << " (" << (unsigned long)GetErrorCode() << ")" << std::endl
		<< "[Error String] " << GetErrorString() << std::endl
		<< "[Description] " << GetErrorDescription() << std::endl;
	if (!info.empty())
	{
		oss << "\n[Error Info]\n" << GetErrorInfo() << std::endl << std::endl;
	}
	oss << GetOriginString();
	whatBuffer = oss.str();
	return whatBuffer.c_str();
}

const char* Graphics::HrException::GetType() const noexcept
{
	return "Tesla Graphics Exception";
}

HRESULT Graphics::HrException::GetErrorCode() const noexcept
{
	return hr;
}

std::string Graphics::HrException::GetErrorString() const noexcept
{
	return DXGetErrorString(hr);
}

std::string Graphics::HrException::GetErrorDescription() const noexcept
{
	char buf[512];
	DXGetErrorDescription(hr, buf, sizeof(buf));
	return buf;
}

std::string Graphics::HrException::GetErrorInfo() const noexcept
{
	return info;
}

const char* Graphics::DeviceRemovedException::GetType() const noexcept
{
	return "Tesla Graphics Exception [Device Removed] (DXGI_ERROR_DEVICE_REMOVED)";
}

Graphics::InfoException::InfoException(int line, const char* file, std::vector<std::string> infoMsgs) noexcept
	:
	Exception(line, file)
{
	for (const auto& m : infoMsgs)
	{
		info += m;
		info.append("\n\n");
	}
	if (!info.empty())
	{
		info.pop_back();
	}
}

const char* Graphics::InfoException::what() const noexcept
{
	std::ostringstream oss;
	oss << GetType() << std::endl
		<< "\n[Error Info]\n" << GetErrorInfo() << std::endl << std::endl;
	oss << GetOriginString();
	whatBuffer = oss.str();
	return whatBuffer.c_str();
}

const char* Graphics::InfoException::GetType() const noexcept
{
	return "Tesla Graphics Info Exception";
}

std::string Graphics::InfoException::GetErrorInfo() const noexcept
{
	return info;
}