#pragma once
#include <d3d12.h>
#include <wrl/module.h>
#include "d3dx12.h"

using namespace Microsoft::WRL;

ComPtr<ID3D12PipelineState> createPipelineState(ComPtr<ID3D12Device> device,
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc,
	ComPtr<ID3D12RootSignature> rootSignature, 
	ComPtr<ID3DBlob> vertexShader,
	ComPtr<ID3DBlob> pixelShader, 
	CD3DX12_RASTERIZER_DESC rasterizerStateDesc,
	CD3DX12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT), 
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));

ComPtr<ID3D12PipelineState> createPipelineState(ComPtr<ID3D12Device> device, 
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc, 
	ComPtr<ID3D12RootSignature> rootSignature, 
	UINT8* pVertexShaderData, 
	UINT8* pPixelShaderData, 
	UINT vertexShaderDataLength, 
	UINT pixelShaderDataLength, 
	CD3DX12_RASTERIZER_DESC rasterizerStateDesc, 
	CD3DX12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT), 
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT));