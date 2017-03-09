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
// Convenience macro for error handling
//---------------------------------------------------------------------------------------

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
		ThrowIfFailed(device->CreateBuffer(&desc, data, buffer.GetAddressOf()));
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
    XMGLOBALCONST XMVECTORF32 AliceBlue            = {0.941176534f, 0.972549081f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 AntiqueWhite         = {0.980392218f, 0.921568692f, 0.843137324f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Aqua                 = {0.000000000f, 1.000000000f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Aquamarine           = {0.498039246f, 1.000000000f, 0.831372619f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Azure                = {0.941176534f, 1.000000000f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Beige                = {0.960784376f, 0.960784376f, 0.862745166f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Bisque               = {1.000000000f, 0.894117713f, 0.768627524f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Black                = {0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 BlanchedAlmond       = {1.000000000f, 0.921568692f, 0.803921640f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Blue                 = {0.000000000f, 0.000000000f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 BlueViolet           = {0.541176498f, 0.168627456f, 0.886274576f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Brown                = {0.647058845f, 0.164705887f, 0.164705887f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 BurlyWood            = {0.870588303f, 0.721568644f, 0.529411793f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 CadetBlue            = {0.372549027f, 0.619607866f, 0.627451003f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Chartreuse           = {0.498039246f, 1.000000000f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Chocolate            = {0.823529482f, 0.411764741f, 0.117647067f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Coral                = {1.000000000f, 0.498039246f, 0.313725501f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 CornflowerBlue       = {0.392156899f, 0.584313750f, 0.929411829f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Cornsilk             = {1.000000000f, 0.972549081f, 0.862745166f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Crimson              = {0.862745166f, 0.078431375f, 0.235294133f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Cyan                 = {0.000000000f, 1.000000000f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkBlue             = {0.000000000f, 0.000000000f, 0.545098066f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkCyan             = {0.000000000f, 0.545098066f, 0.545098066f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkGoldenrod        = {0.721568644f, 0.525490224f, 0.043137256f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkGray             = {0.662745118f, 0.662745118f, 0.662745118f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkGreen            = {0.000000000f, 0.392156899f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkKhaki            = {0.741176486f, 0.717647076f, 0.419607878f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkMagenta          = {0.545098066f, 0.000000000f, 0.545098066f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkOliveGreen       = {0.333333343f, 0.419607878f, 0.184313729f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkOrange           = {1.000000000f, 0.549019635f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkOrchid           = {0.600000024f, 0.196078449f, 0.800000072f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkRed              = {0.545098066f, 0.000000000f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkSalmon           = {0.913725555f, 0.588235319f, 0.478431404f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkSeaGreen         = {0.560784340f, 0.737254918f, 0.545098066f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkSlateBlue        = {0.282352954f, 0.239215702f, 0.545098066f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkSlateGray        = {0.184313729f, 0.309803933f, 0.309803933f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkTurquoise        = {0.000000000f, 0.807843208f, 0.819607913f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DarkViolet           = {0.580392182f, 0.000000000f, 0.827451050f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DeepPink             = {1.000000000f, 0.078431375f, 0.576470613f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DeepSkyBlue          = {0.000000000f, 0.749019623f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DimGray              = {0.411764741f, 0.411764741f, 0.411764741f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 DodgerBlue           = {0.117647067f, 0.564705908f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Firebrick            = {0.698039234f, 0.133333340f, 0.133333340f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 FloralWhite          = {1.000000000f, 0.980392218f, 0.941176534f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 ForestGreen          = {0.133333340f, 0.545098066f, 0.133333340f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Fuchsia              = {1.000000000f, 0.000000000f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Gainsboro            = {0.862745166f, 0.862745166f, 0.862745166f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 GhostWhite           = {0.972549081f, 0.972549081f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Gold                 = {1.000000000f, 0.843137324f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Goldenrod            = {0.854902029f, 0.647058845f, 0.125490203f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Gray                 = {0.501960814f, 0.501960814f, 0.501960814f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Green                = {0.000000000f, 0.501960814f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 GreenYellow          = {0.678431392f, 1.000000000f, 0.184313729f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Honeydew             = {0.941176534f, 1.000000000f, 0.941176534f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 HotPink              = {1.000000000f, 0.411764741f, 0.705882370f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 IndianRed            = {0.803921640f, 0.360784322f, 0.360784322f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Indigo               = {0.294117659f, 0.000000000f, 0.509803951f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Ivory                = {1.000000000f, 1.000000000f, 0.941176534f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Khaki                = {0.941176534f, 0.901960850f, 0.549019635f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Lavender             = {0.901960850f, 0.901960850f, 0.980392218f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LavenderBlush        = {1.000000000f, 0.941176534f, 0.960784376f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LawnGreen            = {0.486274540f, 0.988235354f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LemonChiffon         = {1.000000000f, 0.980392218f, 0.803921640f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightBlue            = {0.678431392f, 0.847058892f, 0.901960850f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightCoral           = {0.941176534f, 0.501960814f, 0.501960814f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightCyan            = {0.878431439f, 1.000000000f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightGoldenrodYellow = {0.980392218f, 0.980392218f, 0.823529482f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightGreen           = {0.564705908f, 0.933333397f, 0.564705908f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightGray            = {0.827451050f, 0.827451050f, 0.827451050f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightPink            = {1.000000000f, 0.713725507f, 0.756862819f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightSalmon          = {1.000000000f, 0.627451003f, 0.478431404f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightSeaGreen        = {0.125490203f, 0.698039234f, 0.666666687f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightSkyBlue         = {0.529411793f, 0.807843208f, 0.980392218f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightSlateGray       = {0.466666698f, 0.533333361f, 0.600000024f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightSteelBlue       = {0.690196097f, 0.768627524f, 0.870588303f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LightYellow          = {1.000000000f, 1.000000000f, 0.878431439f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Lime                 = {0.000000000f, 1.000000000f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 LimeGreen            = {0.196078449f, 0.803921640f, 0.196078449f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Linen                = {0.980392218f, 0.941176534f, 0.901960850f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Magenta              = {1.000000000f, 0.000000000f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Maroon               = {0.501960814f, 0.000000000f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MediumAquamarine     = {0.400000036f, 0.803921640f, 0.666666687f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MediumBlue           = {0.000000000f, 0.000000000f, 0.803921640f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MediumOrchid         = {0.729411781f, 0.333333343f, 0.827451050f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MediumPurple         = {0.576470613f, 0.439215720f, 0.858823597f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MediumSeaGreen       = {0.235294133f, 0.701960802f, 0.443137288f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MediumSlateBlue      = {0.482352972f, 0.407843173f, 0.933333397f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MediumSpringGreen    = {0.000000000f, 0.980392218f, 0.603921592f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MediumTurquoise      = {0.282352954f, 0.819607913f, 0.800000072f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MediumVioletRed      = {0.780392230f, 0.082352944f, 0.521568656f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MidnightBlue         = {0.098039225f, 0.098039225f, 0.439215720f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MintCream            = {0.960784376f, 1.000000000f, 0.980392218f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 MistyRose            = {1.000000000f, 0.894117713f, 0.882353008f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Moccasin             = {1.000000000f, 0.894117713f, 0.709803939f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 NavajoWhite          = {1.000000000f, 0.870588303f, 0.678431392f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Navy                 = {0.000000000f, 0.000000000f, 0.501960814f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 OldLace              = {0.992156923f, 0.960784376f, 0.901960850f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Olive                = {0.501960814f, 0.501960814f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 OliveDrab            = {0.419607878f, 0.556862772f, 0.137254909f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Orange               = {1.000000000f, 0.647058845f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 OrangeRed            = {1.000000000f, 0.270588249f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Orchid               = {0.854902029f, 0.439215720f, 0.839215755f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 PaleGoldenrod        = {0.933333397f, 0.909803987f, 0.666666687f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 PaleGreen            = {0.596078455f, 0.984313786f, 0.596078455f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 PaleTurquoise        = {0.686274529f, 0.933333397f, 0.933333397f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 PaleVioletRed        = {0.858823597f, 0.439215720f, 0.576470613f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 PapayaWhip           = {1.000000000f, 0.937254965f, 0.835294187f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 PeachPuff            = {1.000000000f, 0.854902029f, 0.725490212f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Peru                 = {0.803921640f, 0.521568656f, 0.247058839f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Pink                 = {1.000000000f, 0.752941251f, 0.796078503f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Plum                 = {0.866666734f, 0.627451003f, 0.866666734f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 PowderBlue           = {0.690196097f, 0.878431439f, 0.901960850f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Purple               = {0.501960814f, 0.000000000f, 0.501960814f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Red                  = {1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 RosyBrown            = {0.737254918f, 0.560784340f, 0.560784340f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 RoyalBlue            = {0.254901975f, 0.411764741f, 0.882353008f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 SaddleBrown          = {0.545098066f, 0.270588249f, 0.074509807f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Salmon               = {0.980392218f, 0.501960814f, 0.447058856f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 SandyBrown           = {0.956862807f, 0.643137276f, 0.376470625f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 SeaGreen             = {0.180392161f, 0.545098066f, 0.341176480f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 SeaShell             = {1.000000000f, 0.960784376f, 0.933333397f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Sienna               = {0.627451003f, 0.321568638f, 0.176470593f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Silver               = {0.752941251f, 0.752941251f, 0.752941251f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 SkyBlue              = {0.529411793f, 0.807843208f, 0.921568692f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 SlateBlue            = {0.415686309f, 0.352941185f, 0.803921640f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 SlateGray            = {0.439215720f, 0.501960814f, 0.564705908f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Snow                 = {1.000000000f, 0.980392218f, 0.980392218f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 SpringGreen          = {0.000000000f, 1.000000000f, 0.498039246f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 SteelBlue            = {0.274509817f, 0.509803951f, 0.705882370f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Tan                  = {0.823529482f, 0.705882370f, 0.549019635f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Teal                 = {0.000000000f, 0.501960814f, 0.501960814f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Thistle              = {0.847058892f, 0.749019623f, 0.847058892f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Tomato               = {1.000000000f, 0.388235331f, 0.278431386f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Transparent          = {0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f};
    XMGLOBALCONST XMVECTORF32 Turquoise            = {0.250980407f, 0.878431439f, 0.815686345f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Violet               = {0.933333397f, 0.509803951f, 0.933333397f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Wheat                = {0.960784376f, 0.870588303f, 0.701960802f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 White                = {1.000000000f, 1.000000000f, 1.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 WhiteSmoke           = {0.960784376f, 0.960784376f, 0.960784376f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 Yellow               = {1.000000000f, 1.000000000f, 0.000000000f, 1.000000000f};
    XMGLOBALCONST XMVECTORF32 YellowGreen          = {0.603921592f, 0.803921640f, 0.196078449f, 1.000000000f};

}; // namespace Colors

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

struct Texture
{
	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Resource = nullptr;
};


#endif // D3DUTIL_H