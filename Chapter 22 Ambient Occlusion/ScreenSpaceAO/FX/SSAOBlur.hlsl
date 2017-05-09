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

VertexOut VS( uint id : SV_VertexID )
{
    VertexOut output = (VertexOut)0.0f;
    output.Tex = float2( (id << 1) & 2, id & 2 );
    output.PosH = float4( output.Tex * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f) , 0.0f, 1.0f );
    return output;
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