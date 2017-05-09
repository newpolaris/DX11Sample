//***************************************************************************************
// AOApp.cpp by Frank Luna (C) 2011 All Rights Reserved.
//
// Controls:
//		Hold the left mouse button down and move the mouse to rotate.
//      Hold the right mouse button down to zoom in and out.
//
//***************************************************************************************

#include <random>
#include "d3dApp.h"
#include "d3dx11Effect.h"
#include "GeometryGenerator.h"
#include "MathHelper.h"
#include "LightHelper.h"
#include "FrameResource.h"
#include "PipelineStateObject.h"
#include "MeshGeometry.h"
#include "RenderTarget.h"
#include "TextureResource.h"
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "Shader.h"
#include "Scene3D.h"
#include "SceneBinFile.h"
#include "Camera.h"

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

// Captures from the NVIDIA Medusa demo (Linear depth and back-buffer colors)
BinFileDescriptor g_BinFileDesc[] = 
{
    { L"Medusa 1",   L"..\\..\\Media\\SSAO11\\Medusa_Warrior.bin" },   // 720p
    { L"Medusa 2",   L"..\\..\\Media\\SSAO11\\Medusa_Snake.bin" },     // 720p
    { L"Medusa 3",   L"..\\..\\Media\\SSAO11\\Medusa_Monster.bin" },   // 1080p
};

MeshDescriptor g_MeshDesc[] =
{
    {L"AT-AT",     L"..\\..\\Media\\AT-AT.sdkmesh",   SCENE_USE_GROUND_PLANE, SCENE_NO_SHADING,  SCENE_USE_ORBITAL_CAMERA, 0.05f },
    {L"Leaves",    L"..\\..\\Media\\leaves.sdkmesh",  SCENE_NO_GROUND_PLANE,  SCENE_USE_SHADING, SCENE_USE_ORBITAL_CAMERA, 0.05f },
    {L"Sibenik",   L"..\\..\\Media\\sibenik.sdkmesh", SCENE_NO_GROUND_PLANE,  SCENE_USE_SHADING,  SCENE_USE_FIRST_PERSON_CAMERA, 0.005f },
    {L"Knight",    L"..\\..\\Media\\goof_knight.sdkmesh",   SCENE_NO_GROUND_PLANE,  SCENE_USE_SHADING,  SCENE_USE_FIRST_PERSON_CAMERA, 0.05f },
    {L"Test",      L"..\\..\\Media\\FenceSsaoTestScene.sdkmesh",   SCENE_NO_GROUND_PLANE,  SCENE_USE_SHADING,  SCENE_USE_FIRST_PERSON_CAMERA, 0.05f },
};


Color ConvertColor(const XMVECTORF32& vec) {
	return Color(vec.f[0], vec.f[1], vec.f[2], vec.f[3]);
}

#define NUM_BIN_FILES   (sizeof(g_BinFileDesc)  / sizeof(g_BinFileDesc[0]))
#define NUM_MESHES      (sizeof(g_MeshDesc)     / sizeof(g_MeshDesc[0]))

SceneMesh g_SceneMeshes[NUM_MESHES];
SceneBinFile g_SceneBinFiles[NUM_BIN_FILES];
SceneRenderer g_pSceneRenderer;

static int g_CurrentSceneId = 3;
static float gRaidus = 0.5;

struct Scene
{
    Scene()
        : pMesh(NULL)
        , pBinFile(NULL)
    {
    }
    SceneMesh *pMesh;
    SceneBinFile *pBinFile;
};
Scene g_Scenes[NUM_MESHES+NUM_BIN_FILES];

class SSAOApp : public D3DApp 
{
public:
	SSAOApp(HINSTANCE hInstance);
	~SSAOApp();

	bool Init();
	void OnResize();
	void UpdateScene(float dt);
	void DrawScene();
	void BlurAmbientMap(UINT nCount);
	void Blur1D(bool bHorzBlur);
	void ShaderCheckResource(ShaderType Type, D3D_SHADER_INPUT_TYPE InputType, UINT Slot, std::string Name);
	void DrawSceneWithSSAO();
	void ComputeSSAO();
	void OnMouseDown(WPARAM btnState, int x, int y);
	void OnMouseUp(WPARAM btnState, int x, int y);
	void OnMouseMove(WPARAM btnState, int x, int y);
	void OnKeyBoardInput(float dt);

private:
	void BuildViewDependentResource();
	void CompileShader(ShaderType Type, std::string Name, D3D_SHADER_MACRO * Defines, const std::wstring & Filename, const std::string & Function, const std::string & Model);
	void CreatePSO(const std::string & tag, const PipelineStateDesc & desc);
	RenderTargetPtr CreateRenderTarget(std::string name);
	ColorBufferPtr CreateColorBuffer(std::string name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format, D3D11_SUBRESOURCE_DATA* Data = nullptr);
	DepthBufferPtr CreateDepthBuffer(std::string name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format);
	void CreateSampler(std::string name, const CD3D11_SAMPLER_DESC & desc);
	void BuildFX();
	void CreateBlendState(const std::string& tag, const D3D11_BLEND_DESC & desc);
	void CreateDepthStencilState(const std::string& tag, const D3D11_DEPTH_STENCIL_DESC& desc);
	void CreateResterizerState(const std::string & tag, const D3D11_RASTERIZER_DESC & desc);
	void BuildPSO();
	void UpdateCamera(float dt);
	void BuildStaticSamplers();
	void ApplyPipelineState(const std::string& tag);
	void FrameRender(double fTime, float fElapsedTime, void * pUserContext);

	ID3D11SamplerState * GetSampler(const std::string & name);
	ID3DBlob* GetShaderBlob(ShaderType Type, std::string Name);

private:
	std::unordered_map<std::string, ShaderPtr> m_Shaders[NumShader];
	std::unordered_map<std::string, ComPtr<ID3D11BlendState>> mBlendState;
	std::unordered_map<std::string, ComPtr<ID3D11DepthStencilState>> mDepthStencilState;
	std::unordered_map<std::string, ComPtr<ID3D11InputLayout>> mInputLayout;
	std::unordered_map<std::string, ComPtr<ID3D11RasterizerState>> mRasterizerState;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<MaterialFresnel>> mMaterials;
	std::unordered_map<std::string, TextureResourcePtr> mTextures;
	std::unordered_map<std::string, ColorBufferPtr> mColors;
	std::unordered_map<std::string, DepthBufferPtr> mDepths;
	std::unordered_map<std::string, RenderTargetPtr> mRenderTargets;
    std::unique_ptr<FrameResource> mFrameResource;
	std::unordered_map<std::string, ComPtr<ID3D11SamplerState>> m_SamplerState;

	ColorBufferPtr m_RenderTargetBuffer;

	ComPtr<ID3D11Buffer> m_ScreenVertex, m_ScreenIndex;

	std::unordered_map<std::string, std::unique_ptr<PipelineStateObject>> mPSOs;
	std::unordered_map<std::string, ComPtr<ID3D11Buffer>> mConstantBuffers;

	D3D11_VIEWPORT m_HalfViewport;
	PipelineStateObject* m_pCurrentPSO = nullptr;

	// Define transformations from local spaces to world space.
	XMFLOAT4X4 mSphereWorld[10];
	XMFLOAT4X4 mCylWorld[10];
	XMFLOAT4X4 mBoxWorld;
	XMFLOAT4X4 mGridWorld;
	XMFLOAT4X4 mSkullWorld;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMMATRIX mView = XMMatrixIdentity();
	XMMATRIX mProj = XMMatrixIdentity();

	Camera mCam;
	int mState = 1;

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
				   PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
	SSAOApp theApp(hInstance);

	if (!theApp.Init())
		return 0;
	return theApp.Run();
}
 

SSAOApp::SSAOApp(HINSTANCE hInstance)
: D3DApp(hInstance),
  mEyePos(0.0f, 0.0f, 0.0f)
{
	mMainWndCaption = L"SSAO Demo";
	
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
	m_RenderTargetBuffer = std::make_shared<ColorBuffer>();

    D3DXVECTOR3 sibenikVecEye(0.025372846f, -0.059336402f, -0.057159711f);
    D3DXVECTOR3 sibenikVecAt (0.68971950f, -0.61976534f ,0.43737334f);

	XMFLOAT3 eye(0.025372846f, -0.059336402f, -0.057159711f);
	XMFLOAT3 target(0.68971950f, -0.61976534f ,0.43737334f);
	XMFLOAT3 up(0.f, 1.0, 0.0);
	mCam.LookAt(eye, target, up);
}

SSAOApp::~SSAOApp()
{
}

bool SSAOApp::Init()
{
	if (!D3DApp::Init())
		return false;

	BuildStaticSamplers();
	BuildFX();
	BuildViewDependentResource();
	BuildPSO();

	return true;
}

void SSAOApp::OnResize()
{
	m_RenderTargetBuffer->Destroy();

	D3DApp::OnResize();

	// Resize the swap chain and recreate the render target view.

	ComPtr<ID3D11Texture2D> RenderTargetBuffer;

	HR(mSwapChain->GetBuffer(0, IID_PPV_ARGS(&RenderTargetBuffer)));
	m_RenderTargetBuffer->CreateFromSwapChain("Screen", RenderTargetBuffer);
	m_RenderTargetBuffer->SetColor(ConvertColor(Colors::LightSteelBlue));

	auto HalfWidth = static_cast<uint32_t>(mClientWidth * 1.0);
	auto HalfHeight = static_cast<uint32_t>(mClientHeight * 1.0);

	// TextureResize
	if (mColors.count("normal"))
		mColors["normal"]->Resize(mClientWidth, mClientHeight);
	if (mColors.count("AmbientBuffer0"))
		mColors["AmbientBuffer0"]->Resize(HalfWidth, HalfHeight);
	if (mColors.count("AmbientBuffer1"))
		mColors["AmbientBuffer1"]->Resize(HalfWidth, HalfHeight);
	if (mDepths.count("depthBuffer"))
		mDepths["depthBuffer"]->Resize(mClientWidth, mClientHeight);

	m_HalfViewport.TopLeftX = 0;
	m_HalfViewport.TopLeftY = 0;
	m_HalfViewport.Width    = HalfWidth;
	m_HalfViewport.Height    = HalfHeight;
	m_HalfViewport.MinDepth = 0.0f;
	m_HalfViewport.MaxDepth = 1.0f;

	g_pSceneRenderer.OnDestroyDevice();

    // Load Scene3D.fx
    g_pSceneRenderer.OnCreateDevice(md3dDevice);

    // Load the scene bin files
    int SceneId = 0;
    for (int i = 0; i < NUM_BIN_FILES; ++i)
    {
        if (FAILED(g_SceneBinFiles[i].OnCreateDevice(g_BinFileDesc[i])))
        {
            MessageBox(NULL, L"Unable to load data file", L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
            PostQuitMessage(0);
        }
        g_Scenes[SceneId++].pBinFile = &g_SceneBinFiles[i];
    }

    // Load the sdkmesh data files
    for (int i = 0; i < NUM_MESHES; ++i)
    {
        if (FAILED(g_SceneMeshes[i].OnCreateDevice(md3dDevice, g_MeshDesc[i])))
        {
            MessageBox(NULL, L"Unable to create mesh", L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
            PostQuitMessage(0);
        }
        g_Scenes[SceneId++].pMesh = &g_SceneMeshes[i];
    }


    // Resize the bin-file data
    for (int i = 0; i < NUM_BIN_FILES; ++i)
    {
        g_SceneBinFiles[i].OnResizedSwapChain(md3dDevice, mClientWidth, mClientHeight);
    }
	mCam.SetLens(0.05f*MathHelper::Pi, AspectRatio(), 0.1f, 1000000.0f);
}


void SSAOApp::BuildFX()
{
	CompileShader(VertexShader, "ssao", nullptr, L"FX/SSAO.hlsl", "VS", "vs_5_0");
	CompileShader(PixelShader, "ssao", nullptr, L"FX/SSAO.hlsl", "PS", "ps_5_0");
	CompileShader(VertexShader, "blur", nullptr, L"FX/SSAOBlur.hlsl", "VS", "vs_5_0");
	CompileShader(PixelShader, "blur", nullptr, L"FX/SSAOBlur.hlsl", "PS", "ps_5_0");
	CompileShader(VertexShader, "composite", nullptr, L"FX/Composite.hlsl", "VS", "vs_5_0");
	CompileShader(PixelShader, "composite", nullptr, L"FX/Composite.hlsl", "PS", "ps_5_0");
}

void SSAOApp::CompileShader(
	ShaderType Type,
	std::string Name, 
	D3D_SHADER_MACRO* Defines, 
	const std::wstring & Filename,
	const std::string& Function,
	const std::string& Model)
{
	auto Shader = std::make_shared<ShaderDX11>(Type, Name, Defines, Filename, Function, Model);
	m_Shaders[Type].insert_or_assign(Name, Shader);
}

ID3DBlob* SSAOApp::GetShaderBlob(ShaderType Type, std::string Name)
{
	assert(m_Shaders[Type].count( Name ));
	return m_Shaders[Type][Name]->m_CompiledShader.Get();
}

void SSAOApp::UpdateScene(float dt)
{
	OnKeyBoardInput(dt);
	UpdateCamera(dt);

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
		std::wostringstream outs;
		outs.precision(6);
		outs << mMainWndCaption << L"    "
			<< L"FPS: " << fps << L"    "
			<< L"Frame Time: " << mspf << L" (ms)    "
			<< L"X: " << mCam.GetPosition().x << L", "
			<< L"Y: " << mCam.GetPosition().y << L", "
			<< L"Z: " << mCam.GetPosition().y << L", "
			<< L"Radius " << gRaidus;
		SetWindowText(mhMainWnd, outs.str().c_str());
	}
}

void SSAOApp::OnKeyBoardInput(float dt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mState = 1;
	if (GetAsyncKeyState('2') & 0x8000)
		mState = 2;

	if (GetAsyncKeyState('3') & 0x8000)
		g_CurrentSceneId = 3;
	if (GetAsyncKeyState('4') & 0x8000)
		g_CurrentSceneId = 4;
	if (GetAsyncKeyState('5') & 0x8000)
		g_CurrentSceneId = 5;
	if (GetAsyncKeyState('6') & 0x8000)
		g_CurrentSceneId = 6;
	if (GetAsyncKeyState('7') & 0x8000)
		g_CurrentSceneId = 7;

	if (GetAsyncKeyState('F') & 0x8000)
		gRaidus -= 0.05*dt;
	if (GetAsyncKeyState('G') & 0x8000)
		gRaidus += 0.05*dt;

	if( GetAsyncKeyState('W') & 0x8000 )
		mCam.Walk(1.0f*dt);

	if( GetAsyncKeyState('S') & 0x8000 )
		mCam.Walk(-1.0f*dt);

	if( GetAsyncKeyState('A') & 0x8000 )
		mCam.Strafe(-1.0f*dt);

	if( GetAsyncKeyState('D') & 0x8000 )
		mCam.Strafe(1.0f*dt);

	if (GetAsyncKeyState('Z') & 0x8000)
		mCam.Zoom(dt*0.1);

	if( GetAsyncKeyState('X') & 0x8000 )
		mCam.Zoom(-dt*0.1);
}

void SSAOApp::UpdateCamera(float dt)
{
	mCam.UpdateViewMatrix();
	mView = mCam.View();
	mProj = mCam.Proj();
}

void SSAOApp::ApplyPipelineState(const std::string& tag)
{
	ID3D11ShaderResourceView* pSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { 0 };
    md3dImmediateContext->VSSetShaderResources(0, ARRAYSIZE(pSRVs), pSRVs);
    md3dImmediateContext->GSSetShaderResources(0, ARRAYSIZE(pSRVs), pSRVs);
    md3dImmediateContext->PSSetShaderResources(0, ARRAYSIZE(pSRVs), pSRVs);
    md3dImmediateContext->CSSetShaderResources(0, ARRAYSIZE(pSRVs), pSRVs);

	assert(mPSOs.count(tag));
	auto pPSO = mPSOs[tag].get();

	for (int i = 0; i < NumShader; i++)
	{
		if (!pPSO->m_Shaders[i]) 
			continue;

		auto Type = static_cast<ShaderType>(i);
		switch (Type) {
		case VertexShader:
			{
				auto pVS = reinterpret_cast<ID3D11VertexShader*>(pPSO->m_Shaders[VertexShader]->m_Shader.Get());
				md3dImmediateContext->VSSetShader(pVS, nullptr, 0);
				break;
			}
		case PixelShader:
			{
				auto pPS = reinterpret_cast<ID3D11PixelShader*>(pPSO->m_Shaders[PixelShader]->m_Shader.Get());
				md3dImmediateContext->PSSetShader(pPS, nullptr, 0);
				break;
			}
		}
	}

	md3dImmediateContext->IASetInputLayout(pPSO->pIL);
	md3dImmediateContext->OMSetBlendState(
		pPSO->blend.pBlendState,
		pPSO->blend.BlendFactor.data(),
		pPSO->blend.SampleMask);
	md3dImmediateContext->OMSetDepthStencilState(
		pPSO->depthStencil.pDepthStencilState,
		pPSO->depthStencil.StencilRef);
	md3dImmediateContext->RSSetState(pPSO->pResterizer);
	if (pPSO->pRenderTarget)
		pPSO->pRenderTarget->Bind();

	m_pCurrentPSO = pPSO;
}

ID3D11SamplerState* SSAOApp::GetSampler(const std::string& name)
{
	 assert(m_SamplerState.count(name));
	 return m_SamplerState[name].Get();
}

void SSAOApp::FrameRender(double fTime, float fElapsedTime, void* pUserContext)
{
    SceneMesh *pMesh = g_Scenes[g_CurrentSceneId].pMesh;
    SceneBinFile *pBinFile = g_Scenes[g_CurrentSceneId].pBinFile;

    md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // md3dImmediateContext->RSSetViewports(1, &g_Viewport);

	// m_RenderTargetBuffer->Clear();
	// mDepths["depthBuffer"]->Clear();

    //--------------------------------------------------------------------------------------
    // Render colors and depths
    //--------------------------------------------------------------------------------------
    if (pMesh)
    {
        // Clear render target and depth buffer
        float BgColor[4] = { 1.0f, 1.0f, 1.0f };

		// ID3D11RenderTargetView* ColorRTV = { m_RenderTargetBuffer->GetRTV() };
        // Render color and depth with the Scene3D class
        // md3dImmediateContext->OMSetRenderTargets(1, &ColorRTV, mDepths["depthBuffer"]->GetDSV());
		bool g_UseOrbitalCamera = false;
        if (g_UseOrbitalCamera)
        {
			/*
            g_pSceneRenderer.OnFrameRender(g_OrbitalCamera.GetWorldMatrix(),
                                           g_OrbitalCamera.GetViewMatrix(),
                                           g_OrbitalCamera.GetProjMatrix(),
                                           pMesh);
			*/
        }
        else
        {
			XMFLOAT3 up(1, 0, 0.f);
			XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&up), XM_PI*1.5);
            D3DXMATRIX IdentityMatrix((float*)&R);
			if (g_CurrentSceneId != 5)
				D3DXMatrixIdentity(&IdentityMatrix);
            g_pSceneRenderer.OnFrameRender(&IdentityMatrix,
				(D3DXMATRIX*)&mView,
				(D3DXMATRIX*)&mProj,
				pMesh);
        }

		/*
        if (g_MSAADesc[g_MSAACurrentSettings].samples > 1)
        {
            pd3dImmediateContext->ResolveSubresource(g_NonMSAAColorTexture, 0, g_ColorTexture, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        }
        else
        {
            pd3dImmediateContext->CopyResource(g_NonMSAAColorTexture, g_ColorTexture);
        }
		*/
    }

    //--------------------------------------------------------------------------------------
    // Render the SSAO to the backbuffer
    //--------------------------------------------------------------------------------------

	ID3D11RenderTargetView* ColorRTV = { m_RenderTargetBuffer->GetRTV() };
	// Render color and depth with the Scene3D class
	// md3dImmediateContext->OMSetRenderTargets(1, &ColorRTV, mDepths["depthBuffer"]->GetDSV());
	/*
    ID3D11RenderTargetView* pBackBufferRTV = DXUTGetD3D11RenderTargetView(); // does not addref
    pd3dImmediateContext->OMSetRenderTargets(1, &pBackBufferRTV, NULL);
	*/
	bool g_ShowColors = true;
    if (g_ShowColors)
    {
        // if "Show Colors" is enabled in the GUI, copy the non-MSAA colors to the back buffer
        if (pBinFile)
        {
            g_pSceneRenderer.CopyColors(pBinFile->GetColorSRV());
        }
        else
        {
            // g_pSceneRenderer.CopyColors(g_NonMSAAColorSRV);
        }
    }
	bool g_ShowAO = false;
    if (!g_ShowColors && !g_ShowAO)
    {
        // If both "Show Colors" and "Show AO" are disabled in the GUI
        float BgColor[4] = { 0.0f, 0.0f, 0.0f };
        // md3dImmediateContext->ClearRenderTargetView(pBackBufferRTV, BgColor);
    }

	/*
    if (g_ShowAO)
    {
        NVSDK_Status status;
        if (pMesh)
        {
            status = NVSDK_D3D11_SetHardwareDepthsForAO(pd3dDevice, g_DepthStencilTexture, ZNEAR, ZFAR, g_Viewport.MinDepth, g_Viewport.MaxDepth, pMesh->GetSceneScale());
            assert(status == NVSDK_OK);
        }
        else
        {
            status = NVSDK_D3D11_SetViewSpaceDepthsForAO(pd3dDevice, pBinFile->GetLinearDepthTexture(), pBinFile->GetSceneScale());
            assert(status == NVSDK_OK);
        }

        g_AOParams.useBlending = g_ShowColors;
        status = NVSDK_D3D11_SetAOParameters(&g_AOParams);
        assert(status == NVSDK_OK);

        status = NVSDK_D3D11_RenderAO(pd3dDevice, pd3dImmediateContext, FOVY, pBackBufferRTV, NULL, &RenderTimes);
        assert(status == NVSDK_OK);

        status = NVSDK_D3D11_GetAllocatedVideoMemoryBytes(&NumBytes);
        assert(status == NVSDK_OK);
    }

    //--------------------------------------------------------------------------------------
    // Render the GUI
    //--------------------------------------------------------------------------------------
    if (g_DrawUI)
    {
        RenderText(RenderTimes, NumBytes);
        g_HUD.OnRender(fElapsedTime); 
    }
	*/
}

void SSAOApp::DrawScene()
{
	md3dImmediateContext->RSSetViewports(1, &mScreenViewport);

	if (mState == 1)
	{
		ApplyPipelineState("NormalDepth");
		md3dImmediateContext->RSSetState(mRasterizerState["nowireframe"].Get());
		FrameRender(0, 0, nullptr);

		ApplyPipelineState("ComputeSSAO");
		ComputeSSAO();

		ApplyPipelineState("BlurSSAO");
		BlurAmbientMap(1);

		ApplyPipelineState("Composite");
		DrawSceneWithSSAO();
	}
	else
	{
		m_RenderTargetBuffer->Clear();
		mDepths["depthBuffer"]->Clear();
		ID3D11RenderTargetView* ColorRTV = { m_RenderTargetBuffer->GetRTV() };
		md3dImmediateContext->RSSetState(mRasterizerState["wireframe"].Get());
		// Render color and depth with the Scene3D class
		md3dImmediateContext->OMSetRenderTargets(1, &ColorRTV, mDepths["depthBuffer"]->GetDSV());
		FrameRender(0, 0, nullptr);
	}
	HR(mSwapChain->Present(0, 0));
}

void SSAOApp::BlurAmbientMap(UINT nCount)
{
	std::vector<ID3D11SamplerState*> Sampler = { GetSampler("border") };
	md3dImmediateContext->PSSetSamplers(0, Sampler.size(), Sampler.data());

	Blur1D(true);
	Blur1D(false);
}

void SSAOApp::Blur1D(bool bHorzBlur)
{
	__declspec(align(16)) struct BlurConstants
	{
		float gTexelWidth;
		float gTexelHeight;
	};
	static ConstantBuffer<BlurConstants> buffer(md3dDevice, 1);
	BlurConstants blurCB;
	blurCB.gTexelHeight = 1.0f / (mClientHeight*1.0);
	blurCB.gTexelWidth = 1.0f / (mClientWidth*1.0);
	buffer.UploadData(md3dImmediateContext, 0, blurCB);

	std::vector<ID3D11Buffer*> Buffer = { buffer.Resource(0) };
	md3dImmediateContext->VSSetConstantBuffers(0, Buffer.size(), Buffer.data());
	md3dImmediateContext->PSSetConstantBuffers(0, Buffer.size(), Buffer.data());

	std::string srv = "AmbientBuffer0";
	std::string rtv = "AmbientBuffer1";

	if (!bHorzBlur)
		std::swap(rtv, srv);

	ShaderCheckResource(PixelShader, D3D_SIT_TEXTURE, 0, "gInputImage");

	std::vector<ID3D11ShaderResourceView*> SRV = { nullptr, nullptr };
	std::vector<ID3D11RenderTargetView*> RTV = { nullptr };
	md3dImmediateContext->PSSetShaderResources(0, SRV.size(), SRV.data());
	md3dImmediateContext->OMSetRenderTargets(RTV.size(), RTV.data(), nullptr);

	SRV = { mColors[srv]->GetSRV() };
	RTV = { mColors[rtv]->GetRTV() };

	// BindShaderResource(0, mColors[srv]->GetSRV());
	md3dImmediateContext->PSSetShaderResources(0, SRV.size(), SRV.data());
	md3dImmediateContext->OMSetRenderTargets(RTV.size(), RTV.data(), nullptr);
	mColors[rtv]->Clear();

	UINT stride = sizeof(VertexTex);
    UINT offset = 0;
	std::vector<ID3D11Buffer*> VB = { m_ScreenVertex.Get() };
    md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	md3dImmediateContext->IASetVertexBuffers(0, 1, VB.data(), &stride, &offset);
	md3dImmediateContext->IASetIndexBuffer(m_ScreenIndex.Get(), DXGI_FORMAT_R16_UINT, 0);
	md3dImmediateContext->DrawInstanced(6, 1, 0, 0);
}

void SSAOApp::ShaderCheckResource(ShaderType Type, D3D_SHADER_INPUT_TYPE InputType, UINT Slot, std::string Name)
{
	assert(m_pCurrentPSO->m_Shaders[Type]);
	assert(m_pCurrentPSO->m_Shaders[Type]->ShaderCheckResource(Type, InputType, Slot, Name));
}

void SSAOApp::DrawSceneWithSSAO()
{
	m_RenderTargetBuffer->Clear();

	std::vector<ID3D11SamplerState*> Sampler = { GetSampler("anisotropicWrap") };
	md3dImmediateContext->PSSetSamplers(0, Sampler.size(), Sampler.data());

	std::vector<ID3D11ShaderResourceView*> SRV = { mColors["AmbientBuffer0"]->GetSRV() };
	ShaderCheckResource(PixelShader, D3D_SIT_TEXTURE, 0, "tAO");

	md3dImmediateContext->PSSetShaderResources(0, SRV.size(), SRV.data());
    md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	md3dImmediateContext->DrawInstanced(6, 1, 0, 0);
}

float lerp(float a, float b, float f)
{
    return a + f * (b - a);
}  

const int kernelSize = 64;
std::vector<XMFLOAT4> CalcOffsetVectors() 
{
	std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between 0.0 - 1.0
	std::default_random_engine generator;
	std::vector<XMFLOAT4> ssaoKernel;
	for (uint16_t i = 0; i < kernelSize; ++i)
	{
		XMFLOAT3 sample(
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator)
		);

		XMVECTOR T = XMLoadFloat3(&sample);
		T = XMVector3Normalize(T);
		XMVectorScale(T, randomFloats(generator));

		float scale = float(i) / float(kernelSize);
		scale = lerp(0.1f, 1.0f, scale * scale);
		XMVectorScale(T, scale);
		XMFLOAT4 target;
		target.w = 0.f;
		XMStoreFloat4(&target, T);
		ssaoKernel.push_back(target);
	}
	return ssaoKernel;
}

void SetOffsetVectors(XMFLOAT4 target[kernelSize])
{
	static auto offset = CalcOffsetVectors();
    std::copy(offset.begin(), offset.end(), 
		stdext::checked_array_iterator<XMFLOAT4*>(target, kernelSize));
}

void SSAOApp::ComputeSSAO()
{
    // Transform NDC space [-1,+1]^2 to texture space [0,1]^2
    XMMATRIX T(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);


	__declspec(align(16)) struct SSAOConstants
	{
		XMFLOAT4X4 gProj;
		XMFLOAT4X4 gInvProj;
		XMFLOAT4X4 gProjTex;
		XMFLOAT4   gOffsetVectors[kernelSize];

		// Coordinates given in view space.
		float    gOcclusionRadius = 0.5f;
		float    gOcclusionFadeStart = 0.2f;
		float    gOcclusionFadeEnd = 2.0f;
		float    gSurfaceEpsilon = 0.05f;
	};
	static ConstantBuffer<SSAOConstants> buffer(md3dDevice, 1);
	SSAOConstants ssaoCB;
	ssaoCB.gOcclusionRadius = gRaidus;
    XMStoreFloat4x4(&ssaoCB.gProj, mProj);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(mProj), mProj);
    XMStoreFloat4x4(&ssaoCB.gInvProj, invProj);
    XMStoreFloat4x4(&ssaoCB.gProjTex, mProj * T);
	SetOffsetVectors(ssaoCB.gOffsetVectors);
	buffer.UploadData(md3dImmediateContext, 0, ssaoCB);

	std::vector<ID3D11Buffer*> Buffer = { buffer.Resource(0) };
	md3dImmediateContext->VSSetConstantBuffers(0, Buffer.size(), Buffer.data());
	md3dImmediateContext->PSSetConstantBuffers(0, Buffer.size(), Buffer.data());
	std::vector<ID3D11SamplerState*> Sampler = {
		GetSampler("normal"),
		GetSampler("depth"),
		GetSampler("randomVec") 
	};
	md3dImmediateContext->PSSetSamplers(0, Sampler.size(), Sampler.data());
	std::vector<ID3D11ShaderResourceView*> SRV = { 
		mColors["normal"]->GetSRV(),
		mDepths["depthBuffer"]->GetSRV(),
		mColors["randomVec"]->GetSRV() 
	};
	md3dImmediateContext->PSSetShaderResources(0, SRV.size(), SRV.data());

	md3dImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    md3dImmediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    md3dImmediateContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	md3dImmediateContext->DrawInstanced(6, 1, 0, 0);
}

void SSAOApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void SSAOApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void SSAOApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if( (btnState & MK_LBUTTON) != 0 )
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

		mCam.Pitch(dy*0.1);
		mCam.RotateY(dx*0.1);
	}
	else if( (btnState & MK_RBUTTON) != 0 )
	{
		// Make each pixel correspond to 0.01 unit in the scene.
		float dx = 0.01f*static_cast<float>(x - mLastMousePos.x);
		float dy = 0.01f*static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		// mRadius += dx - dy;

		// Restrict the radius.
		// mRadius = MathHelper::Clamp(mRadius, 1.0f, 20000.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void SSAOApp::BuildStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	auto pointWrap = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	pointWrap.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	pointWrap.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	pointWrap.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	pointWrap.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	CreateSampler("pointWrap", pointWrap);

	auto pointClamp = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	pointClamp.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	pointClamp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	pointClamp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	pointClamp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	CreateSampler("pointClamp", pointClamp);

	auto linearWrap = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	linearWrap.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	linearWrap.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	linearWrap.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	linearWrap.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	CreateSampler("linearWrap", linearWrap);

	auto linearClamp = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	linearClamp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	linearClamp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	linearClamp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	linearClamp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	CreateSampler("linearClamp", linearClamp);

	auto anisotropicWrap = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	anisotropicWrap.Filter = D3D11_FILTER_ANISOTROPIC;
	anisotropicWrap.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	anisotropicWrap.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	anisotropicWrap.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	anisotropicWrap.MipLODBias = 0.0f;
	anisotropicWrap.MaxAnisotropy = 8;
	anisotropicWrap.BorderColor[1] = 0.0f;
	anisotropicWrap.BorderColor[2] = 0.0f;
	CreateSampler("anisotropicWrap", anisotropicWrap);

	auto anisotropicClamp = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	anisotropicClamp.Filter = D3D11_FILTER_ANISOTROPIC;
	anisotropicClamp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	anisotropicClamp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	anisotropicClamp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	anisotropicClamp.MipLODBias = 0.0f;
	anisotropicClamp.MaxAnisotropy = 8;
	CreateSampler("anisotropicClamp", anisotropicClamp);
}

void SSAOApp::CreateBlendState(const std::string& tag, const D3D11_BLEND_DESC & desc)
{
	ComPtr<ID3D11BlendState> BS;
	HR(md3dDevice->CreateBlendState(&desc, BS.GetAddressOf()));
	mBlendState[tag] = BS;
}

void SSAOApp::CreateDepthStencilState(const std::string& tag, const D3D11_DEPTH_STENCIL_DESC& desc)
{
	ComPtr<ID3D11DepthStencilState> DDS;
	HR(md3dDevice->CreateDepthStencilState(&desc, DDS.GetAddressOf()));
	mDepthStencilState[tag] = DDS;
}

void SSAOApp::CreateResterizerState(const std::string& tag, const D3D11_RASTERIZER_DESC& desc)
{
	ComPtr<ID3D11RasterizerState> RS;
	HR(md3dDevice->CreateRasterizerState(&desc, RS.GetAddressOf()));
	mRasterizerState[tag] = RS;
}

void SSAOApp::CreateSampler(std::string name, const CD3D11_SAMPLER_DESC& desc)
{
	ComPtr<ID3D11SamplerState> SS;
	HR(md3dDevice->CreateSamplerState(&desc, SS.GetAddressOf()));
	m_SamplerState[name] = SS;
}

RenderTargetPtr SSAOApp::CreateRenderTarget(std::string name)
{
    auto renderTarget = std::make_shared<RenderTarget>();
    mRenderTargets[name] = renderTarget;
    return renderTarget;
}

ColorBufferPtr SSAOApp::CreateColorBuffer(std::string Name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format, D3D11_SUBRESOURCE_DATA* Data)
{
	auto Buffer = std::make_shared<ColorBuffer>();
	Buffer->Create(Name, Width, Height, NumMips, Format, Data);
	mColors[Name] = Buffer;
	return Buffer;
}

DepthBufferPtr SSAOApp::CreateDepthBuffer(std::string name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format)
{
	auto Buffer = std::make_shared<DepthBuffer>(name);
	Buffer->Create(Width, Height, Format);
	mDepths[name] = Buffer;
	return Buffer;
}

void SSAOApp::CreatePSO(const std::string& tag, const PipelineStateDesc& desc)
{
	auto pPSO = std::make_unique<PipelineStateObject>();
	if (mInputLayout.count(desc.IL)) {
		pPSO->pIL = mInputLayout[desc.IL].Get();
		assert(pPSO->pIL != nullptr);
	}

	for (int i = 0; i < NumShader; i++)
	{
		auto Type = static_cast<ShaderType>(i);
		if (desc.m_ShaderName[Type].empty()) 
			continue;
		pPSO->m_Shaders[Type] = m_Shaders[Type][desc.m_ShaderName[Type]];
		assert(Type == pPSO->m_Shaders[Type]->m_ShaderType);
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

	if (!desc.RT.empty()) {
		assert(mRenderTargets.count(desc.RT));
		pPSO->pRenderTarget = mRenderTargets[desc.RT].get();
	}

	for (int i = 0; i < NumShader; i++) {
		// Sampler state
	}

	mPSOs[tag].swap(pPSO);
}

void SSAOApp::BuildViewDependentResource()
{
	auto HalfWidth = static_cast<uint32_t>(mClientWidth * 1.0);
	auto HalfHegiht = static_cast<uint32_t>(mClientHeight* 1.0);

	auto AmbientBuffer0 = CreateColorBuffer("AmbientBuffer0", HalfWidth, HalfHegiht, 1, DXGI_FORMAT_R16_FLOAT);
	AmbientBuffer0->SetColor({0.0f, 0.0f, 0.0f, 1.0f});

	auto AmbientBuffer1 = CreateColorBuffer("AmbientBuffer1", HalfWidth, HalfHegiht, 1, DXGI_FORMAT_R16_FLOAT);
	AmbientBuffer1->SetColor({0.0f, 0.0f, 0.0f, 1.0f});
}

void SSAOApp::BuildPSO()
{
	CD3D11_RASTERIZER_DESC rs(D3D11_DEFAULT);
	rs.FillMode = D3D11_FILL_WIREFRAME;
	rs.CullMode = D3D11_CULL_NONE;
	CreateResterizerState("wireframe", rs);

	CD3D11_RASTERIZER_DESC nors(D3D11_DEFAULT);
	nors.CullMode = D3D11_CULL_NONE;
	CreateResterizerState("nowireframe", nors);

	auto normal = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	normal.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	normal.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	normal.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	normal.BorderColor[0] = 0.f;
	normal.BorderColor[1] = 0.f;
	normal.BorderColor[2] = 0.f;
	CreateSampler("normal", normal);

	auto depth = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	depth.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	depth.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	depth.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	depth.BorderColor[0] = 1.f;
	CreateSampler("depth", depth);

	auto randomVec = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	randomVec.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	randomVec.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	randomVec.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	CreateSampler("randomVec", randomVec);

	auto border = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
	border.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	border.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	border.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	CreateSampler("border", border);

	PipelineStateDesc NormalDepthState { "flat", "", "" };
	auto Normal = CreateColorBuffer("normal", mClientWidth, mClientHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
#ifdef NON_ZERO_CLEAR 
	Normal->SetColor(Color(0.0f, 0.0f, -1.0f, 1e5f));
#endif
	auto DepthBuffer = CreateDepthBuffer("depthBuffer", mClientWidth, mClientHeight, DXGI_FORMAT_R32_TYPELESS);
	auto NormalDepthRenderTarget = CreateRenderTarget("normalDepth");
	NormalDepthRenderTarget->SetColor(Slot::Color0, Normal);
	NormalDepthRenderTarget->SetDepth(DepthBuffer);
	NormalDepthState.RT = "normalDepth";
	CreatePSO("NormalDepth", NormalDepthState);

	std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between 0.0 - 1.0
	std::default_random_engine generator;
	std::vector<XMFLOAT3> ssaoNoise;
	const int noiseSize = 16;
	for (uint16_t i = 0; i < noiseSize; i++)
	{
		XMFLOAT3 noise(
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			0.0f);
		ssaoNoise.push_back(noise);
	}

    XMFLOAT4 initData[noiseSize];
    for (int i = 0; i < noiseSize; ++i)
    {
		auto& v = ssaoNoise[i];
		initData[i] = XMFLOAT4(v.x, v.y, v.z, 0.0f);
    }

    D3D11_SUBRESOURCE_DATA subResourceData = {0};
    subResourceData.pSysMem = initData;
    subResourceData.SysMemPitch = noiseSize * sizeof(XMFLOAT4);
	CreateColorBuffer("randomVec", 4, 4, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, &subResourceData);

	PipelineStateDesc ComputeSSAOState { "", "ssao", "ssao" };
	auto ComputeSSAOTarget = CreateRenderTarget("ssao");
	ComputeSSAOTarget->SetColor(Slot::Color0, mColors["AmbientBuffer0"]);
	ComputeSSAOState.RT = "ssao";
	ComputeSSAOState.BindSampler(VertexShader | PixelShader, { "normal", "depth", "randomeVec" });
	CreatePSO("ComputeSSAO", ComputeSSAOState);

	auto DrawRenderTarget = CreateRenderTarget("draw");
	DrawRenderTarget->SetColor(Slot::Color0, m_RenderTargetBuffer);
	DrawRenderTarget->SetDepth(DepthBuffer);
	DrawRenderTarget->m_bClearColor = false;
	DrawRenderTarget->m_bClearDepth = false;
	DrawRenderTarget->m_bClearStencil = false;

	PipelineStateDesc BlurState { "", "blur", "blur" };
	CreatePSO("BlurSSAO", BlurState);

	PipelineStateDesc CompositeState { "", "composite", "composite" };
	CompositeState.RT = "draw";
	CreatePSO("Composite", CompositeState);

}
