#pragma once
#include <directxmath.h>
#include <vector>
#include <d3d12.h>
#include <wrl/module.h>

using namespace DirectX;
using namespace Microsoft::WRL;

static D3D12_INPUT_ELEMENT_DESC g_inputElementDescs[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT3 color;
	XMFLOAT2 uv;
};

struct Mesh
{
	std::vector<Vertex> vertices;
	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> vertexBufferUploadHeap;
	//ComPtr<D3D12_VERTEX_BUFFER_VIEW> vertexBufferView;

	bool LoadFromObj(const char* filename);
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView();
};