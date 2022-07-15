#include <dx_engine.h>
#include <SDL.h>
#include <stdexcept>
#include "SDL_syswm.h"
#include <d3d12.h>
#include "d3dx12.h"
#include <d3dcompiler.h>

// #define DX_CHECK(x)															\
// 	do																		\
// 	{																		\
// 		HRESULT err = x;													\
// 		if (x < 0)															\
// 		{																	\
// 			std::cout << "Detected Direct X error: " << err << std::endl;	\
// 			abort();														\
// 		}																	\
// 	} while (0);															\

static void throwIfFail(HRESULT hr, const char* errMessage)
{
	if (FAILED(hr))
	{
		throw std::runtime_error(errMessage);
	}
}

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::runtime_error("");
	}
}

void DxEngine::init(int width, int height)
{
	SDL_Init(SDL_INIT_VIDEO);
	this->m_width = width;
	this->m_height = height;
	window = SDL_CreateWindow(
		"DirectX 12 Engine",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		width,
		height,
		0
	);

	m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	LoadPipeline();
	LoadAssets();
// 	init_dx12();
// 
// 	init_swapchain();
// 
// 	init_pipeline();
// 
// 	init_command();
// 
// 	upload_mesh();
// 
// 	init_sync_structures();
	//everything went fine
	isInitialized = true;
}

void DxEngine::cleanup()
{
	if ( isInitialized )
	{
		SDL_DestroyWindow(window);

		// Wait for the GPU to be done with all resources.
		WaitForPreviousFrame();

		CloseHandle(m_fenceEvent);
	}
}

void DxEngine::draw()
{
	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(1, 0));

	WaitForPreviousFrame();
}

void DxEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	//main loop
	while (!bQuit)
	{
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT)
			{
				bQuit = true;
			}
		}

		draw();
	}
}

void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter)
{
	*ppAdapter = nullptr;
	for (UINT adapterIndex = 0; ; ++adapterIndex)
	{
		IDXGIAdapter1* pAdapter = nullptr;
		if (DXGI_ERROR_NOT_FOUND == pFactory->EnumAdapters1(adapterIndex, &pAdapter))
		{
			// No more adapters to enumerate.
			break;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			*ppAdapter = pAdapter;
			return;
		}
		pAdapter->Release();
	}
}

void DxEngine::LoadPipeline()
{
	UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

		ComPtr<IDXGIAdapter1> hardwareAdapter;
		GetHardwareAdapter(factory.Get(), &hardwareAdapter);

		ThrowIfFailed(D3D12CreateDevice(
			hardwareAdapter.Get(),
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(&m_device)
		));

	// Describe and create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
		GetActiveWindow(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	ZeroMemory(&m_viewport, sizeof(D3D12_VIEWPORT));
	m_viewport.TopLeftX = 0;
	m_viewport.TopLeftY = 0;
	m_viewport.Width = (float)m_width;
	m_viewport.Height = (float)m_height;
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_scissorRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };

	// This sample does not support fullscreen transitions.
	ThrowIfFailed(factory->MakeWindowAssociation(GetActiveWindow(), DXGI_MWA_NO_ALT_ENTER));

	ThrowIfFailed(swapChain.As(&m_swapChain));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}
	}

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

void DxEngine::LoadAssets()
{
	// Create an empty root signature.
	{
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	}

	// Create the pipeline state, which includes compiling and loading shaders.
	{
		ComPtr<ID3DBlob> vertexShader;
		ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif

		ThrowIfFailed(D3DCompileFromFile(L"../../shaders/shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(L"../../shaders/shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	// Create the command list.
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

	// Command lists are created in the recording state, but there is nothing
	// to record yet. The main loop expects it to be closed, so close it now.
	ThrowIfFailed(m_commandList->Close());

	// Create the vertex buffer.
	{
		// Define the geometry for a triangle.
		Vertex triangleVertices[] =
		{
			{ { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		// Note: using upload heaps to transfer static data like vert buffers is not 
		// recommended. Every time the GPU needs it, the upload heap will be marshalled 
		// over. Please read up on Default Heap usage. An upload heap is used here for 
		// code simplicity and because there are very few verts to actually transfer.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));

		// Copy the triangle data to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
		m_vertexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;
	}

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue = 1;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForPreviousFrame();
	}
}

// void DxEngine::init_dx12()
// {
// 	UINT dxgiFactoryFlags = 0;
// 	//Enable Debug Layer
// 	ComPtr<ID3D12Debug> debugController;
// 	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
// 	{
// 		debugController->EnableDebugLayer();
// 
// 		// Enable additional debug layers.
// 		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
// 	}
// 
// 	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_factory));
// 	throwIfFail(hr, "Factory creation failed");
// 
// 	ComPtr<IDXGIAdapter1> hardwareAdapter;
// 	GetHardwareAdapter(m_factory.Get(), &hardwareAdapter);
// 
// 	hr = D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
// 	throwIfFail(hr, "Device creation failed");
// 
// 	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
// 	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
// 	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
// 
// 	hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
// 
// 	throwIfFail(hr, "Command Queue creation Failed");
// }
// 
// void DxEngine::init_swapchain()
// {
// 	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
// 	swapChainDesc.BufferCount = FRAME_OVERLAP;
// 	swapChainDesc.Width = m_width;
// 	swapChainDesc.Height = m_height;
// 	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
// 	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
// 	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
// 	swapChainDesc.SampleDesc.Count = 1;
// 
// 	SDL_SysWMinfo info = {};
// 	SDL_GetWindowWMInfo(window, &info);
// 
// 	ComPtr<IDXGISwapChain1> tempSwapChain;
// 	HRESULT hr = m_factory->CreateSwapChainForHwnd(m_commandQueue.Get(), info.info.win.window, &swapChainDesc, nullptr, nullptr, &tempSwapChain);
// 	throwIfFail(hr, "Swap Chain creation failed");
// 
// 	hr = m_factory->MakeWindowAssociation(info.info.win.window, DXGI_MWA_NO_ALT_ENTER);
// 	throwIfFail(hr, "");
// 
// 	hr = tempSwapChain.As(&m_swapChain);
// 	throwIfFail(hr, "Swap chain creation failed");
// 
// 	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
// 
// 	{
// 		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
// 		rtvHeapDesc.NumDescriptors = FRAME_OVERLAP;
// 		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
// 		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
// 		hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
// 		throwIfFail(hr, "Descriptor heap creation failed");
// 
// 		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
// 	}
// 
// 	{
// 		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
// 
// 		for (int i = 0; i < FRAME_OVERLAP; i++)
// 		{
// 			hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
// 			throwIfFail(hr, "Failed to create a RTV for the swap chain");
// 			m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
// 			rtvHandle.Offset(1, m_rtvDescriptorSize);
// 		}
// 	}
// }
// 
// void DxEngine::init_command()
// {
// 	HRESULT hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
// 	throwIfFail(hr, "failed to create the command allocator");
// 
// 	hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList));
// 	throwIfFail(hr, "Failed to create the command list");
// 
// 	m_commandList->Close();
// }
// 
// void DxEngine::init_pipeline()
// {
// 	//Create empty root signature
// 	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
// 	rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
// 	ComPtr<ID3DBlob> signature;
// 	ComPtr<ID3DBlob> error;
// 
// 	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
// 	throwIfFail(hr, "Failed to Serialize the root signature");
// 
// 	hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
// 	throwIfFail(hr, "Failed to create the root signature");
// 
// 	//Create the pipeline state
// 	ComPtr<ID3DBlob> vertexShader;
// 	ComPtr<ID3DBlob> pixelShader;
// 
// 	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
// 
// 	hr = D3DCompileFromFile(L"../../shaders/shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr);
// 	throwIfFail(hr, "Failed to compile the Vertex Shader");
// 
// 	hr = D3DCompileFromFile(L"../../shaders/shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr);
// 	throwIfFail(hr, "Failed to compile the Pixel Shader");
// 
// 	//Define the vertex input layout
// 	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
// 	{
// 		{"POSITION", 0,DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
// 		{"COLOR", 0,DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
// 	};
// 
// 	//Describe and create the graphics pipeline state object (PSO)
// 	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
// 	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
// 	psoDesc.pRootSignature = m_rootSignature.Get();
// 	psoDesc.VS = { reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
// 	psoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
// 	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
// 	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
// 	psoDesc.DepthStencilState.DepthEnable = false;
// 	psoDesc.DepthStencilState.StencilEnable = false;
// 	psoDesc.SampleMask = UINT_MAX;
// 	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
// 	psoDesc.NumRenderTargets = 1;
// 	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
// 	psoDesc.SampleDesc.Count = 1;
// 
// 	hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
// 	throwIfFail(hr, "Failed to create graphics pipeline state");
// }
// 
// void DxEngine::upload_mesh()
// {
// 	Vertex triangleVertices[] =
// 	{
// 		{ { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
// 		{ { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
// 		{ { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
// 	};
// 
// 	const UINT vertexBufferSize = sizeof(triangleVertices);
// 
// 	HRESULT hr = m_device->CreateCommittedResource(
// 		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
// 		D3D12_HEAP_FLAG_NONE,
// 		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
// 		D3D12_RESOURCE_STATE_GENERIC_READ,
// 		nullptr,
// 		IID_PPV_ARGS(&m_vertexBuffer));
// 	throwIfFail(hr, "failed to create commited resource");
// 
// 	//Copy the triangle data to the vertex buffer
// 	UINT8* vertexDataBegin;
// 	CD3DX12_RANGE readRange(0, 0); // We do not intent to read from this resource on the CPU
// 	hr = m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin));
// 	throwIfFail(hr, "failed to map the buffer");
// 	memcpy(vertexDataBegin, triangleVertices, sizeof(triangleVertices));
// 	m_vertexBuffer->Unmap(0, nullptr);
// 
// 	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
// 	m_vertexBufferView.StrideInBytes = sizeof(Vertex);
// 	m_vertexBufferView.SizeInBytes = vertexBufferSize;
// }
// 
// void DxEngine::init_sync_structures()
// {
// 	HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
// 	throwIfFail(hr, "Failed to create a fence");
// 	m_fenceValue = 1;
// 
// 	// Create an event handle to use for frame synchronization.
// 	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
// 	if (m_fenceEvent == nullptr)
// 	{
// 		throwIfFail(HRESULT_FROM_WIN32(GetLastError()), "failed to create an event handle for frame syncro");
// 	}
// 
// 	WaitForPreviousFrame();
// }



void DxEngine::WaitForPreviousFrame()
{
	// WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	// This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
	// sample illustrates how to use fences for efficient resource usage and to
	// maximize GPU utilization.

	// Signal and increment the fence value.
	const UINT64 fence = m_fenceValue;
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
	m_fenceValue++;

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fence)
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DxEngine::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocator->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_commandList->DrawInstanced(3, 1, 0, 0);

	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(m_commandList->Close());

}