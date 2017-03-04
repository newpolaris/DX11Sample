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
#include "MathHelper.h"
#include "ShaderFactoryDX11.h"

using Microsoft::WRL::ComPtr;

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D11_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
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
	void BuildFX();
	void BuildVertexLayout();
	void BuildGeometry();
	void BuildRenderItems();

	template<typename T> void UpdateVariable(std::string tag, T * data);

private:
	ComPtr<ID3D11VertexShader> pVertexShader;
	ComPtr<ID3DBlob> pVertexCompiledShader;

	ComPtr<ID3D11PixelShader> pPixelShader;
	ComPtr<ID3DBlob> pPixelCompiledShader;
	
	ComPtr<ID3D11InputLayout> pLayout;
	ComPtr<ID3D11Buffer> buffer;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, ComPtr<ID3D11Buffer>> resources;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

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
	: D3DApp(hInstance), mTheta(1.5f*MathHelper::Pi),
	mPhi(0.25f*MathHelper::Pi), mRadius(5.0f)
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
}

bool BoxApp::Init()
{
	if(!D3DApp::Init())
		return false;

	BuildFX();
	BuildVertexLayout();
	BuildGeometry();
	BuildRenderItems();

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

template <typename T>
void BoxApp::UpdateVariable(
	std::string tag,
	T* data)
{
	D3D11_MAPPED_SUBRESOURCE Data;
	Data.pData = nullptr;
	Data.DepthPitch = Data.RowPitch = 0;

	const int subresourceID = 0;

	auto pBuffer = resources[tag].Get();
	HR(md3dImmediateContext->Map(
		pBuffer,
		subresourceID,
		D3D11_MAP_WRITE_DISCARD,
		0,
		&Data));
	memcpy(Data.pData, data, sizeof(T));
	md3dImmediateContext->Unmap(pBuffer, subresourceID);
}

void BoxApp::DrawScene()
{
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&Colors::LightSteelBlue));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

	md3dImmediateContext->IASetInputLayout(mInputLayout.Get());

	for (auto& ri : mAllRitems) {
		md3dImmediateContext->IASetPrimitiveTopology(ri->PrimitiveType);

		auto geo = ri->Geo;
		auto pib = static_cast<ID3D11Buffer*>(geo->IndexBufferGPU.Get());
		auto ibType = geo->IndexFormat;
		auto iStart = ri->StartIndexLocation;
		md3dImmediateContext->IASetIndexBuffer(pib, ibType, iStart);

		auto pvb = static_cast<ID3D11Buffer*>(geo->VertexBufferGPU.Get());
		UINT vstride = geo->VertexByteStride, vstart = ri->BaseVertexLocation;
		md3dImmediateContext->IASetVertexBuffers(0, 1, &pvb, &vstride, &vstart);
	}

	md3dImmediateContext->VSSetShader(pVertexShader.Get(), nullptr, 0);
	md3dImmediateContext->PSSetShader(pPixelShader.Get(), nullptr, 0);

	// Set constants
	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX view  = XMLoadFloat4x4(&mView);
	XMMATRIX proj  = XMLoadFloat4x4(&mProj);
	XMMATRIX worldViewProj = world*view*proj;

	UpdateVariable("worldViewProj", &worldViewProj);

	std::array<ID3D11Buffer*, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT>  ConstantBuffers;
	ConstantBuffers[0] = resources["worldViewProj"].Get();
	md3dImmediateContext->VSSetConstantBuffers(0, 1, ConstantBuffers.data());

	// 36 indices for the box.
	md3dImmediateContext->DrawIndexed(36, 0, 0);
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

void BoxApp::BuildFX()
{
	pVertexCompiledShader = ShaderFactoryDX11::CompileShader(L"FX/color.fx", nullptr, "VS", "vs_5_0");
	md3dDevice->CreateVertexShader(pVertexCompiledShader->GetBufferPointer(),
		pVertexCompiledShader->GetBufferSize(), nullptr, pVertexShader.ReleaseAndGetAddressOf());
	pPixelCompiledShader = ShaderFactoryDX11::CompileShader(L"FX/color.fx", nullptr, "PS", "ps_5_0");
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

	resources["worldViewProj"] = d3dHelper::CreateConstantBuffer(md3dDevice, sizeof(XMMATRIX));
}

void BoxApp::BuildVertexLayout()
{
	// Create the vertex input layout.
	std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	// Create the input layout
	HR(md3dDevice->CreateInputLayout(
		inputLayout.data(),
		inputLayout.size(),
		pVertexCompiledShader->GetBufferPointer(),
		pVertexCompiledShader->GetBufferSize(),
		mInputLayout.GetAddressOf()));
}

void BoxApp::BuildGeometry()
{
	std::vector<Vertex> vertices =
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

	std::vector<int> indices = {
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

	const UINT vbByteSize = sizeof(Vertex)*vertices.size();
	const UINT ibByteSize = sizeof(indices)*indices.size();

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";
	geo->VertexBufferGPU = d3dHelper::CreateVertexBuffer(
		md3dDevice, vertices.data(), vbByteSize);
	geo->IndexBufferGPU = d3dHelper::CreateIndexBuffer(
		md3dDevice, indices.data(), ibByteSize);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void BoxApp::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem));
}
 