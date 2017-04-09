Texture2D gDiffuseMap : register(t0);
RWTexture2D<float4> gOutput;

SamplerState gLinearClamp       : register(s0);

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

static const float offset[6] = { -4.5, -2.5, -0.333333313, 1.5, 3.3333333, 5.0 };
static const float weight[6] = { 0.1, 0.2, 0.3, 0.2, 0.15, 0.05 };
static const int gBlurRadius = 6;
static const int width = 800;
static const int height = 600;

float4 HorzBlurPS(VertexOut pin) : SV_Target
{
	float4 blurColor = float4(0, 0, 0, 0);
	for (int i = 0; i < gBlurRadius; i++) {
		blurColor += weight[i] * gDiffuseMap.Sample(gLinearClamp, pin.TexC + float2(offset[i]/width, 0.f));
	}
	return blurColor;
}

float4 VertBlurPS(VertexOut pin) : SV_Target
{
	float4 blurColor = float4(0, 0, 0, 0);
	for (int i = 0; i < gBlurRadius; i++) {
		blurColor += weight[i] * gDiffuseMap.Sample(gLinearClamp, pin.TexC + float2(0.f, offset[i]/height));
	}
	return blurColor;
}