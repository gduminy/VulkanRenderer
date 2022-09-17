#pragma once
#include <wrl/module.h>
#include <d3d12.h>
using namespace Microsoft::WRL;

inline void SetName(ID3D12Object* pObject, LPCWSTR name)
{
	pObject->SetName(name);
}

// Naming helper for ComPtr<T>.
// Assigns the name of the variable as the name of the object.
// The indexed variant will include the index in the name of the object.
#define NAME_D3D12_OBJECT(x) SetName((x).Get(), L#x)