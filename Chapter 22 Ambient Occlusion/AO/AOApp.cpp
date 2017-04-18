//***************************************************************************************
// AOApp.cpp by Frank Luna (C) 2011 All Rights Reserved.
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
#include "MeshGeometry.h"
#include "Octree.h"

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

enum RenderLayer
{
	Textured = 0,
	Flat,
	Count
};

enum ShaderType
{
	VertexShader = 0,
	GeometryShader,
	PixelShader, 
	ComputeShader,
};

// Self AO 이상을 원한다면, interaction이 있는 모든 개체를 넣어서
// 수행할 수 밖에 없음. 리얼타임으로 돌릴 수 없ㄷ
template <typename T>
void BuildVertexAmbientOcclusion(
	std::vector<T>& vertices,
	const std::vector<UINT>& indices)
{
	UINT vcount = vertices.size();
	UINT tcount = indices.size()/3;

	std::vector<XMFLOAT3> positions(vcount);
	for (UINT i = 0; i < vcount; ++i) 
		positions[i] = vertices[i].Pos;

	Octree octree;
	octree.Build(positions, indices);
	
	// For each vertex, count how many triangles contain the vertex.
	std::vector<int> vertexSharedCount(vcount);
	for(UINT i = 0; i < tcount; ++i)
	{
		UINT i0 = indices[i*3+0];
		UINT i1 = indices[i*3+1];
		UINT i2 = indices[i*3+2];

		XMVECTOR v0 = XMLoadFloat3(&vertices[i0].Pos);
		XMVECTOR v1 = XMLoadFloat3(&vertices[i1].Pos);
		XMVECTOR v2 = XMLoadFloat3(&vertices[i2].Pos);

		XMVECTOR edge0 = v1 - v0;
		XMVECTOR edge1 = v2 - v0;

		XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge0, edge1));

		XMVECTOR centroid = (v0 + v1 + v2)/3.0f;

		// Offset to avoid self intersection.
		centroid += 0.001f*normal;

		const int NumSampleRays = 32;
		float numUnoccluded = 0;
		for(int j = 0; j < NumSampleRays; ++j)
		{
			XMVECTOR randomDir = MathHelper::RandHemisphereUnitVec3(normal);

			// TODO: Technically we should not count intersections that are far 
			// away as occluding the triangle, but this is OK for demo.
			if( !octree.RayOctreeIntersect(centroid, randomDir) )
			{
				numUnoccluded++;
			}
		}
		
		float ambientAccess = numUnoccluded / NumSampleRays;

		// Average with vertices that share this face.
		vertices[i0].Ambient += ambientAccess;
		vertices[i1].Ambient += ambientAccess;
		vertices[i2].Ambient += ambientAccess;

		vertexSharedCount[i0]++;
		vertexSharedCount[i1]++;
		vertexSharedCount[i2]++;
	}

	// Finish average by dividing by the number of samples we added.
	for(UINT i = 0; i < vcount; ++i)
	{
		vertices[i].Ambient /= vertexSharedCount[i];
	}
}


class AOApp : public D3DApp 
{
public:
	AOApp(HINSTANCE hInstance);
	~AOApp();

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
	void BuildGeometry();
	void BuildShapeGeometryBuffers();
	void BuildSkullGeometryBuffers();
	void BuildRenderItems();
	void AddRenderItems(
		RenderLayer layer,
		std::string geoname,
		std::string subname,
		std::string matname,
		D3D11_PRIMITIVE_TOPOLOGY primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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
	void DrawItems();
	void UpdateMainPassCB(float dt);

	ID3DBlob* GetVertexShaderBlob(std::string name);
	ID3D11VertexShader* GetVertexShader(std::string name);
	ID3DBlob* GetPixelShaderBlob(std::string name);
	ID3D11PixelShader* GetPixelShader(std::string name);
	ID3DBlob* GetComputeShaderBlob(std::string name);
	ID3D11ComputeShader* GetComputeShader(std::string name);

	std::array<const CD3D11_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	using VShader = std::pair<ComPtr<ID3D11VertexShader>, ComPtr<ID3DBlob>>;
	using PShader = std::pair<ComPtr<ID3D11PixelShader>, ComPtr<ID3DBlob>>;
	using GShader = std::pair<ComPtr<ID3D11GeometryShader>, ComPtr<ID3DBlob>>;
	using CShader = std::pair<ComPtr<ID3D11ComputeShader>, ComPtr<ID3DBlob>>;
	std::unordered_map<std::string, VShader> mVShaders;
	std::unordered_map<std::string, GShader> mGShaders;
	std::unordered_map<std::string, PShader> mPShaders;
	std::unordered_map<std::string, CShader> mCShaders;
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
	std::vector<RenderItem*> mRitemLayer[Count];
	std::vector<std::pair<int, std::string>> mRenderLayerConstantBuffer[Count];
	std::unique_ptr<PipelineStateObject> mPSOs[Count];

	std::unordered_map<std::string, ComPtr<ID3D11Buffer>> mConstantBuffers;

	ComPtr<ID3D11ShaderResourceView> mOffscreenSRV;
	ComPtr<ID3D11UnorderedAccessView> mOffscreenUAV;

	// Define transformations from local spaces to world space.
	XMFLOAT4X4 mSphereWorld[10];
	XMFLOAT4X4 mCylWorld[10];
	XMFLOAT4X4 mBoxWorld;
	XMFLOAT4X4 mGridWorld;
	XMFLOAT4X4 mSkullWorld;

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

	AOApp theApp(hInstance);
	
	if( !theApp.Init() )
		return 0;
	
	return theApp.Run();
}
 

AOApp::AOApp(HINSTANCE hInstance)
: D3DApp(hInstance),
  mEyePos(0.0f, 0.0f, 0.0f), mTheta(1.5f*MathHelper::Pi), mPhi(0.1f*MathHelper::Pi), mRadius(15.0f)
{
	mMainWndCaption = L"AO Demo";
	
	mLastMousePos.x = 0;
	mLastMousePos.y = 0;

	XMMATRIX I = XMMatrixIdentity();
	XMStoreFloat4x4(&mGridWorld, I);

	XMMATRIX boxScale = XMMatrixScaling(3.0f, 1.0f, 3.0f);
	XMMATRIX boxOffset = XMMatrixTranslation(0.0f, 0.5f, 0.0f);
	XMStoreFloat4x4(&mBoxWorld, XMMatrixMultiply(boxScale, boxOffset));

	XMMATRIX skullScale = XMMatrixScaling(0.5f, 0.5f, 0.5f);
	XMMATRIX skullOffset = XMMatrixTranslation(0.0f, 1.0f, 0.0f);
	XMStoreFloat4x4(&mSkullWorld, XMMatrixMultiply(skullScale, skullOffset));

	for(int i = 0; i < 5; ++i)
	{
		XMStoreFloat4x4(&mCylWorld[i*2+0], XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i*5.0f));
		XMStoreFloat4x4(&mCylWorld[i*2+1], XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i*5.0f));

		XMStoreFloat4x4(&mSphereWorld[i*2+0], XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i*5.0f));
		XMStoreFloat4x4(&mSphereWorld[i*2+1], XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i*5.0f));
	}
}

AOApp::~AOApp()
{
}

bool AOApp::Init()
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

void AOApp::OnResize()
{
	D3DApp::OnResize();

	mProj = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void AOApp::BuildFX()
{
	D3D_SHADER_MACRO textureMacros[] = { "TEXTURE", "1", NULL, NULL };
	CompileShader(VertexShader, "texture", textureMacros, L"FX/Basic.fx", "VS", "vs_5_0");
	CompileShader(PixelShader, "texture", textureMacros, L"FX/Basic.fx", "PS", "ps_5_0");
	CompileShader(VertexShader, "flat", nullptr, L"FX/Basic.fx", "VS", "vs_5_0");
	CompileShader(PixelShader, "flat", nullptr, L"FX/Basic.fx", "PS", "ps_5_0");
}

void AOApp::CompileShader(
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
	case ComputeShader:
		{
			ComPtr<ID3D11ComputeShader> shader;
			md3dDevice->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, shader.GetAddressOf());
			mCShaders.insert({ name, { shader, blob } });
			break;
		}
	}
}

ID3DBlob* AOApp::GetVertexShaderBlob(std::string name)
{
	return mVShaders[name].second.Get();
}

ID3D11VertexShader* AOApp::GetVertexShader(std::string name)
{
	return mVShaders[name].first.Get();
}

ID3D11PixelShader * AOApp::GetPixelShader(std::string name)
{
	return mPShaders[name].first.Get();
}

ID3D11ComputeShader* AOApp::GetComputeShader(std::string name)
{
	return mCShaders[name].first.Get();
}

void AOApp::BuildVertexLayout()
{
	// Create the vertex input layout.
	std::vector<D3D11_INPUT_ELEMENT_DESC> ILTex = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"AMBIENT",  0, DXGI_FORMAT_R32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	ComPtr<ID3D11InputLayout> pLayoutTex;
	HR(md3dDevice->CreateInputLayout(
		ILTex.data(),
		ILTex.size(),
		GetVertexShaderBlob("texture")->GetBufferPointer(),
		GetVertexShaderBlob("texture")->GetBufferSize(),
		pLayoutTex.GetAddressOf()));
	mInputLayout["texture"] = pLayoutTex;

	// Create the vertex input layout.
	std::vector<D3D11_INPUT_ELEMENT_DESC> IL = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"AMBIENT",  0, DXGI_FORMAT_R32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	ComPtr<ID3D11InputLayout> pLayout;
	HR(md3dDevice->CreateInputLayout(
		IL.data(),
		IL.size(),
		GetVertexShaderBlob("flat")->GetBufferPointer(),
		GetVertexShaderBlob("flat")->GetBufferSize(),
		pLayout.GetAddressOf()));
	mInputLayout["flat"] = pLayout;
}

void AOApp::BuildMaterials()
{
	auto bricks0 = std::make_unique<MaterialFresnel>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrv = "bricksTex";
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto stone0 = std::make_unique<MaterialFresnel>();
	stone0->Name = "stone0";
	stone0->MatCBIndex = 1;
	stone0->DiffuseSrv = "stoneTex";
	stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;
 
	auto tile0 = std::make_unique<MaterialFresnel>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 2;
	tile0->DiffuseSrv = "tileTex";
	tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.2f;

	auto skullMat = std::make_unique<MaterialFresnel>();
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.3f;
	
	auto screenMat = std::make_unique<MaterialFresnel>();
	screenMat->MatCBIndex = 4;
	screenMat->Name = "screen";
	screenMat->DiffuseSrv = "screen";

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["stone0"] = std::move(stone0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["skullMat"] = std::move(skullMat);
	mMaterials["screenMat"] = std::move(screenMat);
}

void AOApp::UpdateScene(float dt)
{
	OnKeyBoardInput(dt);
	UpdateCamera(dt);
	UpdateMainPassCB(dt);
	UploadObjects();
}

void AOApp::OnKeyBoardInput(float dt)
{
}

void AOApp::UpdateCamera(float dt)
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

void AOApp::DrawRenderItems(RenderLayer layer)
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
	auto& cbuffers = mRenderLayerConstantBuffer[(int)layer];
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

		std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> SRV;
		auto textureName = ri->Mat->DiffuseSrv;
		if (!textureName.empty())
		{
			assert(mTextures.count(textureName));
			SRV[0] = mTextures[textureName]->Resource.Get();
			md3dImmediateContext->PSSetShaderResources(0, 1, SRV.data());
		}

		md3dImmediateContext->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

void AOApp::DrawItems()
{
	DrawRenderItems(RenderLayer::Flat);
	DrawRenderItems(RenderLayer::Textured);
}

void AOApp::DrawScene()
{
	std::array<ID3D11SamplerState*, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT> Sampler;
	Sampler[0] = mSamplerState[4].Get();
	md3dImmediateContext->PSSetSamplers(0, 1, Sampler.data());

	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, reinterpret_cast<const float*>(&Colors::LightSteelBlue));
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL, 1.0f, 0);

	DrawItems();

	HR(mSwapChain->Present(0, 0));
}

void AOApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void AOApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void AOApp::OnMouseMove(WPARAM btnState, int x, int y)
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

void AOApp::BuildGeometry()
{
	BuildShapeGeometryBuffers();
	BuildSkullGeometryBuffers();
}

void AOApp::BuildShapeGeometryBuffers()
{
	GeometryGenerator::MeshData box;
	GeometryGenerator::MeshData grid;
	GeometryGenerator::MeshData sphere;
	GeometryGenerator::MeshData cylinder;

	GeometryGenerator geoGen;
	geoGen.CreateBox(1.0f, 1.0f, 1.0f, box);
	geoGen.CreateGrid(20.0f, 30.0f, 60, 40, grid);
	geoGen.CreateSphere(0.5f, 20, 20, sphere);
	geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20, cylinder);

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	auto boxVertexOffset = 0;
	auto gridVertexOffset = box.Vertices.size();
	auto sphereVertexOffset = gridVertexOffset + grid.Vertices.size();
	auto cylinderVertexOffset = sphereVertexOffset + sphere.Vertices.size();

	// Cache the index count of each object.
	auto boxIndexCount = box.Indices.size();
	auto gridIndexCount = grid.Indices.size();
	auto sphereIndexCount = sphere.Indices.size();
	auto cylinderIndexCount = cylinder.Indices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	auto boxIndexOffset = 0;
	auto gridIndexOffset = boxIndexCount;
	auto sphereIndexOffset = gridIndexOffset + gridIndexCount;
	auto cylinderIndexOffset = sphereIndexOffset + sphereIndexCount;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;
	
	UINT totalVertexCount = 
		box.Vertices.size() + 
		grid.Vertices.size() + 
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	UINT totalIndexCount = 
		boxSubmesh.IndexCount + 
		gridSubmesh.IndexCount + 
		sphereSubmesh.IndexCount +
		cylinderSubmesh.IndexCount;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//
	std::vector<VertexTexAO> vertices(totalVertexCount);

	UINT k = 0;
	for(size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos    = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].Tex    = box.Vertices[i].TexC;
		vertices[k].Ambient = 1.0f;
	}

	for(size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos    = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].Tex    = grid.Vertices[i].TexC;
		vertices[k].Ambient = 1.0f;
	}

	for(size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos    = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].Tex    = sphere.Vertices[i].TexC;
		vertices[k].Ambient = 1.0f;
	}

	for(size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos    = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].Tex    = cylinder.Vertices[i].TexC;
		vertices[k].Ambient = 1.0f;
	}

	//
	// Pack the indices of all the meshes into one index buffer.
	//
	std::vector<UINT> indices;
	indices.insert(indices.end(), box.Indices.begin(), box.Indices.end());
	indices.insert(indices.end(), grid.Indices.begin(), grid.Indices.end());
	indices.insert(indices.end(), sphere.Indices.begin(), sphere.Indices.end());
	indices.insert(indices.end(), cylinder.Indices.begin(), cylinder.Indices.end());

	const UINT vbByteSize = sizeof(VertexTexAO)*vertices.size();
	const UINT ibByteSize = sizeof(UINT)*indices.size();

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";
	geo->VertexBufferGPU = d3dHelper::CreateVertexBuffer(
		md3dDevice, vertices.data(), vbByteSize);
	geo->IndexBufferGPU = d3dHelper::CreateIndexBuffer(
		md3dDevice, indices.data(), ibByteSize);

	geo->VertexByteStride = sizeof(VertexTexAO);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}
 
void AOApp::BuildSkullGeometryBuffers()
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
	
	std::vector<VertexAO> vertices(vcount);
	for(UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
		vertices[i].Ambient = 1.0f;
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

	BuildVertexAmbientOcclusion(vertices, indices);

	const UINT vbByteSize = sizeof(VertexAO)*vertices.size();
	const UINT ibByteSize = sizeof(UINT)*indices.size();

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skull";
	geo->VertexBufferGPU = d3dHelper::CreateVertexBuffer(
		md3dDevice, vertices.data(), vbByteSize);
	geo->IndexBufferGPU = d3dHelper::CreateIndexBuffer(
		md3dDevice, indices.data(), ibByteSize);

	geo->VertexByteStride = sizeof(VertexAO);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry skull;
	skull.IndexCount = (UINT)indices.size();
	skull.StartIndexLocation = 0;
	skull.BaseVertexLocation = 0;

	geo->DrawArgs["skull"] = skull;
	mGeometries[geo->Name] = std::move(geo);
}

void AOApp::LoadTextures()
{
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"../../Textures/bricks.dds";
	ThrowIfFailed(D3DX11CreateShaderResourceViewFromFile(
		md3dDevice,
		bricksTex->Filename.c_str(),
		0, 0, bricksTex->Resource.GetAddressOf(), 0));

	auto stoneTex = std::make_unique<Texture>();
	stoneTex->Name = "stoneTex";
	stoneTex->Filename = L"../../Textures/stone.dds";
	ThrowIfFailed(D3DX11CreateShaderResourceViewFromFile(
		md3dDevice,
		stoneTex->Filename.c_str(),
		0, 0,
		stoneTex->Resource.GetAddressOf(), 0));

	auto tileTex = std::make_unique<Texture>();
	tileTex->Name = "tileTex";
	tileTex->Filename = L"../../Textures/tile.dds";
	ThrowIfFailed(D3DX11CreateShaderResourceViewFromFile(
		md3dDevice,
		tileTex->Filename.c_str(),
		0, 0,
		tileTex->Resource.GetAddressOf(), 0));

	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[stoneTex->Name] = std::move(stoneTex);
	mTextures[tileTex->Name] = std::move(tileTex);
}

void AOApp::BuildRenderItems()
{
	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->ObjCBIndex = 0;
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->World = mGridWorld;
	gridRitem->Mat = mMaterials["tile0"].get();
	mRitemLayer[RenderLayer::Textured].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));

	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->ObjCBIndex = 1;
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->World = mBoxWorld;
	boxRitem->Mat = mMaterials["stone0"].get();
	mRitemLayer[RenderLayer::Textured].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));

	XMMATRIX brickTexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	UINT objCBIndex = 2;
	for(int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i*5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i*5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i*5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i*5.0f);

		XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		XMStoreFloat4x4(&leftCylRitem->TexTransform, brickTexTransform);
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Mat = mMaterials["bricks0"].get();
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
		leftCylRitem->World = mCylWorld[2*i];

		XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		XMStoreFloat4x4(&rightCylRitem->TexTransform, brickTexTransform);
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Mat = mMaterials["bricks0"].get();
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
		rightCylRitem->World = mCylWorld[2*i+1];

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->TexTransform = MathHelper::Identity4x4();
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Mat = mMaterials["stone0"].get();
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
		leftSphereRitem->World = mSphereWorld[2*i];

		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Mat = mMaterials["stone0"].get();
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
		rightSphereRitem->World = mSphereWorld[2*i+1];

		mRitemLayer[Textured].push_back(leftCylRitem.get());
		mRitemLayer[Textured].push_back(rightCylRitem.get());
		mRitemLayer[Textured].push_back(leftSphereRitem.get());
		mRitemLayer[Textured].push_back(rightSphereRitem.get());

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}

	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->ObjCBIndex = 2;
	skullRitem->Geo = mGeometries["skull"].get();
	skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
	skullRitem->World = mSkullWorld;
	skullRitem->Mat = mMaterials["skullMat"].get();
	mRitemLayer[RenderLayer::Flat].push_back(skullRitem.get());
	mAllRitems.push_back(std::move(skullRitem));
}

void AOApp::AddRenderItems(
	RenderLayer layer,
	std::string geoname,
	std::string subname,
	std::string matname,
	D3D11_PRIMITIVE_TOPOLOGY primitiveType)
{
	auto item = std::make_unique<RenderItem>();
	item->ObjCBIndex = mAllRitems.size();
	assert(mGeometries.count(geoname));
	item->Geo = mGeometries[geoname].get();
	item->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	assert(item->Geo->DrawArgs.count(subname));
	item->IndexCount = item->Geo->DrawArgs[subname].IndexCount;
	item->StartIndexLocation = item->Geo->DrawArgs[subname].StartIndexLocation;
	item->BaseVertexLocation = item->Geo->DrawArgs[subname].BaseVertexLocation;
	if (mMaterials.count(matname))
		item->Mat = mMaterials[matname].get();

	mRitemLayer[layer].push_back(item.get());
	mAllRitems.push_back(std::move(item));
}
 
void AOApp::BuildFrameResources()
{
	mFrameResource = std::make_unique<FrameResource>(md3dDevice, 1, (UINT)mAllRitems.size(), (UINT)mMaterials.size());

	mConstantBuffers["MainPass"] = mFrameResource->PassCB->Resource(0);
	for (int i = 0; i < (int)RenderLayer::Count; i++)
		mRenderLayerConstantBuffer[i].push_back({ 2, "MainPass" });

	for (size_t i = 0; i < mAllRitems.size(); i++) {
		XMMATRIX world = XMLoadFloat4x4(&mAllRitems[i]->World);
		XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);

		ObjectConstants obj;
		obj.World = world;
		obj.WorldInvTranspose = worldInvTranspose;
		obj.TexTransform = XMLoadFloat4x4(&mAllRitems[i]->TexTransform);
		mFrameResource->ObjectCB->UploadData(md3dImmediateContext, mAllRitems[i]->ObjCBIndex, obj);
	}

	for (auto& m : mMaterials) {
		auto mat = m.second.get();
		MaterialConstants matConstants;
		matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
		matConstants.FresnelR0 = mat->FresnelR0;
		matConstants.Roughness = mat->Roughness;
		mFrameResource->MaterialCB->UploadData(md3dImmediateContext, mat->MatCBIndex, matConstants);
	}
}

void AOApp::UpdateMainPassCB(float dt)
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
}
 
void AOApp::UploadMaterials()
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

void AOApp::UploadObjects()
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

std::array<const CD3D11_SAMPLER_DESC, 6> AOApp::GetStaticSamplers()
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

void AOApp::BuildSamplerState() 
{
	auto state = GetStaticSamplers();
	for (auto s : state) {
		ComPtr<ID3D11SamplerState> ss;
		md3dDevice->CreateSamplerState(&s, ss.GetAddressOf());
		mSamplerState.push_back(ss);
	}
}

void AOApp::CreateBlendState(const std::string& tag, const D3D11_BLEND_DESC & desc)
{
	ComPtr<ID3D11BlendState> BS;
	HR(md3dDevice->CreateBlendState(&desc, BS.GetAddressOf()));
	mBlendState[tag] = BS;
}

void AOApp::CreateDepthStencilState(const std::string& tag, const D3D11_DEPTH_STENCIL_DESC& desc)
{
	ComPtr<ID3D11DepthStencilState> DDS;
	HR(md3dDevice->CreateDepthStencilState(&desc, DDS.GetAddressOf()));
	mDepthStencilState[tag] = DDS;
}

void AOApp::CreateResterizerState(const std::string& tag, const D3D11_RASTERIZER_DESC& desc)
{
	ComPtr<ID3D11RasterizerState> RS;
	HR(md3dDevice->CreateRasterizerState(&desc, RS.GetAddressOf()));
	mRasterizerState[tag] = RS;
}

void AOApp::CreatePSO(RenderLayer layer, const PipelineStateDesc& desc)
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

void AOApp::BuildPSO()
{
	CreatePSO(RenderLayer::Textured, { "texture", "texture", "texture" });
	CreatePSO(RenderLayer::Flat, { "flat", "flat", "flat" });
}
