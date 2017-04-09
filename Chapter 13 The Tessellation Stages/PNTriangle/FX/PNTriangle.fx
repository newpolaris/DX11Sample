//=============================================================================
//=============================================================================

#define PAPER

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
    float gTessFactor;
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
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
	// DS에서의 추가 옵션 제공을 위해 Worold 까지만 바꾼다
	vout.PosW = mul(vin.PosL, (float3x3)gWorld);
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorldInvTranspose);
	
	return vout;
}

struct ConstantOut
{
	float EdgeTess[3]   : SV_TessFactor;
	float InsideTess    : SV_InsideTessFactor;

	// WORLD SPACE
    float3 b210    : POSITION3;
    float3 b120    : POSITION4;
    float3 b021    : POSITION5;
    float3 b012    : POSITION6;
    float3 b102    : POSITION7;
    float3 b201    : POSITION8;
    float3 b111    : CENTER;
    
    // Normal quadratic generated control points
    float3 n110    : NORMAL3;      
    float3 n011    : NORMAL4;
    float3 n101    : NORMAL5;
};

// Directional derivative for n is 2
float3 derivative(float3 direction, float3 bycentricPoint, float3x3 B[3])
{
	float3 ret = bycentricPoint[0] * mul(direction, B[0])
		       + bycentricPoint[1] * mul(direction, B[1])
		       + bycentricPoint[2] * mul(direction, B[2]);
	return 2 * ret;
}

ConstantOut ConstantHS(InputPatch<VertexOut, 3> patch)
{
	ConstantOut cout;
	cout.EdgeTess[0] = cout.EdgeTess[1] = cout.EdgeTess[2] = gTessFactor;

	// Now setup the PNTriangle control points...

	// Assign Positions
	float3 f3B003 = patch[0].PosW;
	float3 f3B030 = patch[1].PosW;
	float3 f3B300 = patch[2].PosW;
	// And Normals
	float3 f3N002 = patch[0].NormalW;
	float3 f3N020 = patch[1].NormalW;
	float3 f3N200 = patch[2].NormalW;

	// Compute the cubic geometry control points
	// Edge control points
	cout.b210 = ((2.0f * f3B003) + f3B030 - (dot((f3B030 - f3B003), f3N002) * f3N002)) / 3.0f;
	cout.b120 = ((2.0f * f3B030) + f3B003 - (dot((f3B003 - f3B030), f3N020) * f3N020)) / 3.0f;
	cout.b021 = ((2.0f * f3B030) + f3B300 - (dot((f3B300 - f3B030), f3N020) * f3N020)) / 3.0f;
	cout.b012 = ((2.0f * f3B300) + f3B030 - (dot((f3B030 - f3B300), f3N200) * f3N200)) / 3.0f;
	cout.b102 = ((2.0f * f3B300) + f3B003 - (dot((f3B003 - f3B300), f3N200) * f3N200)) / 3.0f;
	cout.b201 = ((2.0f * f3B003) + f3B300 - (dot((f3B300 - f3B003), f3N002) * f3N002)) / 3.0f;
	// Center control point
	float3 f3E = (cout.b210 + cout.b120 + cout.b021 + cout.b012 + cout.b102 + cout.b201) / 6.0f;
	float3 f3V = (f3B003 + f3B030 + f3B300) / 3.0f;
	cout.b111 = f3E + ((f3E - f3V) / 2.0f);

	// Compute the quadratic normal control points, and rotate into world space
	float fV12 = 2.0f * dot(f3B030 - f3B003, f3N002 + f3N020) / dot(f3B030 - f3B003, f3B030 - f3B003);
	cout.n110 = normalize(f3N002 + f3N020 - fV12 * (f3B030 - f3B003));
	float fV23 = 2.0f * dot(f3B300 - f3B030, f3N020 + f3N200) / dot(f3B300 - f3B030, f3B300 - f3B030);
	cout.n011 = normalize(f3N020 + f3N200 - fV23 * (f3B300 - f3B030));
	float fV31 = 2.0f * dot(f3B003 - f3B300, f3N200 + f3N002) / dot(f3B003 - f3B300, f3B003 - f3B300);
	cout.n101 = normalize(f3N200 + f3N002 - fV31 * (f3B003 - f3B300));

	cout.InsideTess = (cout.EdgeTess[0] + cout.EdgeTess[1] + cout.EdgeTess[2]) / 3.0f;

	return cout;
}

struct HullOut
{
	float3 PosW : POSITION;
	float3 NormalW : NORMAL;
};

[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("ConstantHS")]
[outputcontrolpoints(3)]
[maxtessfactor(9.f)]
HullOut HS(InputPatch<VertexOut, 3> hin, uint patchID: SV_OutputControlPointID)
{
	HullOut hout;
	hout.PosW = hin[patchID].PosW;
	hout.NormalW = hin[patchID].NormalW;
	return hout;
}

struct DomainOut
{	
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
};

// Description: http://reedbeta.com/blog/tess-quick-ref/
[domain("tri")]
DomainOut DS(in ConstantOut cout,
			 in float3 f3BarycentricCoords : SV_DomainLocation,
			 in const OutputPatch<HullOut, 3> hout)
{
    DomainOut dout;

    // The barycentric coordinates
    float fU = f3BarycentricCoords.x;
    float fV = f3BarycentricCoords.y;
    float fW = f3BarycentricCoords.z;

    // Precompute squares and squares * 3 
    float fUU = fU * fU;
    float fVV = fV * fV;
    float fWW = fW * fW;
    float fUU3 = fUU * 3.0f;
    float fVV3 = fVV * 3.0f;
    float fWW3 = fWW * 3.0f;
    
    // Compute position from cubic control points and barycentric coords
    float3 f3Position = hout[0].PosW * fWW * fW +
                        hout[1].PosW * fUU * fU +
                        hout[2].PosW * fVV * fV +
                        cout.b210 * fWW3 * fU +
                        cout.b120 * fW * fUU3 +
                        cout.b201 * fWW3 * fV +
                        cout.b021 * fUU3 * fV +
                        cout.b102 * fW * fVV3 +
                        cout.b012 * fU * fVV3 +
                        cout.b111 * 6.0f * fW * fU * fV;
    
    // Compute normal from quadratic control points and barycentric coords
    float3 f3Normal =   hout[0].NormalW * fWW +
                        hout[1].NormalW * fUU +
                        hout[2].NormalW * fVV +
#if PAPER
                        cout.n110 * fW * fU +
                        cout.n011 * fU * fV +
                        cout.n101 * fW * fV;
#else // correct equation
                        cout.n110 * fW * fU * 2 +
                        cout.n011 * fU * fV * 2 +
                        cout.n101 * fW * fV * 2;
#endif

    // Normalize the interpolated normal    
    dout.NormalW = normalize( f3Normal );

    // Linearly interpolate the texture coords
    // O.f2TexCoord = I[0].f2TexCoord * fW + I[1].f2TexCoord * fU + I[2].f2TexCoord * fV;

    // Transform model position with view-projection matrix
    dout.PosW = f3Position;
    dout.PosH = mul( float4( f3Position.xyz, 1.0 ), gViewProj );
        
    return dout;
}
 
float4 PS(DomainOut pin) : SV_Target
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

technique11 Tess
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS() ) );
        SetHullShader( CompileShader( hs_5_0, HS() ) );
        SetDomainShader( CompileShader( ds_5_0, DS() ) );
        SetPixelShader( CompileShader( ps_5_0, PS() ) );
    }
}
