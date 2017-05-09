cbuffer cbPerFrame
{
	float gTexelWidth;
	float gTexelHeight;
    bool gHorizontalBlur;
};

Texture2D gInputImage : register(t0);

SamplerState gSamLinearClamp;

static const int gBlurRadius = 5;
static float gWeights[11] = { 0.05f, 0.05f, 0.1f, 0.1f, 0.1f, 0.2f, 0.1f, 0.1f, 0.1f, 0.05f, 0.05f };

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

float PS(VertexOut pin) : SV_Target
{
	float2 texOffset;
	if (gHorizontalBlur)
	{
		texOffset = float2(gTexelWidth, 0.0f);
	}
	else
	{
		texOffset = float2(0.0f, gTexelHeight);
	}

	float color = gWeights[5] * gInputImage.SampleLevel(gSamLinearClamp, pin.Tex, 0.0).x;
	float totalWeight = gWeights[5];
	 
	for(float i = -gBlurRadius; i <=gBlurRadius; ++i)
	{
		// We already added in the center weight.
		if( i == 0 )
			continue;

		float2 tex = pin.Tex + i*texOffset;

		//
		// If the center value and neighbor values differ too much (either in 
		// normal or depth), then we assume we are sampling across a discontinuity.
		// We discard such samples from the blur.
		//
	
			float weight = gWeights[i+gBlurRadius];

			// Add neighbor pixel to blur.
			color += weight*gInputImage.SampleLevel(gSamLinearClamp, tex, 0.0);
			totalWeight += weight;
	}

	// Compensate for discarded samples by making total weights sum to 1.
	return color / totalWeight;
}