#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/module.h>
#include <directxmath.h>
#include <vector>

using namespace Microsoft::WRL;
using namespace DirectX;

constexpr unsigned int FRAME_OVERLAP = 2;
constexpr unsigned int TextureWidth = 256;
constexpr unsigned int TextureHeight = 256;
constexpr unsigned int TexturePixelSize = 4;  // The number of bytes used to represent a pixel in the texture.

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT4 color;
	XMFLOAT2 uv;
};

struct SceneConstantBuffer
{
	XMFLOAT4 offset;
	float padding[60]; //Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant buffer size must be aligned on 256 bytes");

class DxEngine
{
public:

	//initializes everything in the engine
	void init(int width, int height);
	//shuts down the engine
	void cleanup();
	// Update frame-based values.
	void update();
	//draw loop
	void draw();
	//run main loop
	void run();
	struct SDL_Window* window{ nullptr };
	bool isInitialized{ false };


	int m_width;
	int m_height;
	float m_aspectRatio;
	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;
	ComPtr<IDXGIFactory4> m_factory;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;


	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator[FRAME_OVERLAP];
	ComPtr<ID3D12CommandAllocator> m_bundleAllocator[FRAME_OVERLAP];
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12GraphicsCommandList> m_bundle;

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	UINT m_rtvDescriptorSize;
	ComPtr<ID3D12Resource> m_renderTargets[FRAME_OVERLAP];

	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12DescriptorHeap> m_cbvsrvHeap;
	ComPtr<ID3D12Resource> m_constantBuffer;
	SceneConstantBuffer m_constantBufferData = {};
	UINT8* m_pCbvDataBegin;

	//ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	ComPtr<ID3D12Resource> m_texture;

	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue[FRAME_OVERLAP];

private:
	static const UINT FrameCount = 2;
	void LoadPipeline();
	void LoadAssets();

	void WaitForGpu();
	//utility
	void MoveToNextFrame();
	std::vector<UINT8> GenerateTextureData();
	void PopulateCommandList();
};