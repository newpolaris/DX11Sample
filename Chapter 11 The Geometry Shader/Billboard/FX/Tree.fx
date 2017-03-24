// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 0
#endif

#include "LightHelper.fx"

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
	float4x4 gTexTransform;
}; 

cbuffer cbMaterial : register(b1)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float gRoughness;
	float4x4 gMatTransform;
}

// Constant data that varies per material.
cbuffer cbPass : register(b2)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4 gAmbientLight;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

// Nonnumeric values cannot be added to a cbuffer.
Texture2DArray gTreeMapArray : register(t0);

SamplerState samAnisotropic : register(s0);

struct VertexIn
{
	float3 PosW    : POSITION;
	float2 SizeW   : SIZE;
};

struct VertexOut
{
	float3 PosW    : POSITION;
	float2 SizeW   : SIZE;
};

struct GeoOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
	uint   PrimID  : SV_PrimitiveID;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	vout.PosW = vin.PosW;
	vout.SizeW = vin.SizeW;

	return vout;
}

[maxvertexcount(4)]
void GS(point VertexOut gin[1],
	uint primID : SV_PrimitiveID,
	inout TriangleStream<GeoOut> triStream)
{
	float3 CenterW = gin[0].PosW;
	float2 Half = gin[0].SizeW / 2;
	float3 w = float3(gEyePosW.x - CenterW.x, 0, gEyePosW.z - CenterW.z);
	w = normalize(w);
	float3 v = {0, 1, 0};
	float3 u = cross(v, w);
	float4 board[4];
	board[0] = float4(CenterW + Half.x * u - Half.y * v, 1);
	board[1] = float4(CenterW + Half.x * u + Half.y * v, 1);
	board[2] = float4(CenterW - Half.x * u - Half.y * v, 1);
	board[3] = float4(CenterW - Half.x * u + Half.y * v, 1);

	float2 texC[4] = 
	{
		float2(0.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 1.0f),
		float2(1.0f, 0.0f)
	};

	[unroll]
	for (int i = 0; i < 4; i++)
	{
		GeoOut gout;
		gout.PosH    = mul(board[i], gViewProj);
		gout.PosW    = board[i].xyz;
		gout.NormalW = w;
		gout.TexC    = texC[i];
		gout.PrimID  = primID;
		triStream.Append(gout);
	}
}
 
float4 PS(GeoOut pin) : SV_Target
{
    float4 texColor = float4(1.0, 1.0, 1.0, 1.0);
	texColor = gTreeMapArray.Sample(samAnisotropic, float3(pin.TexC, pin.PrimID%3));
    float4 diffuseAlbedo = texColor * gDiffuseAlbedo;

	// Discard pixel if texture alpha < 0.1.  We do this test as soon 
	// as possible in the shader so that we can potentially exit the
	// shader early, thereby skipping the rest of the shader code.
	clip(texColor.a - 0.05f);

	// Interpolating normal can unnormalize it, so normalize it.
    pin.NormalW = normalize(pin.NormalW);

	// The toEye vector is used in lighting.
	float3 toEyeW = normalize(gEyePosW - pin.PosW);
	// Start with a sum of zero. 

	// Indirect lighting.
	float4 ambient = gAmbientLight*diffuseAlbedo;

	const float shininess = 1.0f - gRoughness;

	Material mat = { diffuseAlbedo, gFresnelR0, shininess };
	float3 shadowFactor = 1.0f;
	float4 directLight = ComputeLighting(gLights, mat, pin.PosW,
		pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // Common convention to take alpha from diffuse material.
    litColor.a = gDiffuseAlbedo.a * texColor.a;
    return litColor;
}