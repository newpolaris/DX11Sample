cbuffer cbPerFrame
{
	float gTexelWidth;
	float gTexelHeight;
};

Texture2D gInputImage : register(t0);
SamplerState gSamLinearClamp : register(s0);

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 Tex     : TEXCOORD;
};

struct VertexOut
{
    float4 PosH  : SV_POSITION;
	float2 Tex   : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	
	// Already in NDC space.
	vout.PosH = float4(vin.PosL, 1.0f);

	// Pass onto pixel shader.
	vout.Tex = vin.Tex;
	
    return vout;
}

static const int radius = 2;
float PS(VertexOut pin) : SV_Target
{
    float result = 0.0;
    for (int x = -radius; x < radius; ++x) 
    {
        for (int y = -radius; y < radius; ++y) 
        {
			float2 offset = float2(x, y) * float2(gTexelWidth, gTexelHeight);
			result += gInputImage.SampleLevel(gSamLinearClamp, pin.Tex + offset, 0.0).x;
        }
    }
	return result / float(16.f);
	// return gInputImage.SampleLevel(gSamLinearClamp, pin.Tex, 0.0).x;
}