#include "dx_texture.h"
#include <iostream>
#include <d3d12.h>
#include "d3dx12.h"
#include "Helper.h"
#include "dx_engine.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>



bool dxutils::loadImageFromFile(DxEngine& engine, const char* file, Texture& image)
{
	int texWidth, texHeight, texChannels;

	stbi_uc* pixels = stbi_load(file, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels) {
		std::cout << "Failed to load texture file " << file << std::endl;
		return false;
	}

	D3D12_RESOURCE_DESC textureDesc = {};
	textureDesc.MipLevels = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.Width = texWidth;
	textureDesc.Height = texHeight;
	textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	textureDesc.DepthOrArraySize = 1;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	ThrowIfFailed(engine.m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&image.Image)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(image.Image.Get(), 0, 1);

	//Create the GPU upload buffer
	ThrowIfFailed(engine.m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&image.imageUploadHeap)));
	NAME_D3D12_OBJECT(image.Image);

	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = pixels;
	textureData.RowPitch = texWidth * texChannels;
	textureData.SlicePitch = textureData.RowPitch * texHeight;

	UpdateSubresources(engine.m_commandList.Get(), image.Image.Get(), image.imageUploadHeap.Get(), 0, 0, 1, &textureData);
	engine.m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(image.Image.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	return true;
}

