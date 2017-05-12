#pragma once

#include "d3dUtil.h"
#include "Camera.h"
#include "TextureResource.h"

class Sky
{
public:
	Sky(ID3D11Device* pDevice, const std::wstring& cubemap_path, float skySphereRadius);

	~Sky();

	void Draw(ID3D11DeviceContext * dc, const Camera & camera);

private:
	ID3D11Buffer* mVB;
	ID3D11Buffer* mIB;

	UINT mIndexCount;

    // The effects and rendering techniques
    ID3DX11Effect               *m_Effect;
    ID3DX11EffectPass           *m_SkyTechPass;
	ID3D11InputLayout			*m_pInputLayout;

    // Effect variable pointers
    ID3DX11EffectMatrixVariable *m_VarWVP;
	ID3DX11EffectShaderResourceVariable* m_CubeMap;
	TextureResourcePtr m_CubeMapTexture;
};