#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/module.h>
#include <directxmath.h>

using namespace Microsoft::WRL;

constexpr unsigned int FRAME_OVERLAP = 2;

struct Vertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 color;
};

class DxEngine
{
public:

	//initializes everything in the engine
	void init(int width, int height);
	//shuts down the engine
	void cleanup();
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
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	UINT m_rtvDescriptorSize;
	ComPtr<ID3D12Resource> m_renderTargets[FRAME_OVERLAP];

	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

private:
	static const UINT FrameCount = 2;
	void LoadPipeline();
	void LoadAssets();

	//utility
	void WaitForPreviousFrame();
	void PopulateCommandList();
};