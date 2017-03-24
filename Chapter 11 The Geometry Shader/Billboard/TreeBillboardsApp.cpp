//***************************************************************************************
// LitSkullDemo.cpp by Frank Luna (C) 2011 All Rights Reserved.
//
// Controls:
//		Hold the left mouse button down and move the mouse to rotate.
//      Hold the right mouse button down to zoom in and out.
//
//***************************************************************************************

#include <DDSTextureLoader.h>
#include <numeric>
#include "d3dApp.h"
#include "d3dx11Effect.h"
#include "GeometryGenerator.h"
#include "MathHelper.h"
#include "LightHelper.h"
#include "FrameResource.h"
#include "PipelineStateObject.h"
#include "ShaderFactoryDX11.h"
#include "MeshGeometry.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "dxerr.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Gdi32.lib")

#ifdef _DEBUG
#pragma comment(lib, "d3dx11d.lib")
#else
#pragma comment(lib, "d3dx11.lib")
#endif

using Microsoft::WRL::ComPtr;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;
	bool bDirty = true;
    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	MaterialFresnel* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D11_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	AlphaTestedTreeSprites = 0,
	Texture,
	Count
};

enum ShaderType
{
	VertexShader = 0,
	GeometryShader,
	PixelShader, 
};

class TreeBillboardsApp : public D3DApp 
{
public:
	TreeBillboardsApp(HINSTANCE hInstance);
	~TreeBillboardsApp();

	bool Init();
	void OnResize();
	void UpdateScene(float dt);
	void DrawScene();

	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
	void OnKeyBoardInput(float dt);

private:
	void LoadTextures();
	void CompileShader(ShaderType shaderType, std::string name, D3D_SHADER_MACRO * defines, const std::wstring & filename, const std::string & function, const std::string & model);
	void CreatePSO(RenderLayer layer, const PipelineStateDesc& desc);

	void BuildFX();
	void BuildVertexLayout();
	void BuildMaterials();
	void BuildRoomGeometryBuffers();
	void BuildTreeGeometryBuffers();
	void BuildGeometry();
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildSamplerState();
	void CreateBlendState(const std::string& tag, const D3D11_BLEND_DESC & desc);
	void CreateDepthStencilState(const std::string& tag, const D3D11_DEPTH_STENCIL_DESC& desc);
	void CreateResterizerState(const std::string & tag, const D3D11_RASTERIZER_DESC & desc);
	void BuildPSO();
	void UploadMaterials();
	void UploadObjects();
	void UpdateCamera(float dt);
	void DrawRenderItems(RenderLayer layer);
	void UpdateMainPassCB(float dt);

	std::array<const CD3D11_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	using VShader = std::pair<ComPtr<ID3D11VertexShader>, ComPtr<ID3DBlob>>;
	using PShader = std::pair<ComPtr<ID3D11PixelShader>, ComPtr<ID3DBlob>>;
	using GShader = std::pair<ComPtr<ID3D11GeometryShader>, ComPtr<ID3DBlob>>;
	std::unordered_map<std::string, VShader> mVShaders;
	std::unordered_map<std::string, GShader> mGShaders;
	std::unordered_map<std::string, PShader> mPShaders;
	std::unordered_map<std::string, ComPtr<ID3D11BlendState>> mBlendState;
	std::unordered_map<std::string, ComPtr<ID3D11DepthStencilState>> mDepthStencilState;
	std::unordered_map<std::string, ComPtr<ID3D11InputLayout>> mInputLayout;
	std::unordered_map<std::string, ComPtr<ID3D11RasterizerState>> mRasterizerState;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, std::unique_ptr<MaterialFresnel>> mMaterials;
    std::unique_ptr<FrameResource> mFrameResource;
	std::vector<ComPtr<ID3D11SamplerState>> mSamplerState;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by state (like PSO)
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	std::vector<std::pair<int, std::string>> mRenderLayerCounstBuffer[(int)RenderLayer::Count];
	std::unique_ptr<PipelineStateObject> mPSOs[(int)RenderLayer::Count];

	std::unordered_map<std::string, ComPtr<ID3D11Buffer>> mConstantBuffers;

	XMFLOAT3 mSkullTranslation = { 0.0f, 1.0f, -5.0f };

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMMATRIX mView = XMMatrixIdentity();
	XMMATRIX mProj = XMMatrixIdentity();

    float mTheta = 1.24f*XM_PI;
    float mPhi = 0.42f*XM_PI;
    float mRadius = 12.0f;

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
				   PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

	TreeBillboardsApp theApp(hInstance);
	
	if( !theApp.Init() )
		return 0;
	
	return theApp.Run();
}
 

TreeBillboardsApp::TreeBillboardsApp(HINSTANCE hInstance)
: D3DApp(hInstance),
  mEyePos(0.0f, 0.0f, 0.0f), mTheta(1.5f*MathHelper::Pi), mPhi(0.1f*MathHelper::Pi), mRadius(15.0f)
{
	mMainWndCaption = L"TreeBillboards Demo";
	
	mLastMousePos.x = 0;
	mLastMousePos.y = 0;
	mEnable4xMsaa = true;
}

TreeBillboardsApp::~TreeBillboardsApp()
{
}

bool TreeBillboardsApp::Init()
{
	if (!D3DApp::Init())
		return false;

	LoadTextures();
	BuildFX();
	BuildVertexLayout();
	BuildGeometry();
	BuildMaterials();
	BuildSamplerState();
	BuildPSO();
	BuildRenderItems();
	BuildFrameResources();
	UploadMaterials();

	return true;
}

void TreeBillboardsApp::OnResize()
{
	D3DApp::OnResize();

	mProj = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void TreeBillboardsApp::BuildFX()
{
	CompileShader(VertexShader, "texture", nullptr, L"FX/Basic.fx", "VS", "vs_5_0");
	CompileShader(PixelShader, "texture", nullptr, L"FX/Basic.fx", "PS", "ps_5_0");

	CompileShader(VertexShader, "tree", nullptr, L"FX/tree.fx", "VS", "vs_5_0");
	CompileShader(GeometryShader, "tree", nullptr, L"FX/tree.fx", "GS", "gs_5_0");
	CompileShader(PixelShader, "tree", nullptr, L"FX/tree.fx", "PS", "ps_5_0");
}

void TreeBillboardsApp::CompileShader(
	ShaderType shaderType,
	std::string name, 
	D3D_SHADER_MACRO* defines, 
	const std::wstring & filename,
	const std::string& function,
	const std::string& model)
{
	auto blob = ShaderFactoryDX11::CompileShader(filename, defines, function, model);
	switch (shaderType) {
	case VertexShader:
		{
			ComPtr<ID3D11VertexShader> shader;
			md3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader.GetAddressOf());
			mVShaders.insert({ name, { shader, blob } });
			break;
		}
	case PixelShader:
		{
			ComPtr<ID3D11PixelShader> shader;
			md3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader.GetAddressOf());
			mPShaders.insert({ name, { shader, blob } });
			break;
		}
	case GeometryShader:
		{
			ComPtr<ID3D11GeometryShader> shader;
			md3dDevice->CreateGeometryShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader.GetAddressOf());
			mGShaders.insert({ name, { shader, blob } });
			break;
		}
	}
}

void TreeBillboardsApp::BuildVertexLayout()
{
	// Create the vertex-tex input layout.
	std::vector<D3D11_INPUT_ELEMENT_DESC> ILTex = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	ComPtr<ID3D11InputLayout> pLayoutTex;
	HR(md3dDevice->CreateInputLayout(
		ILTex.data(),
		ILTex.size(),
		mVShaders["texture"].second->GetBufferPointer(),
		mVShaders["texture"].second->GetBufferSize(),
		pLayoutTex.GetAddressOf()));
	mInputLayout["texture"] = pLayoutTex;

	// Create the sprite vertex input layout.
	std::vector<D3D11_INPUT_ELEMENT_DESC> IL = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"SIZE",   0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	ComPtr<ID3D11InputLayout> pLayout;
	HR(md3dDevice->CreateInputLayout(
		IL.data(),
		IL.size(),
		mVShaders["tree"].second->GetBufferPointer(),
		mVShaders["tree"].second->GetBufferSize(),
		pLayout.GetAddressOf()));
	mInputLayout["tree"] = pLayout;
}

void TreeBillboardsApp::BuildMaterials()
{
	using Material = MaterialFresnel;

	auto grassTile = std::make_unique<Material>();
	grassTile->Name = "grass";
	grassTile->MatCBIndex = 0;
	grassTile->DiffuseSrv = "grassTex";
	grassTile->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grassTile->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
	grassTile->Roughness = 0.3f;

	auto treeMat = std::make_unique<Material>();
	treeMat->Name = "tree";
	treeMat->MatCBIndex = 0;
	treeMat->DiffuseSrv = "treeTex";
	treeMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeMat->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
	treeMat->Roughness = 0.3f;

	mMaterials["grass"] = std::move(grassTile);
	mMaterials["tree"] = std::move(treeMat);
}

void TreeBillboardsApp::UpdateScene(float dt)
{
	OnKeyBoardInput(dt);
	UpdateCamera(dt);
	UpdateMainPassCB(dt);
	UploadObjects();
}

void TreeBillboardsApp::OnKeyBoardInput(float dt)
{
}

void TreeBillboardsApp::UpdateCamera(float dt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	mEyePos.y = mRadius*cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	mView = XMMatrixLookAtLH(pos, target, up);
}

void TreeBillboardsApp::DrawRenderItems(RenderLayer layer)
{
	auto pPSO = mPSOs[(int)layer].get();
	md3dImmediateContext->VSSetShader(pPSO->pVS, nullptr, 0);
	md3dImmediateContext->PSSetShader(pPSO->pPS, nullptr, 0);
	md3dImmediateContext->GSSetShader(pPSO->pGS, nullptr, 0);
	md3dImmediateContext->IASetInputLayout(pPSO->pIL);
	md3dImmediateContext->OMSetBlendState(
		pPSO->blend.pBlendState,
		pPSO->blend.BlendFactor.data(),
		pPSO->blend.SampleMask);
	md3dImmediateContext->OMSetDepthStencilState(
		pPSO->depthStencil.pDepthStencilState,
		pPSO->depthStencil.StencilRef);
	md3dImmediateContext->RSSetState(pPSO->pResterizer);

	std::array<ID3D11Buffer*, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> ConstantBuffers;
	ConstantBuffers.assign(nullptr);
	auto& cbuffers = mRenderLayerCounstBuffer[(int)layer];
	for (auto cb : cbuffers) {
		ConstantBuffers[cb.first] = mConstantBuffers[cb.second].Get();
	}

	const std::vector<RenderItem*>& ritems = mRitemLayer[(int)layer];

	for (size_t i = 0; i < ritems.size(); ++i) {
		auto& ri = ritems[i];
		md3dImmediateContext->IASetPrimitiveTopology(ri->PrimitiveType);
		auto geo = ri->Geo;
		auto pib = static_cast<ID3D11Buffer*>(geo->IndexBufferGPU.Get());
			md3dImmediateContext->IASetIndexBuffer(pib, geo->IndexFormat, 0);

		auto pvb = static_cast<ID3D11Buffer*>(geo->VertexBufferGPU.Get());
		UINT vstride = geo->VertexByteStride, offset = 0;
		md3dImmediateContext->IASetVertexBuffers(0, 1, &pvb, &vstride, &offset);

		ConstantBuffers[0] = mFrameResource->ObjectCB->Resource(ri->ObjCBIndex);
		ConstantBuffers[1] = mFrameResource->MaterialCB->Resource(ri->Mat->MatCBIndex);
		md3dImmediateContext->VSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, ConstantBuffers.data());
		md3dImmediateContext->PSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, ConstantBuffers.data());
		md3dImmediateContext->GSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, ConstantBuffers.data());

		std::array<ID3D11ShaderResourceView*, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> SRV;
		auto& rTex = mTextures[ri->Mat->DiffuseSrv];
		if (rTex) {
			SRV[0] = rTex->Resource.Get();
			md3dImmediateContext->PSSetShaderResources(0, 1, SRV.data());
		}

		md3dImmediateContext->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void TreeBillboardsApp::DrawScene()
{
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&Colors::LightSteelBlue));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

	std::array<ID3D11SamplerState*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> Sampler;
	Sampler[0] = mSamplerState[4].Get();
	md3dImmediateContext->PSSetSamplers(0, 1, Sampler.data());

	DrawRenderItems(RenderLayer::Texture);
	DrawRenderItems(RenderLayer::AlphaTestedTreeSprites);

	HR(mSwapChain->Present(0, 0));
}

void TreeBillboardsApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void TreeBillboardsApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void TreeBillboardsApp::OnMouseMove(WPARAM btnState, int x, int y)
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
		// Make each pixel correspond to 0.01 unit in the scene.
		float dx = 0.01f*static_cast<float>(x - mLastMousePos.x);
		float dy = 0.01f*static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 3.0f, 200.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void TreeBillboardsApp::BuildGeometry()
{
	BuildRoomGeometryBuffers();
	BuildTreeGeometryBuffers();
}

void TreeBillboardsApp::BuildRoomGeometryBuffers()
{
	using Vertex = VertexTex;

    // Create and specify geometry.  For this sample we draw a floor
	// and a wall with a mirror on it.  We put the floor, wall, and
	// mirror geometry in one vertex buffer.
	//
    //   /--------------/
    //  /   Floor      /
	// /--------------/

	std::array<Vertex, 4> vertices = 
	{
		// Floor: Observe we tile texture coordinates.
		Vertex(-10.0f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
		Vertex(-10.0f, 0.0f,  10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
		Vertex(+10.0f, 0.0f,  10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
		Vertex(+10.0f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),
	};

	std::array<std::int16_t, 6> indices = 
	{
		// Floor
		0, 1, 2,	
		0, 2, 3,
	};

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = 6;
	floorSubmesh.StartIndexLocation = 0;
	floorSubmesh.BaseVertexLocation = 0;

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "roomGeo";
	geo->VertexBufferGPU = d3dHelper::CreateVertexBuffer(
		md3dDevice, vertices.data(), vbByteSize);
	geo->IndexBufferGPU = d3dHelper::CreateIndexBuffer(
		md3dDevice, indices.data(), ibByteSize);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["floor"] = floorSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void TreeBillboardsApp::BuildTreeGeometryBuffers()
{
	struct TreePointSprite
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	static const int treeCount = 16;
	std::array<TreePointSprite, 16> vertices;
	for(UINT i = 0; i < treeCount; ++i)
	{
		float x = MathHelper::RandF(-10.0f, 10.0f);
		float z = MathHelper::RandF(-10.0f, 10.0f);
		float y = 0;

		// Move tree slightly above land height.
		y += 1.45f;

		vertices[i].Pos = XMFLOAT3(x, y, z);
		vertices[i].Size = XMFLOAT2(3.0f, 3.0f);
	}

	std::vector<std::uint16_t> indices(treeCount);
	std::iota(indices.begin(), indices.end(), 0);

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreePointSprite);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = indices.size();
	floorSubmesh.StartIndexLocation = 0;
	floorSubmesh.BaseVertexLocation = 0;

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeGeo";
	geo->VertexBufferGPU = d3dHelper::CreateVertexBuffer(
		md3dDevice, vertices.data(), vbByteSize);
	geo->IndexBufferGPU = d3dHelper::CreateIndexBuffer(
		md3dDevice, indices.data(), ibByteSize);

	geo->VertexByteStride = sizeof(TreePointSprite);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["tree"] = floorSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void TreeBillboardsApp::LoadTextures()
{
	auto checkboardTex = std::make_unique<Texture>();
	checkboardTex->Name = "grassTex";
	checkboardTex->Filename = L"../../Textures/grass.dds";
	ThrowIfFailed(D3DX11CreateShaderResourceViewFromFile(
		md3dDevice,
		checkboardTex->Filename.c_str(),
		0, 0,
		checkboardTex->Resource.GetAddressOf(), 0));

	auto treeTex = std::make_unique<Texture>();
	treeTex->Name = "treeTex";
	treeTex->Filename = L"../../Textures/treeArray2.dds";

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile(
		md3dDevice,
		treeTex->Filename.c_str(),
		treeTex->texture.GetAddressOf(),
		treeTex->Resource.GetAddressOf()));

	mTextures[checkboardTex->Name] = std::move(checkboardTex);
	mTextures[treeTex->Name] = std::move(treeTex);
}

void TreeBillboardsApp::BuildRenderItems()
{
	auto floorRitem = std::make_unique<RenderItem>();
	floorRitem->World = MathHelper::Identity4x4();
	floorRitem->TexTransform = MathHelper::Identity4x4();
	floorRitem->ObjCBIndex = 0;
	floorRitem->Mat = mMaterials["grass"].get();
	floorRitem->Geo = mGeometries["roomGeo"].get();
	floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Texture].push_back(floorRitem.get());

	auto treeRitem = std::make_unique<RenderItem>();
	treeRitem->World = MathHelper::Identity4x4();
	treeRitem->TexTransform = MathHelper::Identity4x4();
	treeRitem->ObjCBIndex = 0;
	treeRitem->Mat = mMaterials["tree"].get();
	treeRitem->Geo = mGeometries["treeGeo"].get();
	treeRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeRitem->IndexCount = treeRitem->Geo->DrawArgs["tree"].IndexCount;
	treeRitem->StartIndexLocation = treeRitem->Geo->DrawArgs["tree"].StartIndexLocation;
	treeRitem->BaseVertexLocation = treeRitem->Geo->DrawArgs["tree"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeRitem.get());

	mAllRitems.push_back(std::move(floorRitem));
	mAllRitems.push_back(std::move(treeRitem));

	for (int i = 0; i < (int)RenderLayer::Count; i++)
		mRenderLayerCounstBuffer[i].push_back({ 2, "MainPass" });
}
 
void TreeBillboardsApp::BuildFrameResources()
{
	mFrameResource = std::make_unique<FrameResource>(md3dDevice, 2, (UINT)mAllRitems.size(), (UINT)mMaterials.size());
}

void TreeBillboardsApp::UploadMaterials()
{
	for (auto& m : mMaterials) {
		auto mat = m.second.get();
		MaterialConstants matConstants;
		matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
		matConstants.FresnelR0 = mat->FresnelR0;
		matConstants.Roughness = mat->Roughness;
		mFrameResource->MaterialCB->UploadData(md3dImmediateContext, mat->MatCBIndex, matConstants);
	}
}

void TreeBillboardsApp::UploadObjects()
{
	for (size_t i = 0; i < mAllRitems.size(); i++) {
		if (!mAllRitems[i]->bDirty) continue;
		XMMATRIX world = XMLoadFloat4x4(&mAllRitems[i]->World);
		XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);

		ObjectConstants obj;
		obj.World = world;
		obj.WorldInvTranspose = worldInvTranspose;
		obj.TexTransform = XMLoadFloat4x4(&mAllRitems[i]->TexTransform);
		mFrameResource->ObjectCB->UploadData(md3dImmediateContext, mAllRitems[i]->ObjCBIndex, obj);
		mAllRitems[i]->bDirty = false;
	}
}

void TreeBillboardsApp::UpdateMainPassCB(float dt)
{
	XMMATRIX viewProj = XMMatrixMultiply(mView, mProj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(mView), mView);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(mProj), mProj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    PassConstants MainPassCB;
	XMStoreFloat4x4(&MainPassCB.View, mView);
	XMStoreFloat4x4(&MainPassCB.InvView, invView);
	XMStoreFloat4x4(&MainPassCB.Proj, mProj);
	XMStoreFloat4x4(&MainPassCB.InvProj, invProj);
	XMStoreFloat4x4(&MainPassCB.ViewProj, viewProj);
	XMStoreFloat4x4(&MainPassCB.InvViewProj, invViewProj);
	MainPassCB.EyePosW = mEyePos;
	MainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	MainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	MainPassCB.NearZ = 1.0f;
	MainPassCB.FarZ = 1000.0f;
	// MainPassCB.TotalTime = gt.TotalTime();
	MainPassCB.DeltaTime = dt;
	MainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	MainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	MainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	MainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	MainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	MainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	MainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };
	mFrameResource->PassCB->UploadData(md3dImmediateContext, 0, MainPassCB);
	mConstantBuffers["MainPass"] = mFrameResource->PassCB->Resource(0);
}

std::array<const CD3D11_SAMPLER_DESC, 6> TreeBillboardsApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	auto pointWrap = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	pointWrap.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	pointWrap.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	pointWrap.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	pointWrap.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	auto pointClamp = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	pointClamp.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	pointClamp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	pointClamp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	pointClamp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	auto linearWrap = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	linearWrap.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	linearWrap.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	linearWrap.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	linearWrap.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	auto linearClamp = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	linearClamp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	linearClamp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	linearClamp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	linearClamp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	auto anisotropicWrap = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	anisotropicWrap.Filter = D3D11_FILTER_ANISOTROPIC;
	anisotropicWrap.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	anisotropicWrap.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	anisotropicWrap.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	anisotropicWrap.MipLODBias = 0.0f;
	anisotropicWrap.MaxAnisotropy = 8;
	anisotropicWrap.BorderColor[1] = 0.0f;
	anisotropicWrap.BorderColor[2] = 0.0f;

	auto anisotropicClamp = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	anisotropicClamp.Filter = D3D11_FILTER_ANISOTROPIC;
	anisotropicClamp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	anisotropicClamp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	anisotropicClamp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	anisotropicClamp.MipLODBias = 0.0f;
	anisotropicClamp.MaxAnisotropy = 8;

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}

void TreeBillboardsApp::BuildSamplerState() 
{
	auto state = GetStaticSamplers();
	for (auto s : state) {
		ComPtr<ID3D11SamplerState> ss;
		md3dDevice->CreateSamplerState(&s, ss.GetAddressOf());
		mSamplerState.push_back(ss);
	}
}

void TreeBillboardsApp::CreateBlendState(const std::string& tag, const D3D11_BLEND_DESC & desc)
{
	ComPtr<ID3D11BlendState> BS;
	HR(md3dDevice->CreateBlendState(&desc, BS.GetAddressOf()));
	mBlendState[tag] = BS;
}

void TreeBillboardsApp::CreateDepthStencilState(const std::string& tag, const D3D11_DEPTH_STENCIL_DESC& desc)
{
	ComPtr<ID3D11DepthStencilState> DDS;
	HR(md3dDevice->CreateDepthStencilState(&desc, DDS.GetAddressOf()));
	mDepthStencilState[tag] = DDS;
}

void TreeBillboardsApp::CreateResterizerState(const std::string& tag, const D3D11_RASTERIZER_DESC& desc)
{
	ComPtr<ID3D11RasterizerState> RS;
	HR(md3dDevice->CreateRasterizerState(&desc, RS.GetAddressOf()));
	mRasterizerState[tag] = RS;
}

void TreeBillboardsApp::CreatePSO(RenderLayer layer, const PipelineStateDesc& desc)
{
	auto pPSO = std::make_unique<PipelineStateObject>();
	if (mInputLayout.count(desc.IL))
		pPSO->pIL = mInputLayout[desc.IL].Get();
	assert(pPSO->pIL != nullptr);
	if (mVShaders.count(desc.VS))
		pPSO->pVS = mVShaders[desc.VS].first.Get();
	assert(pPSO->pVS != nullptr);
	if (mPShaders.count(desc.PS))
		pPSO->pPS = mPShaders[desc.PS].first.Get();
	assert(pPSO->pPS != nullptr);

	if (!desc.GS.empty()) {
		assert(mGShaders.count(desc.GS));
		pPSO->pGS = mGShaders[desc.GS].first.Get();
	}
	if (!desc.BS.empty()) {
		assert(mBlendState.count(desc.BS));
		pPSO->blend.pBlendState = mBlendState[desc.BS].Get();
	}
	pPSO->blend.BlendFactor = desc.BlendFactor;
	pPSO->blend.SampleMask = desc.SampleMask;

	if (!desc.DSS.empty()) {
		assert(mDepthStencilState.count(desc.DSS));
		pPSO->depthStencil.pDepthStencilState = mDepthStencilState[desc.DSS].Get();
	}
	pPSO->depthStencil.StencilRef = desc.StencilRef;

	if (!desc.RS.empty()) {
		assert(mRasterizerState.count(desc.RS));
		pPSO->pResterizer = mRasterizerState[desc.RS].Get();
	}
	mPSOs[(int)layer] = std::move(pPSO);
}

void TreeBillboardsApp::BuildPSO()
{
	CreatePSO(RenderLayer::Texture, { "texture", "texture", "texture" });

	CD3D11_BLEND_DESC a2cDesc(D3D11_DEFAULT);
	a2cDesc.AlphaToCoverageEnable = true;
	CreateBlendState("a2cBlend", a2cDesc);

	PipelineStateDesc tree { "tree", "tree", "tree", "tree" };
	tree.BS = "a2cBlend";
	CreatePSO(RenderLayer::AlphaTestedTreeSprites, tree);
}
