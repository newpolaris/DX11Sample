Texture2D gDiffuseMap : register(t0);
SamplerState samAnisotropic : register(s0);

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosH = float4(vin.PosL, 1.0f);
	vout.TexC = vin.TexC;
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	return gDiffuseMap.Sample(samAnisotropic, pin.TexC);
}