
#include "dx_initializers.h"
#include "Helper.h"


ComPtr<ID3D12PipelineState> createPipelineState(ComPtr<ID3D12Device> device, D3D12_INPUT_LAYOUT_DESC inputLayoutDesc, ComPtr<ID3D12RootSignature> rootSignature, ComPtr<ID3DBlob> vertexShader, ComPtr<ID3DBlob> pixelShader, CD3DX12_RASTERIZER_DESC rasterizerStateDesc, CD3DX12_BLEND_DESC blendDesc /*= CD3DX12_BLEND_DESC(D3D12_DEFAULT)*/, CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc /*= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT)*/)
{
	ComPtr<ID3D12PipelineState> pipelineState;
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
	psoDesc.RasterizerState = rasterizerStateDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
	NAME_D3D12_OBJECT(pipelineState);

	return pipelineState;
}

ComPtr<ID3D12PipelineState> createPipelineState(ComPtr<ID3D12Device> device, D3D12_INPUT_LAYOUT_DESC inputLayoutDesc, ComPtr<ID3D12RootSignature> rootSignature, UINT8* pVertexShaderData, UINT8* pPixelShaderData, UINT vertexShaderDataLength, UINT pixelShaderDataLength,  CD3DX12_RASTERIZER_DESC rasterizerStateDesc, CD3DX12_BLEND_DESC blendDesc /*= CD3DX12_BLEND_DESC(D3D12_DEFAULT)*/, CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc /*= CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT)*/)
{
	ComPtr<ID3D12PipelineState> pipelineState;
	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderData, vertexShaderDataLength);
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderData, pixelShaderDataLength);
	psoDesc.RasterizerState = rasterizerStateDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
	NAME_D3D12_OBJECT(pipelineState);

	return pipelineState;
}

