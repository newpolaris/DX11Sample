Texture2D gDiffuseMap : register(t0);
RWTexture2D<float4> gOutput;

SamplerState gsamPointClamp       : register(s0);

// groupshared float4 gCache[10];
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
	float gWeights[11] = { 0.05f, 0.05f, 0.1f, 0.1f, 0.1f, 0.2f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f, };
	static const int gBlurRadius = 5;

	float4 blurColor = float4(0, 0, 0, 0);
	for (int i = - gBlurRadius; i <= gBlurRadius; ++i) {
		float k = float(i)/800 + pin.TexC.x;
		float2 texPos = float2(k, pin.TexC.y);
		blurColor += gWeights[i+gBlurRadius] * gDiffuseMap.Sample(gsamPointClamp, texPos);
	}
	return blurColor;
}

float4 VertBlurPS(VertexOut pin) : SV_Target
{
	float gWeights[11] = { 0.05f, 0.05f, 0.1f, 0.1f, 0.1f, 0.2f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f, };
	static const int gBlurRadius = 5;

	float4 blurColor = float4(0, 0, 0, 0);
	for (int i = - gBlurRadius; i <= gBlurRadius; ++i) {
		float k = float(i)/600 + pin.TexC.y;
		float2 texPos = float2(pin.TexC.x, k);
		blurColor += gWeights[i+gBlurRadius] * gDiffuseMap.Sample(gsamPointClamp, texPos);
	}
	return blurColor;
}