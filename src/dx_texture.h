#pragma once
#include <d3d12.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

class DxEngine;

struct Texture
{
	ComPtr<ID3D12Resource> Image;
	ComPtr<ID3D12Resource> imageUploadHeap;
};

namespace dxutils
{
	bool loadImageFromFile(DxEngine& engine, const char* file, Texture& image);
}

