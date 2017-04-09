//=============================================================================
// Basic.fx by Frank Luna (C) 2011 All Rights Reserved.
//
// Basic effect that currently supports transformations, lighting, and texturing.
//=============================================================================

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
Texture2D gDiffuseMap : register(t0);

SamplerState samAnisotropic : register(s0);

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float3 Tangent : TANGENT;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{	
    float3 PosL    : POSITION;
    float3 NormalW : NORMAL;
};

struct GeoOut
{	
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
	vout.PosL = vin.PosL;
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorldInvTranspose);
	
	return vout;
}

void Subdivide(VertexOut gin[3], out VertexOut outVerts[6])
{
	//       1
	//       *
	//      / \
	//     /   \
	//  m0*-----*m1
	//   / \   / \
	//  /   \ /   \
	// *-----*-----*
	// 0    m2     2

	VertexOut m[3];	
	m[0].PosL = 0.5*(gin[0].PosL+gin[1].PosL);
	m[1].PosL = 0.5*(gin[2].PosL+gin[1].PosL);
	m[2].PosL = 0.5*(gin[2].PosL+gin[0].PosL);
	m[0].PosL = normalize(m[0].PosL);
	m[1].PosL = normalize(m[1].PosL);
	m[2].PosL = normalize(m[2].PosL);
	m[0].NormalW = normalize(gin[0].NormalW+gin[1].NormalW);
	m[1].NormalW = normalize(gin[2].NormalW+gin[1].NormalW);
	m[2].NormalW = normalize(gin[2].NormalW+gin[0].NormalW);
	
	outVerts[0] = gin[0];
	outVerts[1] = m[0];
	outVerts[2] = m[2];
	outVerts[3] = m[1];
	outVerts[4] = gin[2];
	outVerts[5] = gin[1];
}

[maxvertexcount(32)]
void GS(triangle VertexOut gin[3], inout TriangleStream<GeoOut> triStream)
{
	float d = distance(gEyePosW, float3(0, 0, 0));
	if (d < 5) {
		VertexOut vout[6];
		Subdivide(gin, vout);
		GeoOut gout[6];
		[unroll]
		for (int i = 0; i < 6; ++i)
		{
			// Transform to world space space. 
			gout[i].PosW = mul(float4(vout[i].PosL, 1.0f), gWorld).xyz;
			gout[i].NormalW = vout[i].NormalW;
			gout[i].PosH = mul(float4(gout[i].PosW, 1.0f), gViewProj);
		}
		[unroll]
		for (int i = 0; i < 5; i++)
			triStream.Append(gout[i]);
		triStream.RestartStrip();
		triStream.Append(gout[1]);
		triStream.Append(gout[5]);
		triStream.Append(gout[3]);
	} else {
		GeoOut gout[3];
		[unroll]
		for (int i = 0; i < 3; ++i)
		{
			gout[i].PosW = mul(float4(gin[i].PosL, 1.0f), gWorld).xyz;
			gout[i].NormalW = gin[i].NormalW;
			gout[i].PosH = mul(float4(gout[i].PosW, 1.0f), gViewProj);
		}
		triStream.Append(gout[0]);
		triStream.Append(gout[1]);
		triStream.Append(gout[2]);
	}
}
 
float4 PS(GeoOut pin) : SV_Target
{
    float4 diffuseAlbedo = gDiffuseAlbedo;

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
    litColor.a = gDiffuseAlbedo.a;
    return litColor;
}