#include "Sky.h"
#include "GeometryGenerator.h"

D3D11_INPUT_ELEMENT_DESC SkyVertexLayout[] =
{
    { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

Sky::Sky(ID3D11Device * pDevice, const std::wstring & cubemap_path, float skySphereRadius) {
	m_CubeMapTexture = std::make_shared<TextureResource>(pDevice, "cubeMap");
	m_CubeMapTexture->LoadTextureFromFile(L"../../Textures/snowcube1024.dds");

	GeometryGenerator::MeshData sphere;
	GeometryGenerator geoGen;
	geoGen.CreateSphere(skySphereRadius, 30, 30, sphere);

	std::vector<XMFLOAT3> vertices(sphere.Vertices.size());

	for (size_t i = 0; i < sphere.Vertices.size(); ++i)
	{
		vertices[i] = sphere.Vertices[i].Position;
	}

	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(XMFLOAT3) * vertices.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA vinitData;
	vinitData.pSysMem = &vertices[0];

	HR(pDevice->CreateBuffer(&vbd, &vinitData, &mVB));

	mIndexCount = sphere.Indices.size();

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(USHORT) * mIndexCount;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.StructureByteStride = 0;
	ibd.MiscFlags = 0;

	std::vector<USHORT> indices16;
	indices16.assign(sphere.Indices.begin(), sphere.Indices.end());

	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &indices16[0];
	HR(pDevice->CreateBuffer(&ibd, &iinitData, &mIB));

    ID3DXBuffer* pShader;
    auto flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    HR(D3DXCompileShaderFromFile(L"FX/Sky.fx", NULL, NULL, NULL, "fx_5_0", flags, &pShader, NULL, NULL));
    HR((D3DX11CreateEffectFromMemory(pShader->GetBufferPointer(), pShader->GetBufferSize(), 0, pDevice, &m_Effect)));

    m_SkyTechPass = m_Effect->GetTechniqueByName("SkyTech")->GetPassByIndex(0);

	D3DX11_PASS_DESC passDesc;
	m_SkyTechPass->GetDesc(&passDesc);
	HR(pDevice->CreateInputLayout(SkyVertexLayout, 1, passDesc.pIAInputSignature, 
		passDesc.IAInputSignatureSize, &m_pInputLayout));

	m_VarWVP = m_Effect->GetVariableByName("gWorldViewProj")->AsMatrix();
	m_CubeMap = m_Effect->GetVariableByName("gCubeMap")->AsShaderResource();
}


Sky::~Sky()
{
    ReleaseCOM(m_Effect);
	ReleaseCOM(mVB);
	ReleaseCOM(mIB);
    ReleaseCOM(m_pInputLayout);
}

void Sky::Draw(ID3D11DeviceContext* dc, const Camera& camera)
{
	// center Sky about eye in world space
	XMFLOAT3 eyePos = camera.GetPosition();
	XMMATRIX T = XMMatrixTranslation(eyePos.x, eyePos.y, eyePos.z);
	XMMATRIX WVP = XMMatrixMultiply(T, camera.ViewProj());

	m_VarWVP->SetMatrix(reinterpret_cast<const float*>(&WVP));
	m_CubeMap->SetResource(m_CubeMapTexture->GetSRV());

	UINT stride = sizeof(XMFLOAT3);
    UINT offset = 0;
    dc->IASetVertexBuffers(0, 1, &mVB, &stride, &offset);
	dc->IASetIndexBuffer(mIB, DXGI_FORMAT_R16_UINT, 0);
	dc->IASetInputLayout(m_pInputLayout);
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
    for(UINT p = 0; p < 1; ++p)
    {
		m_SkyTechPass->Apply(0, dc);

		dc->DrawIndexed(mIndexCount, 0, 0);
	}
}

ID3D11ShaderResourceView * Sky::GetSRV()
{
	return m_CubeMapTexture->GetSRV();
}
