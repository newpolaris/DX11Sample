#define kernelSize 64
#define DEPTH32
// #define RANGE_METHOD0
#define SELF_OCC_REMOVE
// #define GATHERING

cbuffer cbSsao : register(b0)
{
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gProjTex;
	float4   gOffsetVectors[kernelSize];
    float2   gNoiseScale = {1280.f / 4, 720.f / 4};

	// Coordinates given in view space.
	float    gOcclusionRadius    = 0.5f;
	// g_scale: scales distance between occluders and occludee.
    // g_bias: controls the width of the occlusion cone considered by the occludee.
    // g_intensity: the ao intensity.
	float    gScale              = 1.f;
	float    gBias               = 0.1f;
	float    gIntencity          = 2.f;
};

Texture2D gNormalMap      : register(t0);
Texture2D gDepthMap       : register(t1);
Texture2D gRandomVecMap   : register(t2);

SamplerState gSamNormal    : register(s0);
SamplerState gSamDepth     : register(s1);
SamplerState gSamRandomVec : register(s2);

static const int gSampleCount = kernelSize;
static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};
 
struct VertexIn
{
	float3 PosH : POSITION;
	float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosV : POSITION;
	float2 TexC : TEXCOORD0;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;

	vout.PosH = float4(vin.PosH, 1.f);
    // Transform quad corners to view space near plane.
    float4 ph = mul(vout.PosH, gInvProj);
	// divide w to convert view space (after invProj z = 1, w = 1/z)
    vout.PosV = ph.xyz / ph.w;
	vout.TexC = vin.TexC;

    return vout;
}
float NdcDepthToViewDepth(float z_ndc)
{
    // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float gather_occulusion(float3 p, float3 n, float3 occ_p)
{
	float3 v = occ_p - p;
	float d = length(v);
	v /= d;
	d *= gScale;
	return max(0.f, dot(n, v) - gBias) * (1.f / (1 + d)) * gIntencity;
}

float sample_depth(float2 coord)
{
#ifdef DEPTH32
	float depth = gDepthMap.SampleLevel(gSamDepth, coord, 0.0f).x;
	return NdcDepthToViewDepth(depth);
#else
	return gNormalMap.SampleLevel(gSamDepth, coord, 0.0f).w;
#endif
}

float3 sample_position(float3 pos, float depth)
{
	return (depth/pos.z)*pos;	
}

float3 sample_normal(float2 coord)
{
	return normalize(gNormalMap.SampleLevel(gSamNormal, coord, 0.0f).xyz);
}

float3 sample_random(float2 coord)
{
	return gRandomVecMap.SampleLevel(gSamRandomVec, coord, 0.0f).rgb;
}

float other_occlusion(float src_depth, float occ_depth, float3 target_position, float3 src_position, float3 normal)
{
#ifdef RANGE_METHOD0
	float rangeCheck = smoothstep(0.0, 1.0, gOcclusionRadius / abs(src_depth - occ_depth));
#else
	float rangeCheck = abs(src_depth - occ_depth) < gOcclusionRadius ? 1.0 : 0.0;
#endif
	float dp = max(dot(normal, normalize(target_position - src_position)), 0.0f);
	return (occ_depth <= target_position.z ? 1.0 : 0.0) * rangeCheck * dp;
}

float4 PS(VertexOut pin) : SV_Target
{
	float3 normal = sample_normal(pin.TexC);
	float src_depth = sample_depth(pin.TexC);
	float3 src_position = sample_position(pin.PosV, src_depth);
	float3 random = sample_random(gNoiseScale*pin.TexC);

	float3 tangent = normalize(random - normal * dot(random, normal));
	float3 bitangent = cross(normal, tangent);
	float3x3 tbn = float3x3(tangent, bitangent, normal);

	float occlusion = 0.0f;
	// Sample neighboring points about p in the hemisphere oriented by n.
	[unroll]
	for(int i = 0; i < gSampleCount; ++i)
	{
		float3 offset = mul(gOffsetVectors[i].xyz, tbn);
		float3 target_position = src_position + offset * gOcclusionRadius;
	
		// Project and generate projective tex-coords.  
		float4 occ_tex = mul(float4(target_position, 1.0f), gProjTex);
		float2 occ_coord = occ_tex.xy / occ_tex.w;

		// Find the nearest depth value along the ray from the eye to q (this is not
		// the depth of q, as q is just an arbitrary point near p and might
		// occupy empty space).  To find the nearest depth we look it up in the depthmap.
		float occ_depth = sample_depth(occ_coord);

#ifdef GATHERING
		[branch]
		if (occ_depth < src_depth) {
			float3 occ_point = sample_position(target_position, occ_depth);
			occlusion += gather_occulusion(src_position, normal, occ_point);
		}
#else
		occlusion += other_occlusion(src_depth, occ_depth, target_position, src_position, normal);
#endif
	}
	occlusion /= gSampleCount;
	
	float access = 1.0f - occlusion;

	// Sharpen the contrast of the SSAO map to make the SSAO affect more dramatic.
	return access;
}