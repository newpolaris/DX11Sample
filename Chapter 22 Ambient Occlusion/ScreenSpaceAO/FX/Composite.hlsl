Texture2D<float> tAO            : register(t0);
sampler PointSampler            : register(s0);

struct PostProc_VSOut
{
    float4 pos  : SV_POSITION;
    float2 uv   : TEXCOORD0;
};

//----------------------------------------------------------------------------------
// Vertex shader that generates a full-screen triangle with texcoords
//----------------------------------------------------------------------------------
PostProc_VSOut VS( uint id : SV_VertexID )
{
    PostProc_VSOut output = (PostProc_VSOut)0.0f;
    output.uv = float2( (id << 1) & 2, id & 2 );
    output.pos = float4( output.uv * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f) , 0.0f, 1.0f );
    return output;
}
//-------------------------------------------------------------------------
float4 PS( PostProc_VSOut IN) : SV_TARGET
{
    float ao = tAO.Sample(PointSampler, IN.uv);
    return saturate(pow(ao, 1));
}

