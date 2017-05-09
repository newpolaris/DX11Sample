#define kernelSize  64
#define DEPTH32
#define RANGE_CHECK

cbuffer cbSsao : register(b0)
{
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gProjTex;
	float4   gOffsetVectors[kernelSize];

	// Coordinates given in view space.
	float    gOcclusionRadius    = 0.5f;
	float    gOcclusionFadeStart = 0.2f;
	float    gOcclusionFadeEnd   = 2.0f;
	float    gSurfaceEpsilon     = 0.05f;
};

Texture2D gNormalMap      : register(t0);
Texture2D gDepthMap       : register(t1);
Texture2D gRandomVecMap   : register(t2);

SamplerState gSamNormal    : register(s0);
SamplerState gSamDepth     : register(s1);
SamplerState gSamRandomVec : register(s2);

static const float2 gNoiseScale = {1280.f / 4, 720.f / 4};
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
 
struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosV : POSITION;
	float2 TexC : TEXCOORD0;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;

    vout.TexC = gTexCoords[vid];

    // Quad covering screen in NDC space ( near plain)
    vout.PosH = float4(2.0f*vout.TexC.x - 1.0f, 1.0f - 2.0f*vout.TexC.y, 0.0f, 1.0f);
 
    // Transform quad corners to view space near plane.
    float4 ph = mul(vout.PosH, gInvProj);
	// divide w to convert view space (after invProj z = 1, w = 1/z)
    vout.PosV = ph.xyz / ph.w;

    return vout;
}

float NdcDepthToViewDepth(float z_ndc)
{
    // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}

float4 PS(VertexOut pin) : SV_Target
{
	// p -- the point we are computing the ambient occlusion for.
	// n -- normal vector at p.
	// q -- a random offset from p.
	// r -- a potential occluder that might occlude p.

	// Get viewspace normal and z-coord of this pixel.  The tex-coords for
	// the fullscreen quad we drew are already in uv-space.
	float3 normal = normalize(gNormalMap.SampleLevel(gSamNormal, pin.TexC, 0.0f).xyz);
#ifdef DEPTH32
	float pz = gDepthMap.SampleLevel(gSamDepth, pin.TexC, 0.0f).x;
	pz = NdcDepthToViewDepth(pz);
#else
	float pz = gNormalMap.SampleLevel(gSamDepth, pin.TexC, 0.0f).w;
#endif

	//
	// Reconstruct full view space position (x,y,z).
	// Find t such that p = t*pin.ToFarPlane.
	// p.z = t*pin.ToFarPlane.z
	// t = p.z / pin.ToFarPlane.z
	//
	float3 p = (pz/pin.PosV.z)*pin.PosV;	

	float3 randomVec = gRandomVecMap.SampleLevel(gSamRandomVec, gNoiseScale*pin.TexC, 0.0f).rgb;

	float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
	float3 bitangent = cross(normal, tangent);
	float3x3 tbn = float3x3(tangent, bitangent, normal);

	float occlusion = 0.0f;
	// Sample neighboring points about p in the hemisphere oriented by n.
	[unroll]
	for(int i = 0; i < gSampleCount; ++i)
	{
		float3 Sample = mul(gOffsetVectors[i].xyz, tbn);
		Sample = p + Sample * gOcclusionRadius;
	
		// Project q and generate projective tex-coords.  
#ifndef PROJ_TEX
		float4 offset = float4(Sample, 1.0f);
		offset = mul(offset, gProj);
		offset.xy /= offset.w;
		offset.xy = offset.xy * 0.5 + 0.5;
#else
		float4 offset = mul(float4(Sample, 1.0f), gProjTex);
		offset /= offset.w;
#endif

		// Find the nearest depth value along the ray from the eye to q (this is not
		// the depth of q, as q is just an arbitrary point near p and might
		// occupy empty space).  To find the nearest depth we look it up in the depthmap.

#ifdef DEPTH32
		float sampleDepth = gDepthMap.SampleLevel(gSamDepth, offset.xy, 0.0f).r;
        sampleDepth = NdcDepthToViewDepth(sampleDepth);
#else
		float sampleDepth = gNormalMap.SampleLevel(gSamDepth, offset.xy, 0.0f).w;
#endif

#ifdef RANGE_CHECK
		float rangeCheck = smoothstep(0.0, 1.0, gOcclusionRadius / abs(pz - sampleDepth));
		// float rangeCheck = abs(pz - sampleDepth) < gOcclusionRadius ? 1.0 : 0.0;
#else
		float rangeCheck = 1.0f;
#endif
		occlusion += (sampleDepth <= Sample.z ? 1.0 : 0.0) * rangeCheck;
	}
	occlusion /= gSampleCount;
	
	float access = 1.0f - occlusion;

	// Sharpen the contrast of the SSAO map to make the SSAO affect more dramatic.
	return access;
}