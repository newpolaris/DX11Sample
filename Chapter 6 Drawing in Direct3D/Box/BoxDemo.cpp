//***************************************************************************************
// BoxDemo.cpp by Frank Luna (C) 2011 All Rights Reserved.
//
// Demonstrates rendering a colored box.
//
// Controls:
//		Hold the left mouse button down and move the mouse to rotate.
//      Hold the right mouse button down to zoom in and out.
//
//***************************************************************************************

#include "d3dApp.h"
#include "d3dx11Effect.h"
#include "MathHelper.h"

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

class BoxApp : public D3DApp
{
public:
	BoxApp(HINSTANCE hInstance);
	~BoxApp();

	bool Init();
	void OnResize();
	void UpdateScene(float dt);
	void DrawScene(); 

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);

private:
	void BuildGeometryBuffers();
	void BuildFX();
	void BuildVertexLayout();

private:
	ComPtr<ID3D11VertexShader> pVertexShader;
	ComPtr<ID3DBlob> pVertexCompiledShader;

	ComPtr<ID3D11PixelShader> pPixelShader;
	ComPtr<ID3DBlob> pPixelCompiledShader;
	
	ComPtr<ID3D11InputLayout> pLayout;
	ComPtr<ID3D11Buffer> buffer;

	ID3D11Buffer* mBoxVB;
	ID3D11Buffer* mBoxIB;

	ComPtr<ID3D11InputLayout> mInputLayout;

	XMFLOAT4X4 mWorld;
	XMFLOAT4X4 mView;
	XMFLOAT4X4 mProj;

	float mTheta;
	float mPhi;
	float mRadius;

	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
				   PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

	BoxApp theApp(hInstance);
	
	if( !theApp.Init() )
		return 0;
	
	return theApp.Run();
}
 

BoxApp::BoxApp(HINSTANCE hInstance)
: D3DApp(hInstance), mBoxVB(0), mBoxIB(0), 
  mTheta(1.5f*MathHelper::Pi), mPhi(0.25f*MathHelper::Pi), mRadius(5.0f)
{
	mMainWndCaption = L"Box Demo";
	
	mLastMousePos.x = 0;
	mLastMousePos.y = 0;

	XMMATRIX I = XMMatrixIdentity();
	XMStoreFloat4x4(&mWorld, I);
	XMStoreFloat4x4(&mView, I);
	XMStoreFloat4x4(&mProj, I);
}

BoxApp::~BoxApp()
{
	ReleaseCOM(mBoxVB);
	ReleaseCOM(mBoxIB);
}

bool BoxApp::Init()
{
	if(!D3DApp::Init())
		return false;

	BuildGeometryBuffers();
	BuildFX();
	BuildVertexLayout();

	return true;
}

void BoxApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void BoxApp::UpdateScene(float dt)
{
	// Convert Spherical to Cartesian coordinates.
	float x = mRadius*sinf(mPhi)*cosf(mTheta);
	float z = mRadius*sinf(mPhi)*sinf(mTheta);
	float y = mRadius*cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos    = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up     = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX V = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, V);
}

void BoxApp::DrawScene()
{
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&Colors::LightSteelBlue));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

	md3dImmediateContext->IASetInputLayout(mInputLayout.Get());
    md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	UINT stride = sizeof(Vertex);
    UINT offset = 0;
    md3dImmediateContext->IASetVertexBuffers(0, 1, &mBoxVB, &stride, &offset);
	md3dImmediateContext->IASetIndexBuffer(mBoxIB, DXGI_FORMAT_R32_UINT, 0);

	md3dImmediateContext->VSSetShader(pVertexShader.Get(), nullptr, 0);
	md3dImmediateContext->PSSetShader(pPixelShader.Get(), nullptr, 0);

	// Set constants
	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX view  = XMLoadFloat4x4(&mView);
	XMMATRIX proj  = XMLoadFloat4x4(&mProj);
	XMMATRIX worldViewProj = world*view*proj;

	D3D11_MAPPED_SUBRESOURCE Data;
	Data.pData = nullptr;
	Data.DepthPitch = Data.RowPitch = 0;

	const int subresourceID = 0;

	auto pBuffer = buffer.Get();
	HR(md3dImmediateContext->Map(
		pBuffer,
		subresourceID,
		D3D11_MAP_WRITE_DISCARD,
		0,
		&Data));
	memcpy(Data.pData, &worldViewProj, sizeof(worldViewProj));
	md3dImmediateContext->Unmap(pBuffer, subresourceID);
	md3dImmediateContext->VSSetConstantBuffers(0, 1, &pBuffer);

	// 36 indices for the box.
	md3dImmediateContext->DrawIndexed(36, 0, 0);
	auto hr = md3dDevice->GetDeviceRemovedReason();
	HR(mSwapChain->Present(0, 0));
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if( (btnState & MK_LBUTTON) != 0 )
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi   += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi-0.1f);
	}
	else if( (btnState & MK_RBUTTON) != 0 )
	{
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f*static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f*static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void BoxApp::BuildGeometryBuffers()
{
	// Create vertex buffer
    Vertex vertices[] =
    {
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), (const float*)&Colors::White   },
		{ XMFLOAT3(-1.0f, +1.0f, -1.0f), (const float*)&Colors::Black   },
		{ XMFLOAT3(+1.0f, +1.0f, -1.0f), (const float*)&Colors::Red     },
		{ XMFLOAT3(+1.0f, -1.0f, -1.0f), (const float*)&Colors::Green   },
		{ XMFLOAT3(-1.0f, -1.0f, +1.0f), (const float*)&Colors::Blue    },
		{ XMFLOAT3(-1.0f, +1.0f, +1.0f), (const float*)&Colors::Yellow  },
		{ XMFLOAT3(+1.0f, +1.0f, +1.0f), (const float*)&Colors::Cyan    },
		{ XMFLOAT3(+1.0f, -1.0f, +1.0f), (const float*)&Colors::Magenta }
    };

    D3D11_BUFFER_DESC vbd;
    vbd.Usage = D3D11_USAGE_IMMUTABLE;
    vbd.ByteWidth = sizeof(Vertex) * 8;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = 0;
    vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;
    D3D11_SUBRESOURCE_DATA vinitData;
    vinitData.pSysMem = vertices;
    HR(md3dDevice->CreateBuffer(&vbd, &vinitData, &mBoxVB));


	// Create the index buffer

	UINT indices[] = {
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3, 
		4, 3, 7
	};

	D3D11_BUFFER_DESC ibd;
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.ByteWidth = sizeof(UINT) * 36;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;
    ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;
    D3D11_SUBRESOURCE_DATA iinitData;
    iinitData.pSysMem = indices;
    HR(md3dDevice->CreateBuffer(&ibd, &iinitData, &mBoxIB));
}

D3D11_BUFFER_DESC SetDefaultConstantBuffer( UINT size, bool dynamic )
{
	D3D11_BUFFER_DESC state;
	// Create the settings for a constant buffer.  This includes setting the 
	// constant buffer flag, allowing the CPU write access, and a dynamic usage.
	// Additional flags may be set as needed.

	state.ByteWidth = size;
    state.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    state.MiscFlags = 0;
    state.StructureByteStride = 0;

	if ( dynamic )
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

ComPtr<ID3DBlob> CompileShader(
	const std::wstring & filename,
	const D3D_SHADER_MACRO * defines,
	const std::string & function,
	const std::string & model)
{
    UINT flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> byteCode;
	ComPtr<ID3DBlob> errors;

	HRESULT hr = S_OK;
	HR(D3DX11CompileFromFile(filename.c_str(),
		defines,
		nullptr,
		function.c_str(),
		model.c_str(),
		flags,
		0,
		nullptr,
		byteCode.GetAddressOf(),
		errors.GetAddressOf(),
		&hr));

	return byteCode;
}
 
void BoxApp::BuildFX()
{
	pVertexCompiledShader = CompileShader(L"FX/color.fx", nullptr, "VS", "vs_5_0");
	md3dDevice->CreateVertexShader(pVertexCompiledShader->GetBufferPointer(),
		pVertexCompiledShader->GetBufferSize(), nullptr, pVertexShader.ReleaseAndGetAddressOf());
	pPixelCompiledShader = CompileShader(L"FX/color.fx", nullptr, "PS", "ps_5_0");
	md3dDevice->CreatePixelShader(pPixelCompiledShader->GetBufferPointer(),
		pPixelCompiledShader->GetBufferSize(), nullptr, pPixelShader.ReleaseAndGetAddressOf());

	md3dDevice->CreateVertexShader(
		pVertexCompiledShader->GetBufferPointer(),
		pVertexCompiledShader->GetBufferSize(),
		nullptr,
		pVertexShader.ReleaseAndGetAddressOf());

	md3dDevice->CreatePixelShader(
		pPixelCompiledShader->GetBufferPointer(),
		pPixelCompiledShader->GetBufferSize(),
		nullptr,
		pPixelShader.ReleaseAndGetAddressOf());

	auto Desc = SetDefaultConstantBuffer(sizeof(XMMATRIX), true);
	HR(md3dDevice->CreateBuffer(
		&Desc,
		nullptr, 
		buffer.GetAddressOf()));
}

void BoxApp::BuildVertexLayout()
{
	// Create the vertex input layout.
	D3D11_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	// Create the input layout
	HR(md3dDevice->CreateInputLayout(
		inputLayout,
		2,
		pVertexCompiledShader->GetBufferPointer(),
		pVertexCompiledShader->GetBufferSize(),
		mInputLayout.GetAddressOf()));
}
 