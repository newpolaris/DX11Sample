//***************************************************************************************
// LitSkullDemo.cpp by Frank Luna (C) 2011 All Rights Reserved.
//
// Controls:
//		Hold the left mouse button down and move the mouse to rotate.
//      Hold the right mouse button down to zoom in and out.
//
//***************************************************************************************

#include "d3dApp.h"
#include "d3dx11Effect.h"
#include "GeometryGenerator.h"
#include "MathHelper.h"
#include "LightHelper.h"
#include "FrameResource.h"
#include "PipelineStateObject.h"
#include "ShaderFactoryDX11.h"

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
	Opaque = 0,
	Texture,
	Mirrors,
	Reflected,
	Transparent,
	Shadow,
	Count
};

class StencilDemo : public D3DApp 
{
public:
	StencilDemo(HINSTANCE hInstance);
	~StencilDemo();

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
	void CompileShaderVS(std::string name, D3D_SHADER_MACRO * defines, const std::wstring & filename, const std::string & function, const std::string & model);
	void CompileShaderPS(std::string name, D3D_SHADER_MACRO * defines, const std::wstring & filename, const std::string & function, const std::string & model);
	void CreatePSO(RenderLayer layer, const PipelineStateDesc& desc);

	void BuildFX();
	void BuildVertexLayout();
	void BuildMaterials();
	void BuildRoomGeometryBuffers();
	void BuildSkullGeometryBuffers();
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
	std::unordered_map<std::string, VShader> mVShaders;
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

	// Cache render items of interest.
	RenderItem* mSkullRitem = nullptr;
	RenderItem* mReflectedSkullRitem = nullptr;
	RenderItem* mShadowedSkullRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by state (like PSO)
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	std::unique_ptr<PipelineStateObject> mPSOs[(int)RenderLayer::Count];

    PassConstants mMainPassCB;
	PassConstants mReflectedPassCB;

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

	StencilDemo theApp(hInstance);
	
	if( !theApp.Init() )
		return 0;
	
	return theApp.Run();
}
 

StencilDemo::StencilDemo(HINSTANCE hInstance)
: D3DApp(hInstance),
  mEyePos(0.0f, 0.0f, 0.0f), mTheta(1.5f*MathHelper::Pi), mPhi(0.1f*MathHelper::Pi), mRadius(15.0f)
{
	mMainWndCaption = L"Stencil Demo";
	
	mLastMousePos.x = 0;
	mLastMousePos.y = 0;
}

StencilDemo::~StencilDemo()
{
}

bool StencilDemo::Init()
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

void StencilDemo::OnResize()
{
	D3DApp::OnResize();

	mProj = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void StencilDemo::BuildFX()
{
	D3D_SHADER_MACRO textureMacros[] = { "TEXTURE", "1", NULL, NULL };
	CompileShaderVS("texture", textureMacros, L"FX/Basic.fx", "VS", "vs_5_0");
	CompileShaderPS("texture", textureMacros, L"FX/Basic.fx", "PS", "ps_5_0");
	CompileShaderVS("standard", nullptr, L"FX/Basic.fx", "VS", "vs_5_0");
	CompileShaderPS("standard", nullptr, L"FX/Basic.fx", "PS", "ps_5_0");
}

void StencilDemo::CompileShaderVS(
	std::string name, 
	D3D_SHADER_MACRO* defines, 
	const std::wstring & filename,
	const std::string& function,
	const std::string& model)
{
	auto vs = ShaderFactoryDX11::CompileShader(filename, defines, function, model);
	ComPtr<ID3D11VertexShader> blob;
	md3dDevice->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, blob.GetAddressOf());
	mVShaders.insert({ name, { blob, vs } });
}

void StencilDemo::CompileShaderPS(
	std::string name, 
	D3D_SHADER_MACRO* defines, 
	const std::wstring & filename,
	const std::string& function,
	const std::string& model)
{
	auto ps = ShaderFactoryDX11::CompileShader(filename, defines, function, model);
	ComPtr<ID3D11PixelShader> blob;
	md3dDevice->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, blob.GetAddressOf());
	mPShaders.insert({ name, { blob, ps } });
}

void StencilDemo::BuildVertexLayout()
{
	// Create the vertex input layout.
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

	// Create the vertex input layout.
	std::vector<D3D11_INPUT_ELEMENT_DESC> IL = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	ComPtr<ID3D11InputLayout> pLayout;
	HR(md3dDevice->CreateInputLayout(
		IL.data(),
		IL.size(),
		mVShaders["standard"].second->GetBufferPointer(),
		mVShaders["standard"].second->GetBufferSize(),
		pLayout.GetAddressOf()));
	mInputLayout["standard"] = pLayout;
}

void StencilDemo::BuildMaterials()
{
	using Material = MaterialFresnel;

	auto bricks = std::make_unique<Material>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 0;
	bricks->DiffuseSrv = "bricksTex";
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	bricks->Roughness = 0.25f;

	auto checkertile = std::make_unique<Material>();
	checkertile->Name = "checkertile";
	checkertile->MatCBIndex = 1;
	checkertile->DiffuseSrv = "checkboardTex";
	checkertile->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	checkertile->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
	checkertile->Roughness = 0.3f;

	auto icemirror = std::make_unique<Material>();
	icemirror->Name = "icemirror";
	icemirror->MatCBIndex = 2;
	icemirror->DiffuseSrv = "iceTex";
	icemirror->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
	icemirror->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	icemirror->Roughness = 0.5f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrv = -1;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.3f;

	auto shadowMat = std::make_unique<Material>();
	shadowMat->Name = "shadowMat";
	shadowMat->MatCBIndex = 4;
	shadowMat->DiffuseSrv = -1;
	shadowMat->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
	shadowMat->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
	shadowMat->Roughness = 0.0f;

	mMaterials["bricks"] = std::move(bricks);
	mMaterials["checkertile"] = std::move(checkertile);
	mMaterials["icemirror"] = std::move(icemirror);
	mMaterials["skullMat"] = std::move(skullMat);
	mMaterials["shadowMat"] = std::move(shadowMat);
}

void StencilDemo::UpdateScene(float dt)
{
	OnKeyBoardInput(dt);
	UpdateCamera(dt);
	UpdateMainPassCB(dt);
	UploadObjects();
}

void StencilDemo::OnKeyBoardInput(float dt)
{
	//
	// Allow user to move skull.
	//

	if(GetAsyncKeyState('A') & 0x8000)
		mSkullTranslation.x -= 1.0f*dt;

	if(GetAsyncKeyState('D') & 0x8000)
		mSkullTranslation.x += 1.0f*dt;

	if(GetAsyncKeyState('W') & 0x8000)
		mSkullTranslation.y += 1.0f*dt;

	if(GetAsyncKeyState('S') & 0x8000)
		mSkullTranslation.y -= 1.0f*dt;

	// Update the new world matrix.
	XMMATRIX skullRotate = XMMatrixRotationY(0.5f*MathHelper::Pi);
	XMMATRIX skullScale = XMMatrixScaling(0.45f, 0.45f, 0.45f);
	XMMATRIX skullOffset = XMMatrixTranslation(mSkullTranslation.x, mSkullTranslation.y, mSkullTranslation.z);
	XMMATRIX skullWorld = skullRotate*skullScale*skullOffset;
	XMStoreFloat4x4(&mSkullRitem->World, skullWorld);
	mSkullRitem->bDirty = true;

	// Update reflection world matrix.
	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	XMMATRIX R = XMMatrixReflect(mirrorPlane);
	XMStoreFloat4x4(&mReflectedSkullRitem->World, skullWorld * R);
	mReflectedSkullRitem->bDirty = true;

	// Update shadow world matrix.
	XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
	XMVECTOR toMainLight = -XMLoadFloat3(&mMainPassCB.Lights[0].Direction);
	XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
	XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
	XMStoreFloat4x4(&mShadowedSkullRitem->World, skullWorld * S * shadowOffsetY);
	mShadowedSkullRitem->bDirty = true;
}

void StencilDemo::UpdateCamera(float dt)
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

void StencilDemo::DrawRenderItems(RenderLayer layer)
{
	auto pPSO = mPSOs[(int)layer].get();
	md3dImmediateContext->VSSetShader(pPSO->pVS, nullptr, 0);
	md3dImmediateContext->PSSetShader(pPSO->pPS, nullptr, 0);
	md3dImmediateContext->IASetInputLayout(pPSO->pIL);
	md3dImmediateContext->OMSetBlendState(
		pPSO->blend.pBlendState,
		pPSO->blend.BlendFactor.data(),
		pPSO->blend.SampleMask);
	md3dImmediateContext->OMSetDepthStencilState(
		pPSO->depthStencil.pDepthStencilState,
		pPSO->depthStencil.StencilRef);
	md3dImmediateContext->RSSetState(pPSO->pResterizer);

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

		std::array<ID3D11Buffer*, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> ConstantBuffers;
		ConstantBuffers[0] = mFrameResource->ObjectCB->Resource(ri->ObjCBIndex);
		ConstantBuffers[1] = mFrameResource->MaterialCB->Resource(ri->Mat->MatCBIndex);
		md3dImmediateContext->VSSetConstantBuffers(0, 2, ConstantBuffers.data());
		md3dImmediateContext->PSSetConstantBuffers(0, 2, ConstantBuffers.data());

		std::array<ID3D11ShaderResourceView*, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> SRV;
		auto& rTex = mTextures[ri->Mat->DiffuseSrv];
		if (rTex) {
			SRV[0] = rTex->Resource.Get();
			md3dImmediateContext->PSSetShaderResources(0, 1, SRV.data());
		}

		md3dImmediateContext->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void StencilDemo::DrawScene()
{
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&Colors::LightSteelBlue));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

	std::array<ID3D11SamplerState*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> Sampler;
	Sampler[0] = mSamplerState[4].Get();
	md3dImmediateContext->PSSetSamplers(0, 1, Sampler.data());

	std::array<ID3D11Buffer*, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> ConstantBuffers;
	ConstantBuffers[0] = mFrameResource->PassCB->Resource(0);
	md3dImmediateContext->VSSetConstantBuffers(2, 1, ConstantBuffers.data());
	md3dImmediateContext->PSSetConstantBuffers(2, 1, ConstantBuffers.data());
	DrawRenderItems(RenderLayer::Opaque);
	DrawRenderItems(RenderLayer::Texture);
	DrawRenderItems(RenderLayer::Mirrors);
	// pass buffer change to 1
	ConstantBuffers[0] = mFrameResource->PassCB->Resource(1);
	md3dImmediateContext->VSSetConstantBuffers(2, 1, ConstantBuffers.data());
	md3dImmediateContext->PSSetConstantBuffers(2, 1, ConstantBuffers.data());
	DrawRenderItems(RenderLayer::Reflected);
	// pass buffer change to 0
	ConstantBuffers[0] = mFrameResource->PassCB->Resource(0);
	md3dImmediateContext->VSSetConstantBuffers(2, 1, ConstantBuffers.data());
	md3dImmediateContext->PSSetConstantBuffers(2, 1, ConstantBuffers.data());
	DrawRenderItems(RenderLayer::Transparent);
	DrawRenderItems(RenderLayer::Shadow);

	HR(mSwapChain->Present(0, 0));
}

void StencilDemo::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void StencilDemo::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void StencilDemo::OnMouseMove(WPARAM btnState, int x, int y)
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

void StencilDemo::BuildGeometry()
{
	BuildRoomGeometryBuffers();
	BuildSkullGeometryBuffers();
}

void StencilDemo::BuildRoomGeometryBuffers()
{
	using Vertex = VertexTex;

    // Create and specify geometry.  For this sample we draw a floor
	// and a wall with a mirror on it.  We put the floor, wall, and
	// mirror geometry in one vertex buffer.
	//
	//   |--------------|
	//   |              |
    //   |----|----|----|
    //   |Wall|Mirr|Wall|
	//   |    | or |    |
    //   /--------------/
    //  /   Floor      /
	// /--------------/

	std::array<Vertex, 20> vertices = 
	{
		// Floor: Observe we tile texture coordinates.
		Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
		Vertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
		Vertex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
		Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

		// Wall: Observe we tile texture coordinates, and that we
		// leave a gap in the middle for the mirror.
		Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
		Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

		Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
		Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
		Vertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

		Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
		Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
		Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

		// Mirror
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
		Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
		Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
	};

	std::array<std::int16_t, 30> indices = 
	{
		// Floor
		0, 1, 2,	
		0, 2, 3,

		// Walls
		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		// Mirror
		16, 17, 18,
		16, 18, 19
	};

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = 6;
	floorSubmesh.StartIndexLocation = 0;
	floorSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry wallSubmesh;
	wallSubmesh.IndexCount = 18;
	wallSubmesh.StartIndexLocation = 6;
	wallSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.IndexCount = 6;
	mirrorSubmesh.StartIndexLocation = 24;
	mirrorSubmesh.BaseVertexLocation = 0;

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
	geo->DrawArgs["wall"] = wallSubmesh;
	geo->DrawArgs["mirror"] = mirrorSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}
 
void StencilDemo::BuildSkullGeometryBuffers()
{
	std::ifstream fin("Models/skull.txt");
	
	if(!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;
	
	std::vector<Vertex> vertices(vcount);
	for(UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	auto SkullIndexCount = 3*tcount;
	std::vector<UINT> indices(SkullIndexCount);
	for(UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i*3+0] >> indices[i*3+1] >> indices[i*3+2];
	}

	fin.close();

	const UINT vbByteSize = sizeof(Vertex)*vertices.size();
	const UINT ibByteSize = sizeof(UINT)*indices.size();

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skull";
	geo->VertexBufferGPU = d3dHelper::CreateVertexBuffer(
		md3dDevice, vertices.data(), vbByteSize);
	geo->IndexBufferGPU = d3dHelper::CreateIndexBuffer(
		md3dDevice, indices.data(), ibByteSize);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry skull;
	skull.IndexCount = (UINT)indices.size();
	skull.StartIndexLocation = 0;
	skull.BaseVertexLocation = 0;

	geo->DrawArgs["skullGeo"] = skull;
	mGeometries[geo->Name] = std::move(geo);
}

void StencilDemo::LoadTextures()
{
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"../../Textures/bricks3.dds";
	ThrowIfFailed(D3DX11CreateShaderResourceViewFromFile(
		md3dDevice,
		bricksTex->Filename.c_str(),
		0, 0, bricksTex->Resource.GetAddressOf(), 0));

	auto checkboardTex = std::make_unique<Texture>();
	checkboardTex->Name = "checkboardTex";
	checkboardTex->Filename = L"../../Textures/checkboard.dds";
	ThrowIfFailed(D3DX11CreateShaderResourceViewFromFile(
		md3dDevice,
		checkboardTex->Filename.c_str(),
		0, 0,
		checkboardTex->Resource.GetAddressOf(), 0));

	auto icdTex = std::make_unique<Texture>();
	icdTex->Name = "iceTex";
	icdTex->Filename = L"../../Textures/ice.dds";
	ThrowIfFailed(D3DX11CreateShaderResourceViewFromFile(
		md3dDevice,
		icdTex->Filename.c_str(),
		0, 0,
		icdTex->Resource.GetAddressOf(), 0));

	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[checkboardTex->Name] = std::move(checkboardTex);
	mTextures[icdTex->Name] = std::move(icdTex);
}

void StencilDemo::BuildRenderItems()
{
	auto floorRitem = std::make_unique<RenderItem>();
	floorRitem->World = MathHelper::Identity4x4();
	floorRitem->TexTransform = MathHelper::Identity4x4();
	floorRitem->ObjCBIndex = 0;
	floorRitem->Mat = mMaterials["checkertile"].get();
	floorRitem->Geo = mGeometries["roomGeo"].get();
	floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Texture].push_back(floorRitem.get());

    auto wallsRitem = std::make_unique<RenderItem>();
	wallsRitem->World = MathHelper::Identity4x4();
	wallsRitem->TexTransform = MathHelper::Identity4x4();
	wallsRitem->ObjCBIndex = 1;
	wallsRitem->Mat = mMaterials["bricks"].get();
	wallsRitem->Geo = mGeometries["roomGeo"].get();
	wallsRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallsRitem->IndexCount = wallsRitem->Geo->DrawArgs["wall"].IndexCount;
	wallsRitem->StartIndexLocation = wallsRitem->Geo->DrawArgs["wall"].StartIndexLocation;
	wallsRitem->BaseVertexLocation = wallsRitem->Geo->DrawArgs["wall"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Texture].push_back(wallsRitem.get());

	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->World = MathHelper::Identity4x4();
	skullRitem->TexTransform = MathHelper::Identity4x4();
	skullRitem->ObjCBIndex = 2;
	skullRitem->Mat = mMaterials["skullMat"].get();
	skullRitem->Geo = mGeometries["skull"].get();
	skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skullGeo"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skullGeo"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skullGeo"].BaseVertexLocation;
	mSkullRitem = skullRitem.get();
	mRitemLayer[(int)RenderLayer::Opaque].push_back(skullRitem.get());

	// Reflected skull will have different world matrix, so it needs to be its own render item.
	auto reflectedSkullRitem = std::make_unique<RenderItem>();
	*reflectedSkullRitem = *skullRitem;
	reflectedSkullRitem->ObjCBIndex = 3;
	mReflectedSkullRitem = reflectedSkullRitem.get();
	mRitemLayer[(int)RenderLayer::Reflected].push_back(reflectedSkullRitem.get());

	// Shadowed skull will have different world matrix, so it needs to be its own render item.
	auto shadowedSkullRitem = std::make_unique<RenderItem>();
	*shadowedSkullRitem = *skullRitem;
	shadowedSkullRitem->ObjCBIndex = 4;
	shadowedSkullRitem->Mat = mMaterials["shadowMat"].get();
	mShadowedSkullRitem = shadowedSkullRitem.get();
	mRitemLayer[(int)RenderLayer::Shadow].push_back(shadowedSkullRitem.get());

	auto mirrorRitem = std::make_unique<RenderItem>();
	mirrorRitem->World = MathHelper::Identity4x4();
	mirrorRitem->TexTransform = MathHelper::Identity4x4();
	mirrorRitem->ObjCBIndex = 5;
	mirrorRitem->Mat = mMaterials["icemirror"].get();
	mirrorRitem->Geo = mGeometries["roomGeo"].get();
	mirrorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mirrorRitem->IndexCount = mirrorRitem->Geo->DrawArgs["mirror"].IndexCount;
	mirrorRitem->StartIndexLocation = mirrorRitem->Geo->DrawArgs["mirror"].StartIndexLocation;
	mirrorRitem->BaseVertexLocation = mirrorRitem->Geo->DrawArgs["mirror"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Mirrors].push_back(mirrorRitem.get());
	mRitemLayer[(int)RenderLayer::Transparent].push_back(mirrorRitem.get());

	mAllRitems.push_back(std::move(floorRitem));
	mAllRitems.push_back(std::move(wallsRitem));
	mAllRitems.push_back(std::move(skullRitem));
	mAllRitems.push_back(std::move(reflectedSkullRitem));
	mAllRitems.push_back(std::move(shadowedSkullRitem));
	mAllRitems.push_back(std::move(mirrorRitem));
}
 
void StencilDemo::BuildFrameResources()
{
	mFrameResource = std::make_unique<FrameResource>(md3dDevice, 2, (UINT)mAllRitems.size(), (UINT)mMaterials.size());
}

void StencilDemo::UploadMaterials()
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

void StencilDemo::UploadObjects()
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

void StencilDemo::UpdateMainPassCB(float dt)
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
	mMainPassCB = MainPassCB;
	mFrameResource->PassCB->UploadData(md3dImmediateContext, 0, mMainPassCB);

	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	XMMATRIX R = XMMatrixReflect(mirrorPlane);

	mReflectedPassCB = mMainPassCB;
	// Reflect the lighting.
	for(int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mMainPassCB.Lights[i].Direction);
		XMVECTOR reflectedLightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mReflectedPassCB.Lights[i].Direction, reflectedLightDir);
	}

	// Reflected pass stored in index 1
	mFrameResource->PassCB->UploadData(md3dImmediateContext, 1, mReflectedPassCB);
}

std::array<const CD3D11_SAMPLER_DESC, 6> StencilDemo::GetStaticSamplers()
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

void StencilDemo::BuildSamplerState() 
{
	auto state = GetStaticSamplers();
	for (auto s : state) {
		ComPtr<ID3D11SamplerState> ss;
		md3dDevice->CreateSamplerState(&s, ss.GetAddressOf());
		mSamplerState.push_back(ss);
	}
}

void StencilDemo::CreateBlendState(const std::string& tag, const D3D11_BLEND_DESC & desc)
{
	ComPtr<ID3D11BlendState> BS;
	HR(md3dDevice->CreateBlendState(&desc, BS.GetAddressOf()));
	mBlendState[tag] = BS;
}

void StencilDemo::CreateDepthStencilState(const std::string& tag, const D3D11_DEPTH_STENCIL_DESC& desc)
{
	ComPtr<ID3D11DepthStencilState> DDS;
	HR(md3dDevice->CreateDepthStencilState(&desc, DDS.GetAddressOf()));
	mDepthStencilState[tag] = DDS;
}

void StencilDemo::CreateResterizerState(const std::string& tag, const D3D11_RASTERIZER_DESC& desc)
{
	ComPtr<ID3D11RasterizerState> RS;
	HR(md3dDevice->CreateRasterizerState(&desc, RS.GetAddressOf()));
	mRasterizerState[tag] = RS;
}

void StencilDemo::CreatePSO(RenderLayer layer, const PipelineStateDesc& desc)
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

void StencilDemo::BuildPSO()
{
	CD3D11_BLEND_DESC mirrorBlendState(D3D11_DEFAULT);
	mirrorBlendState.RenderTarget[0].RenderTargetWriteMask = 0;
	CreateBlendState("noWrite", mirrorBlendState);

	CD3D11_DEPTH_STENCIL_DESC mirrorDesc(D3D11_DEFAULT);
	mirrorDesc.DepthEnable      = true;
	mirrorDesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ZERO;
    mirrorDesc.DepthFunc        = D3D11_COMPARISON_LESS; 
    mirrorDesc.StencilEnable    = true;
    mirrorDesc.StencilReadMask  = 0xff;
    mirrorDesc.StencilWriteMask = 0xff;
    mirrorDesc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
	mirrorDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	mirrorDesc.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_REPLACE;
	mirrorDesc.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
	CreateDepthStencilState("markMirror", mirrorDesc);

	CD3D11_DEPTH_STENCIL_DESC reflectionsDSS(D3D11_DEFAULT);
	reflectionsDSS.DepthEnable = true;
	reflectionsDSS.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	reflectionsDSS.DepthFunc = D3D11_COMPARISON_LESS;
	reflectionsDSS.StencilEnable = true;
	reflectionsDSS.StencilReadMask = 0xff;
	reflectionsDSS.StencilWriteMask = 0xff;
	reflectionsDSS.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilFunc        = D3D11_COMPARISON_EQUAL;
	CreateDepthStencilState("reflected", reflectionsDSS);

	CD3D11_DEPTH_STENCIL_DESC noDoubleBlendDesc(D3D11_DEFAULT);
	noDoubleBlendDesc.DepthEnable      = true;
	noDoubleBlendDesc.DepthWriteMask   = D3D11_DEPTH_WRITE_MASK_ALL;
	noDoubleBlendDesc.DepthFunc        = D3D11_COMPARISON_LESS; 
	noDoubleBlendDesc.StencilEnable    = true;
    noDoubleBlendDesc.StencilReadMask  = 0xff;
    noDoubleBlendDesc.StencilWriteMask = 0xff;

	noDoubleBlendDesc.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
	noDoubleBlendDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	noDoubleBlendDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
	noDoubleBlendDesc.FrontFace.StencilFunc   = D3D11_COMPARISON_EQUAL;
	CreateDepthStencilState("noDoubleBlend", noDoubleBlendDesc);

	CD3D11_BLEND_DESC transparentDesc(D3D11_DEFAULT);
	transparentDesc.AlphaToCoverageEnable = false;
	transparentDesc.IndependentBlendEnable = false;
	transparentDesc.RenderTarget[0].BlendEnable = true;
	transparentDesc.RenderTarget[0].SrcBlend       = D3D11_BLEND_SRC_ALPHA;
	transparentDesc.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
	transparentDesc.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
	transparentDesc.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
	transparentDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	transparentDesc.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
	transparentDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	CreateBlendState("transparent", transparentDesc);

	CD3D11_RASTERIZER_DESC cullClockwiseDesc(D3D11_DEFAULT);
	cullClockwiseDesc.FillMode = D3D11_FILL_SOLID;
	cullClockwiseDesc.CullMode = D3D11_CULL_BACK;
	cullClockwiseDesc.FrontCounterClockwise = true;
	cullClockwiseDesc.DepthClipEnable = true;
	CreateResterizerState("cullClockwise", cullClockwiseDesc);

	CreatePSO(RenderLayer::Texture, { "texture", "texture", "texture" });
	CreatePSO(RenderLayer::Opaque, { "standard", "standard", "standard" });

	PipelineStateDesc mirrosdesc = { "standard", "standard", "standard" };
	mirrosdesc.BS = "noWrite";
	mirrosdesc.BlendFactor = { 0.f, 0.f, 0.f, 0.f };
	mirrosdesc.DSS = "markMirror";
	mirrosdesc.StencilRef = 1;
	CreatePSO(RenderLayer::Mirrors, mirrosdesc);

	PipelineStateDesc refleced = { "standard", "standard", "standard" };
	refleced.DSS = "reflected";
	refleced.StencilRef = 1;
	refleced.RS = "cullClockwise";
	CreatePSO(RenderLayer::Reflected, refleced);

	auto transparent = PipelineStateDesc { "texture", "texture", "texture" };
	transparent.BS = "transparent";
	transparent.BlendFactor = { 0.f, 0.f, 0.f, 0.f };
	CreatePSO(RenderLayer::Transparent, transparent);

	auto shadow = PipelineStateDesc{ "standard", "standard", "standard" };
	shadow.BS = "transparent";
	shadow.BlendFactor = { 0.f, 0.f, 0.f, 0.f };
	shadow.DSS = "noDoubleBlend";
	shadow.StencilRef = 0;
	CreatePSO(RenderLayer::Shadow, shadow);
}
