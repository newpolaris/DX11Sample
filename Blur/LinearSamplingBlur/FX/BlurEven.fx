Texture2D gDiffuseMap : register(t0);
RWTexture2D<float4> gOutput;

SamplerState gLinearClamp : register(s0);

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 Tex     : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.TexC = vin.Tex;
	vout.PosH = float4(vin.PosL, 1.f);
	return vout;
}

float4 HorzBlurPS(VertexOut pin) : SV_Target
{
	static const float offset[3] = { 0.0, 1.384615, 3.2307692308 };
	static const float weight[3] = { 0.227027, 0.3162162162, 0.0702702703 };
	static const int gBlurRadius = 3;
	static const int width = 800;

	float4 blurColor = float4(0, 0, 0, 0);
	blurColor = gDiffuseMap.Sample(gLinearClamp, pin.TexC) * weight[0];
	for (int i = 1; i < gBlurRadius; i++) {
		blurColor += weight[i] * gDiffuseMap.Sample(gLinearClamp, pin.TexC + float2(offset[i]/width, 0.f));
		blurColor += weight[i] * gDiffuseMap.Sample(gLinearClamp, pin.TexC - float2(offset[i]/width, 0.f));
	}
	return blurColor;
}

float4 VertBlurPS(VertexOut pin) : SV_Target
{
	static const float offset[3] = { 0.0, 1.384615, 3.2307692308 };
	static const float weight[3] = { 0.227027, 0.3162162162, 0.0702702703 };
	static const int gBlurRadius = 3;
	static const int height = 600;

	float4 blurColor = float4(0, 0, 0, 0);
	blurColor = gDiffuseMap.Sample(gLinearClamp, pin.TexC) * weight[0];
	for (int i = 1; i < gBlurRadius; i++) {
		blurColor += weight[i] * gDiffuseMap.Sample(gLinearClamp, pin.TexC + float2(0.f, offset[i]/height));
		blurColor += weight[i] * gDiffuseMap.Sample(gLinearClamp, pin.TexC - float2(0.f, offset[i]/height));
	}
	return blurColor;
}

