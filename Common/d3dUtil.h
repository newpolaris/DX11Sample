//***************************************************************************************
// d3dUtil.h by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#ifndef D3DUTIL_H
#define D3DUTIL_H

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
 
#include <d3dx11.h>
#include "d3dx11Effect.h"
#include <xnamath.h>
#include <dxerr.h>
#include <memory>
#include <cassert>
#include <unordered_map>
#include <ctime>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <wrl.h>
#include <array>
#include "D3Dcompiler.h"
#include "MathHelper.h"
#include "LightHelper.h"
//---------------------------------------------------------------------------------------
// Simple d3d error checker for book demos.
//---------------------------------------------------------------------------------------

#if defined(DEBUG) | defined(_DEBUG)
	#ifndef HR
	#define HR(x)                                              \
	{                                                          \
		HRESULT hr = (x);                                      \
		if(FAILED(hr))                                         \
		{                                                      \
			DXTrace(__FILEW__, (DWORD)__LINE__, hr, L#x, true); \
		}                                                      \
	}
	#endif

#else
	#ifndef HR
	#define HR(x) (x)
	#endif
#endif 


//---------------------------------------------------------------------------------------
// Convenience macro for releasing COM objects.
//---------------------------------------------------------------------------------------

#define ReleaseCOM(x) { if(x){ x->Release(); x = 0; } }

//---------------------------------------------------------------------------------------
// Convenience macro for deleting objects.
//---------------------------------------------------------------------------------------

#define SafeDelete(x) { delete x; x = 0; }

//---------------------------------------------------------------------------------------
// Utility classes.
//---------------------------------------------------------------------------------------

class d3dHelper
{
public:
	///<summary>
	/// 
	/// Does not work with compressed formats.
	///</summary>
	static ID3D11ShaderResourceView* CreateTexture2DArraySRV(
		ID3D11Device* device, ID3D11DeviceContext* context,
		std::vector<std::wstring>& filenames,
		DXGI_FORMAT format = DXGI_FORMAT_FROM_FILE,
		UINT filter = D3DX11_FILTER_NONE,
		UINT mipFilter = D3DX11_FILTER_LINEAR);

	static ID3D11ShaderResourceView* CreateRandomTexture1DSRV(ID3D11Device* device);

	static D3D11_BUFFER_DESC SetDefaultConstantBuffer(
		UINT size,
		bool dynamic)
	{
		D3D11_BUFFER_DESC state;
		// Create the settings for a constant buffer.  This includes setting the 
		// constant buffer flag, allowing the CPU write access, and a dynamic usage.
		// Additional flags may be set as needed.

		state.ByteWidth = size;
		state.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		state.MiscFlags = 0;
		state.StructureByteStride = 0;

		if (dynamic)
		{
			state.Usage = D3D11_USAGE_DYNAMIC;
			state.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		}
		else
		{
			state.Usage = D3D11_USAGE_IMMUTABLE;
			state.CPUAccessFlags = 0;
		}
		return state;
	}

	static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateSRV(
		ID3D11Device* device,
		ID3D11Resource *pResource,
		int start, int len)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		// CAUTION: 이것도 FirstElement 와 같은 의미 byte offset이 아닌 숫자일 듯
		desc.Buffer.ElementOffset = start;
		// CAUTION: num of element 랑 같은 의미, sizeof(XMMATRIX)가 들어가면 
		// 숫자가 맞지 않기에 invalidarg 오류 발생 
		desc.Buffer.ElementWidth = len;

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
		HR(device->CreateShaderResourceView(pResource, &desc, view.GetAddressOf()));
		return view;
	}

	static Microsoft::WRL::ComPtr<ID3D11Buffer> CreateConstantBuffer(
		ID3D11Device* device,
		int size)
	{
		auto Desc = SetDefaultConstantBuffer(size, true);
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		HR(device->CreateBuffer(
			&Desc,
			nullptr,
			buffer.GetAddressOf()));
		return buffer;
	}
	static Microsoft::WRL::ComPtr<ID3D11Buffer> CreateStructuredBuffer(
		ID3D11Device* device,
		int count,
		int structsize,
		bool CPUWritable,
		bool GPUWritable,
		D3D11_SUBRESOURCE_DATA* data) 
	{
		D3D11_BUFFER_DESC desc;
		desc.ByteWidth = count * structsize;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = structsize;

		if (!CPUWritable && !GPUWritable)
		{
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.Usage = D3D11_USAGE_IMMUTABLE;
			desc.CPUAccessFlags = 0;
		}
		else if (CPUWritable && !GPUWritable)
		{
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		}
		// UNDONE: with error, unordered access

		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		HR(device->CreateBuffer(&desc, data, buffer.GetAddressOf()));
		return buffer;
	}

	static Microsoft::WRL::ComPtr<ID3D11Buffer> CreateVertexBuffer(
		ID3D11Device* device,
		void* data,
		int size)
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		D3D11_BUFFER_DESC vbd;
		vbd.Usage = D3D11_USAGE_IMMUTABLE;
		vbd.ByteWidth = size;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.CPUAccessFlags = 0;
		vbd.MiscFlags = 0;
		vbd.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA vinitData;
		vinitData.pSysMem = data;
		HR(device->CreateBuffer(
			&vbd, &vinitData, buffer.GetAddressOf()));
		return buffer;
	}

	static Microsoft::WRL::ComPtr<ID3D11Buffer> CreateIndexBuffer(
		ID3D11Device* device,
		void* data,
		int size)
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		D3D11_BUFFER_DESC ibd;
		ibd.Usage = D3D11_USAGE_IMMUTABLE;
		ibd.ByteWidth = size;
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibd.CPUAccessFlags = 0;
		ibd.MiscFlags = 0;
		ibd.StructureByteStride = 0;
		D3D11_SUBRESOURCE_DATA iinitData;
		iinitData.pSysMem = data;
		HR(device->CreateBuffer(
			&ibd, &iinitData, buffer.GetAddressOf()));
		return buffer;
	}
};

class TextHelper
{
public:

	template<typename T>
	static D3DX11INLINE std::wstring ToString(const T& s)
	{
		std::wostringstream oss;
		oss << s;

		return oss.str();
	}

	template<typename T>
	static D3DX11INLINE T FromString(const std::wstring& s)
	{
		T x;
		std::wistringstream iss(s);
		iss >> x;

		return x;
	}
};

// Order: left, right, bottom, top, near, far.
void ExtractFrustumPlanes(XMFLOAT4 planes[6], CXMMATRIX M);


// #define XMGLOBALCONST extern CONST __declspec(selectany)
//   1. extern so there is only one copy of the variable, and not a separate
//      private copy in each .obj.
//   2. __declspec(selectany) so that the compiler does not complain about
//      multiple definitions in a .cpp file (it can pick anyone and discard 
//      the rest because they are constant--all the same).

namespace Colors
{
	XMGLOBALCONST XMVECTORF32 White     = {1.0f, 1.0f, 1.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Black     = {0.0f, 0.0f, 0.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Red       = {1.0f, 0.0f, 0.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Green     = {0.0f, 1.0f, 0.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Blue      = {0.0f, 0.0f, 1.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Yellow    = {1.0f, 1.0f, 0.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Cyan      = {0.0f, 1.0f, 1.0f, 1.0f};
	XMGLOBALCONST XMVECTORF32 Magenta   = {1.0f, 0.0f, 1.0f, 1.0f};

	XMGLOBALCONST XMVECTORF32 Silver    = {0.75f, 0.75f, 0.75f, 1.0f};
	XMGLOBALCONST XMVECTORF32 LightSteelBlue = {0.69f, 0.77f, 0.87f, 1.0f};
}

///<summary>
/// Utility class for converting between types and formats.
///</summary>
class Convert
{
public:
	///<summary>
	/// Converts XMVECTOR to XMCOLOR, where XMVECTOR represents a color.
	///</summary>
	static D3DX11INLINE XMCOLOR ToXmColor(FXMVECTOR v)
	{
		XMCOLOR dest;
		XMStoreColor(&dest, v);
		return dest;
	}

	///<summary>
	/// Converts XMVECTOR to XMFLOAT4, where XMVECTOR represents a color.
	///</summary>
	static D3DX11INLINE XMFLOAT4 ToXmFloat4(FXMVECTOR v)
	{
		XMFLOAT4 dest;
		XMStoreFloat4(&dest, v);
		return dest;
	}

	static D3DX11INLINE UINT ArgbToAbgr(UINT argb)
	{
		BYTE A = (argb >> 24) & 0xff;
		BYTE R = (argb >> 16) & 0xff;
		BYTE G = (argb >>  8) & 0xff;
		BYTE B = (argb >>  0) & 0xff;

		return (A << 24) | (B << 16) | (G << 8) | (R << 0);
	}

};

class DxException
{
public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

    std::wstring ToString()const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

// Defines a subrange of geometry in a MeshGeometry.  This is for when multiple
// geometries are stored in one vertex and index buffer.  It provides the offsets
// and data needed to draw a subset of geometry stores in the vertex and index 
// buffers so that we can implement the technique described by Figure 6.3.
struct SubmeshGeometry
{
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT BaseVertexLocation = 0;

    // Bounding box of the geometry defined by this submesh. 
    // This is used in later chapters of the book.
	// DirectX::BoundingBox Bounds;
};

struct MeshGeometry
{
	// Give it a name so we can look it up by name.
	std::string Name;

	// System memory copies.  Use Blobs because the vertex/index format can be generic.
	// It is up to the client to cast appropriately.  
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU  = nullptr;

	Microsoft::WRL::ComPtr<ID3D11Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Resource> IndexBufferGPU = nullptr;

	Microsoft::WRL::ComPtr<ID3D11Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D11Resource> IndexBufferUploader = nullptr;

    // Data about the buffers.
	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	// A MeshGeometry may store multiple geometries in one vertex/index buffer.
	// Use this container to define the Submesh geometries so we can draw
	// the Submeshes individually.
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	/*
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;

		return ibv;
	}
	*/
};

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif


#endif // D3DUTIL_H