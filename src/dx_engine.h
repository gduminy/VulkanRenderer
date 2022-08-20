#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/module.h>
#include <directxmath.h>
#include <vector>

using namespace Microsoft::WRL;
using namespace DirectX;

constexpr unsigned int FRAME_OVERLAP = 3;
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

struct SceneConstantBufferFrameResource
{
	XMFLOAT4X4 mvp;        // Model-view-projection (MVP) matrix.
	FLOAT padding[48];
};

struct FrameResources
{
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandAllocator> m_bundleAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_bundle;
	ComPtr<ID3D12Resource> m_cbvUploadHeap;
	SceneConstantBufferFrameResource* m_pConstantBuffers;

	UINT64 m_fenceValue;

	std::vector<XMFLOAT4X4> m_modelMatrices;
	UINT m_cityRowCount;
	UINT m_cityColumnCount;
	UINT m_cityMaterialCount;

	void InitBundle(ID3D12Device* pDevice, ID3D12PipelineState* pPso,
		UINT frameResourceIndex, UINT numIndices, D3D12_INDEX_BUFFER_VIEW* pIndexBufferViewDesc, D3D12_VERTEX_BUFFER_VIEW* pVertexBufferViewDesc,
		ID3D12DescriptorHeap* pCbvSrvDescriptorHeap, UINT cbvSrvDescriptorSize, ID3D12DescriptorHeap* pSamplerDescriptorHeap, ID3D12RootSignature* pRootSignature);

	void PopulateCommandList(ID3D12GraphicsCommandList* pCommandList,
		UINT frameResourceIndex, UINT numIndices, D3D12_INDEX_BUFFER_VIEW* pIndexBufferViewDesc, D3D12_VERTEX_BUFFER_VIEW* pVertexBufferViewDesc,
		ID3D12DescriptorHeap* pCbvSrvDescriptorHeap, UINT cbvSrvDescriptorSize, ID3D12DescriptorHeap* pSamplerDescriptorHeap, ID3D12RootSignature* pRootSignature);

	void XM_CALLCONV UpdateConstantBuffers(FXMMATRIX view, CXMMATRIX projection);

};

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

	static const UINT FrameCount = 3;
	static const UINT CityRowCount = 15;
	static const UINT CityColumnCount = 8;
	static const UINT CityMaterialCount = CityRowCount * CityColumnCount;
	static const UINT CityMaterialTextureWidth = 64;
	static const UINT CityMaterialTextureHeight = 64;
	static const UINT CityMaterialTextureChannelCount = 4;
	static const bool UseBundles = true;
	static const float CitySpacingInterval;


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
	ComPtr<ID3D12Resource> m_depthStencil;



	ComPtr<ID3D12DescriptorHeap> m_cbvsrvHeap;
	ComPtr<ID3D12Resource> m_constantBuffer;
	SceneConstantBuffer m_constantBufferData = {};
	UINT8* m_pCbvDataBegin;

	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12DescriptorHeap> m_samplerHeap;

	ComPtr<ID3D12Resource> m_texture;

	//App Resource
	UINT m_numIndices;
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
	ComPtr<ID3D12Resource> m_cityDiffuseTexture;
	ComPtr<ID3D12Resource> m_cityMaterialTextures[CityMaterialCount];
	//StepTimer m_timer;
	UINT m_cbvDescriptorSize;
	UINT m_rtvDescriptorSize;
	//SimpleCamera m_camera;

	//Frame resources
	std::vector<FrameResources*> m_frameResources;
	FrameResources* m_currentFrameResource;
	UINT m_currentFrameResourceIndex;

	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue[FRAME_OVERLAP];

private:

	void LoadPipeline();
	void LoadAssets();

	void WaitForGpu();
	//utility
	void MoveToNextFrame();
	std::vector<UINT8> GenerateTextureData();
	void PopulateCommandList();
	void PopulateCommandList(FrameResources* pFrameResource);
};