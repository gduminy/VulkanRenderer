#include <dx_engine.h>
#include <SDL.h>
#include <stdexcept>
#include "SDL_syswm.h"
#include <d3d12.h>
#include "d3dx12.h"
#include <d3dcompiler.h>
#include "Helper.h"
#include <DirectXMathMatrix.inl>
#include "dx_initializers.h"
#include "dx_texture.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_dx12.h"

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
	m_camera.Init({ 0.f, 3.f, 10.f });
	m_frameCounter = 0;
	LoadPipeline();
	LoadAssets();
	InitScene();
	InitImGui();


	//everything went fine
	isInitialized = true;
}

void DxEngine::cleanup()
{
	if ( isInitialized )
	{
		SDL_DestroyWindow(window);

		// Wait for the GPU to be done with all resources.
		WaitForGpu();

		CloseHandle(m_fenceEvent);
	}
}

void DxEngine::update()
{

	m_timer.Tick(NULL);

	m_frameCounter++;

	m_camera.Update(static_cast<float>(m_timer.GetElapsedSeconds()));

	XMMATRIX view = m_camera.GetViewMatrix();
	XMMATRIX projection = m_camera.GetProjectionMatrix(0.8f, m_aspectRatio);

	//XMMATRIX model = XMMatrixRotationY(XMConvertToRadians(m_frameCounter * 0.4f));// XMMatrixTranslation(0, 0, -3);
	for (int i = 0; i < m_renderables.size(); i++)
	{
		XMMATRIX model = m_renderables[i].tranformMatrix;
		XMFLOAT4X4 mesh_matrix;
		XMStoreFloat4x4(&mesh_matrix, XMMatrixTranspose(model * view * projection));

		m_meshConstantBufferData.mvp = mesh_matrix;
		memcpy(&m_pCbvDataBegin[i], &m_meshConstantBufferData, sizeof(m_meshConstantBufferData));
	}

}

void DxEngine::draw()
{
	ImGui::Render();
	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(1, 0));

	MoveToNextFrame();
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
			//ImGui Process Event
			ImGui_ImplSDL2_ProcessEvent(&e);

			//close the window when user alt-f4s or clicks the X button		
			if (e.type == SDL_QUIT)
			{
				bQuit = true;
			}
			else if (e.type == SDL_KEYDOWN)
			{
				m_camera.OnKeyDown(e.key.keysym.scancode);
			}
			else if (e.type == SDL_KEYUP)
			{
				m_camera.OnKeyUp(e.key.keysym.scancode);
			}
		}


		//imgui new frame
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplSDL2_NewFrame(window);

		ImGui::NewFrame();


		ImGui::ShowDemoWindow();

		update();
		draw();
	}
}

Material* DxEngine::createMaterial(ComPtr<ID3D12RootSignature> rootSignature, ComPtr<ID3D12PipelineState> pipelineState, const std::string& name)
{
	Material mat;
	mat.m_pipelineState = pipelineState.Get();
	mat.m_rootSignature = rootSignature.Get();
	m_materials[name] = mat;
	return &m_materials[name];
}

Material* DxEngine::getMaterial(const std::string& name)
{
	auto it = m_materials.find(name);
	if (it == m_materials.end())
	{
		return nullptr;
	}
	else
	{
		return &(*it).second;
	}
	
}

Mesh* DxEngine::getMesh(const std::string& name)
{
	auto it = m_meshes.find(name);
	if (it == m_meshes.end())
	{
		return nullptr;
	}
	else
	{
		return &(*it).second;
	}
}

void DxEngine::drawObjects(ComPtr<ID3D12GraphicsCommandList> commandList, RenderObjectDx* first, int count)
{
	XMMATRIX view = m_camera.GetViewMatrix();
	//XMMATRIX projection = m_camera.GetProjectionMatrix(0.8f, m_aspectRatio);
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvsrvHandle(m_cbvsrvHeap->GetGPUDescriptorHandleForHeapStart(), 2, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	//cbvsrvHandle.Offset(m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	int lastMatIndex = INT_MAX;
	for ( int i = 0; i < count; i++)
	{
		RenderObjectDx& object = first[i];
		if (object.material != lastMaterial)
		{
			commandList->SetPipelineState(object.material->m_pipelineState);
			lastMaterial = object.material;
		}

		if (object.matIndex != lastMatIndex)
		{
			lastMatIndex = object.matIndex;
			commandList->SetGraphicsRoot32BitConstant(3, object.matIndex, 0);
		}

		m_commandList->SetGraphicsRootDescriptorTable(1, cbvsrvHandle);
		cbvsrvHandle.Offset(m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		if (object.mesh != lastMesh)
		{
			commandList->IASetVertexBuffers(0, 1, &object.mesh->vertexBufferView());
			lastMesh = object.mesh;
		}

		commandList->DrawInstanced(object.mesh->vertices.size(), 1, 0, 0);

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

		ComPtr<ID3D12Debug1> debugController1;
		if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(&debugController1))))
		{

			debugController1->SetEnableGPUBasedValidation(true);
			debugController1->SetEnableSynchronizedCommandQueueValidation(true);

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
	NAME_D3D12_OBJECT(m_commandQueue);

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
		NAME_D3D12_OBJECT(m_rtvHeap);

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_DESCRIPTOR_HEAP_DESC cbvsrvHeapDesc = {};
		cbvsrvHeapDesc.NumDescriptors = 10000;
		cbvsrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		cbvsrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvsrvHeapDesc, IID_PPV_ARGS(&m_cbvsrvHeap)));
		NAME_D3D12_OBJECT(m_cbvsrvHeap);

		 m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// Describe and create a depth stencil view (DSV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

		// Describe and create a sampler descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
		samplerHeapDesc.NumDescriptors = 1;
		samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&m_samplerHeap)));

	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < FrameCount; n++)
		{
			ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			NAME_D3D12_OBJECT(m_renderTargets[n]);
			rtvHandle.Offset(1, m_rtvDescriptorSize);

			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator[n])));
			NAME_D3D12_OBJECT(m_commandAllocator[n]);
			ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&m_bundleAllocator[n])));
			NAME_D3D12_OBJECT(m_bundleAllocator[n]);
		}
	}

	// Create an empty root signature.
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		CD3DX12_DESCRIPTOR_RANGE1 range[3];
		CD3DX12_ROOT_PARAMETER1 rootParameters[4];

		range[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 0);
		range[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, 0);
		range[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

		rootParameters[0].InitAsDescriptorTable(1, &range[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[1].InitAsDescriptorTable(1, &range[1], D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[2].InitAsDescriptorTable(1, &range[2], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[3].InitAsConstants(1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
		NAME_D3D12_OBJECT(m_rootSignature);
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

		ThrowIfFailed(D3DCompileFromFile(L"../../shaders/mesh.hlsl", nullptr, nullptr, "VSMain", "vs_5_1", compileFlags, 0, &vertexShader, nullptr));
		ThrowIfFailed(D3DCompileFromFile(L"../../shaders/mesh.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		CD3DX12_RASTERIZER_DESC rasterizerStateDesc(D3D12_DEFAULT);
		rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;

		D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = { g_inputElementDescs, _countof(g_inputElementDescs) };

		m_pipelineState = createPipelineState(m_device, inputLayoutDesc, m_rootSignature, vertexShader, pixelShader, rasterizerStateDesc);

		createMaterial(m_rootSignature, m_pipelineState, "defaultMesh");

		ComPtr<ID3DBlob> vertexShaderTextured;
		ComPtr<ID3DBlob> pixelShaderTextured;

		UINT8* pVertexShaderData;
		UINT8* pPixelShaderData;
		UINT vertexShaderDataLength;
		UINT pixelShaderDataLength;

		ThrowIfFailed(ReadDataFromFile(L"C:\\Users\\guill\\Documents\\GitHub\\vulkanRenderer\\bin\\Debug\\shader_vs.cso", &pVertexShaderData, &vertexShaderDataLength));
		ThrowIfFailed(ReadDataFromFile(L"C:\\Users\\guill\\Documents\\GitHub\\vulkanRenderer\\bin\\Debug\\shader_ps.cso", &pPixelShaderData, &pixelShaderDataLength));

// 		ThrowIfFailed(D3DCompileFromFile(L"../../shaders/shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShaderTextured, nullptr));
// 		ThrowIfFailed(D3DCompileFromFile(L"../../shaders/shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShaderTextured, nullptr));

		m_pipelineStateTextured = createPipelineState(m_device, inputLayoutDesc, m_rootSignature, pVertexShaderData, pPixelShaderData, vertexShaderDataLength, pixelShaderDataLength,  rasterizerStateDesc);

		createMaterial(m_rootSignature, m_pipelineStateTextured, "texturedMesh");
	}

}

void DxEngine::LoadAssets()
{
	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
	NAME_D3D12_OBJECT(m_commandList);
	// Create the vertex buffer.
	{
		LoadMesh();
	}

	//Create the constant buffer
	{
		const UINT constantBufferSize = sizeof(MeshSceneConstantBuffer) * 60 * 60; // CB size is required to be 256-byte aligned.

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_constantBuffer)));
		NAME_D3D12_OBJECT(m_constantBuffer);

		// Map and initialize the constant buffer. We don't unmap this until the
		// app closes. Keeping things mapped for the lifetime of the resource is okay.
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
	}

	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	m_device->CreateSampler(&samplerDesc, m_samplerHeap->GetCPUDescriptorHandleForHeapStart());

	// Note: ComPtr's are CPU objects but this resource needs to stay in scope until
	// the command list that references it has finished executing on the GPU.
	// We will flush the GPU at the end of this method to ensure the resource is not
	// prematurely destroyed.
	ComPtr<ID3D12Resource> textureUploadHeap;

	//create Texture
	{
		// Describe and create a Texture2D.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = TextureWidth;
		textureDesc.Height = TextureHeight;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_texture)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);

		//Create the GPU upload buffer
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&textureUploadHeap)));
		NAME_D3D12_OBJECT(m_texture);
		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the Texture2D.
		std::vector<UINT8> texture = GenerateTextureData();

		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = &texture[0];
		textureData.RowPitch = TextureWidth * TexturePixelSize;
		textureData.SlicePitch = textureData.RowPitch * TextureHeight;

		UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = textureDesc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		CD3DX12_CPU_DESCRIPTOR_HANDLE cbvsrvHandle(m_cbvsrvHeap->GetCPUDescriptorHandleForHeapStart());
		m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, cbvsrvHandle);

		LoadImages();
	}

	// Create the depth stencil view.
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
		depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE), // Performance tip: Deny shader resource access to resources that don't need shader resource views.
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0), // Performance tip: Tell the runtime at resource creation the desired clear value.
			IID_PPV_ARGS(&m_depthStencil)
		));

		NAME_D3D12_OBJECT(m_depthStencil);

		m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

// 	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, m_bundleAllocator[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_bundle)));
// 	m_bundle->SetGraphicsRootSignature(m_rootSignature.Get());
// 	m_bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
// 	m_bundle->IASetVertexBuffers(0, 1, &m_vertexBufferView);
// 	m_bundle->DrawInstanced(m_monkeyMesh.m_vertices.size(), 1, 0, 0);
// 	NAME_D3D12_OBJECT(m_bundle);
// 	ThrowIfFailed(m_bundle->Close());


	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		ThrowIfFailed(m_device->CreateFence(m_fenceValue[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValue[m_frameIndex]++;

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForGpu();
	}
}

void DxEngine::LoadMesh()
{
	m_triangleMesh.vertices.resize(3);

	//vertex positions
	m_triangleMesh.vertices[0].position = { 0.0f, 0.25f * m_aspectRatio, 0.0f };
	m_triangleMesh.vertices[1].position = { 0.25f, -0.25f * m_aspectRatio, 0.0f };
	m_triangleMesh.vertices[2].position = { -0.25f, -0.25f * m_aspectRatio, 0.0f };

	m_triangleMesh.vertices[0].normal = { 1.0f, 0.0f, 0.0f };
	m_triangleMesh.vertices[1].normal = { 1.0f, 0.0f, 0.0f };
	m_triangleMesh.vertices[2].normal = { 1.0f, 0.0f, 0.0f };

	//vertex color, all green
	m_triangleMesh.vertices[0].color = { 1.0f, 0.0f, 0.0f }; //pure green
	m_triangleMesh.vertices[1].color = { 0.0f, 1.0f, 0.0f }; //pure green
	m_triangleMesh.vertices[2].color = { 0.0f, 0.0f, 1.0f }; //pure green

	m_triangleMesh.vertices[0].uv = { 0.5f, 0.0f };
	m_triangleMesh.vertices[1].uv = { 1.0f, 1.0f }; 
	m_triangleMesh.vertices[2].uv = { 0.0f, 1.0f };

	Mesh monkeyMesh = {};
	m_monkeyMesh.LoadFromObj("../../assets/monkey_smooth.obj");

	m_lostEmpire.LoadFromObj("../../assets/lost_empire.obj");

	
	UploadMesh(m_triangleMesh);
	
	UploadMesh(m_monkeyMesh);

	UploadMesh(m_lostEmpire);
	m_meshes["monkey"] = m_monkeyMesh;
	m_meshes["triangle"] = m_triangleMesh;
	m_meshes["empire"] = m_lostEmpire;

}

void DxEngine::LoadImages()
{
	dxutils::loadImageFromFile(*this, "../../assets/lost_empire-RGBA.png", m_lostEmpireTexture);
	
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescD = {};
	srvDescD.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescD.Format = m_lostEmpireTexture.Image->GetDesc().Format;
	srvDescD.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDescD.Texture2D.MipLevels = 1;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvsrvHandle(m_cbvsrvHeap->GetCPUDescriptorHandleForHeapStart());
	cbvsrvHandle.Offset(m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	m_device->CreateShaderResourceView(m_lostEmpireTexture.Image.Get(), &srvDescD, cbvsrvHandle);
	 
	m_loadedTextures["empire_diffuse"] = m_lostEmpireTexture;
}

void DxEngine::UploadMesh(Mesh& mesh)
{

	const UINT vertexBufferSize = mesh.vertices.size() * sizeof(Vertex);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&mesh.vertexBuffer)));
		NAME_D3D12_OBJECT(mesh.vertexBuffer);

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mesh.vertexBufferUploadHeap)));
		NAME_D3D12_OBJECT(mesh.vertexBufferUploadHeap);
		

		// Copy data to the intermediate upload heap and then schedule a copy 
		// from the upload heap to the vertex buffer.
		D3D12_SUBRESOURCE_DATA vertexData = {};
		vertexData.pData = reinterpret_cast<void*>(mesh.vertices.data());
		vertexData.RowPitch = vertexBufferSize;
		vertexData.SlicePitch = vertexData.RowPitch;

		UpdateSubresources<1>(m_commandList.Get(), mesh.vertexBuffer.Get(), mesh.vertexBufferUploadHeap.Get(), 0, 0, 1, &vertexData);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mesh.vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
}

void DxEngine::InitScene()
{
	UINT64 cbOffset = 0;
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvsrvHandle(m_cbvsrvHeap->GetCPUDescriptorHandleForHeapStart(), 2, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	//cbvsrvHandle.Offset(m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	
	RenderObjectDx monkey;
	monkey.mesh = getMesh("monkey");
	monkey.material = getMaterial("defaultMesh");
	monkey.tranformMatrix = XMMatrixIdentity();
	monkey.matIndex = INT_MAX;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress() + cbOffset;
	cbvDesc.SizeInBytes = sizeof(MeshSceneConstantBuffer);
	cbOffset += cbvDesc.SizeInBytes;
	m_device->CreateConstantBufferView(&cbvDesc, cbvsrvHandle);
	cbvsrvHandle.Offset(m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	m_renderables.push_back(monkey);

	for ( int x = -20; x <= 20; x++)
	{
		for (int y  = -20; y <= 20; y++)
		{
			RenderObjectDx tri;
			tri.mesh = getMesh("triangle");
			tri.material = getMaterial("texturedMesh");
			tri.matIndex = 0;
			XMMATRIX translate = XMMatrixTranslation(x, 0, y);
			XMMATRIX scale = XMMatrixScaling(0.2, 0.2, 0.2);
			tri.tranformMatrix = translate * scale;

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress() + cbOffset;
			cbvDesc.SizeInBytes = sizeof(MeshSceneConstantBuffer);
			cbOffset += cbvDesc.SizeInBytes;
			m_device->CreateConstantBufferView(&cbvDesc, cbvsrvHandle);
			cbvsrvHandle.Offset(m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
			m_renderables.push_back(tri);
		}
	}

	RenderObjectDx map;
	map.mesh = getMesh("empire");
	map.material = getMaterial("texturedMesh");
	map.matIndex = 1;
	XMMATRIX translate = XMMatrixTranslation(0, 0, -100);
	map.tranformMatrix = translate;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDescMap = {};
	cbvDescMap.BufferLocation = m_constantBuffer->GetGPUVirtualAddress() + cbOffset;
	cbvDescMap.SizeInBytes = sizeof(MeshSceneConstantBuffer);
	cbOffset += cbvDesc.SizeInBytes;
	m_device->CreateConstantBufferView(&cbvDescMap, cbvsrvHandle);
	cbvsrvHandle.Offset(m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
	
	m_renderables.push_back(map);

}

void DxEngine::InitImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui_ImplSDL2_InitForD3D(window);
	ImGui_ImplDX12_Init(m_device.Get(), FRAME_OVERLAP, DXGI_FORMAT_R8G8B8A8_UNORM,
		m_cbvsrvHeap.Get(),
		m_cbvsrvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_cbvsrvHeap->GetGPUDescriptorHandleForHeapStart());
}

// Wait for pending GPU work to complete.
void DxEngine::WaitForGpu()
{
	// Schedule a Signal command in the queue.
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue[m_frameIndex]));

	// Wait until the fence has been processed.
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue[m_frameIndex], m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

	// Increment the fence value for the current frame.
	m_fenceValue[m_frameIndex]++;
}

void DxEngine::MoveToNextFrame()
{
	// Signal and increment the fence value.
	const UINT64 currentFrameValue = m_fenceValue[m_frameIndex];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFrameValue));
	
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	
	if (m_fence->GetCompletedValue() < m_fenceValue[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	m_fenceValue[m_frameIndex] = currentFrameValue + 1;
}

void DxEngine::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocator[m_frameIndex]->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator[m_frameIndex].Get(), m_pipelineState.Get()));

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

	ID3D12DescriptorHeap* ppHeaps[] = { m_cbvsrvHeap.Get(), m_samplerHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvsrvHeap->GetGPUDescriptorHandleForHeapStart());
	m_commandList->SetGraphicsRootDescriptorTable(2, m_samplerHeap->GetGPUDescriptorHandleForHeapStart());
// 	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvsrvHandle(m_cbvsrvHeap->GetGPUDescriptorHandleForHeapStart());
// 	cbvsrvHandle.Offset(m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
// 	m_commandList->SetGraphicsRootDescriptorTable(1, cbvsrvHandle);

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	drawObjects(m_commandList, m_renderables.data(), m_renderables.size());
// 	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
// 	m_commandList->DrawInstanced(m_monkeyMesh.m_vertices.size(), 1, 0, 0);
	//m_commandList->ExecuteBundle(m_bundle.Get());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	
	ThrowIfFailed(m_commandList->Close());

}

// Generate a simple black and white checkerboard texture.
std::vector<UINT8> DxEngine::GenerateTextureData()
{
	const UINT rowPitch = TextureWidth * TexturePixelSize;
	const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
	const UINT cellHeight = TextureWidth >> 3;    // The height of a cell in the checkerboard texture.
	const UINT textureSize = rowPitch * TextureHeight;

	std::vector<UINT8> data(textureSize);
	UINT8* pData = &data[0];

	for (UINT n = 0; n < textureSize; n += TexturePixelSize)
	{
		UINT x = n % rowPitch;
		UINT y = n / rowPitch;
		UINT i = x / cellPitch;
		UINT j = y / cellHeight;

		if (i % 2 == j % 2)
		{
			pData[n] = 0x00;        // R
			pData[n + 1] = 0x00;    // G
			pData[n + 2] = 0x00;    // B
			pData[n + 3] = 0xff;    // A
		}
		else
		{
			pData[n] = 0xff;        // R
			pData[n + 1] = 0xff;    // G
			pData[n + 2] = 0xff;    // B
			pData[n + 3] = 0xff;    // A
		}
	}

	return data;
}
