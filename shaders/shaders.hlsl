struct PSInput
{
    float4 position : SV_POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 g_mWorldViewProj;
};

struct MaterialConstants
{
    uint matIndex;    // Dynamically set index for looking up from g_txMats[].
};

ConstantBuffer<MaterialConstants> materialConstants : register(b0, space0);
Texture2D g_texture[] : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position : POSITION, float4 normal : NORMAL, float4 color : COLOR, float2 uv : TEXCOORD )
{
    PSInput result;

    result.position = mul(position, g_mWorldViewProj);
    result.normal = normal;
    result.color = color;
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
   return g_texture[materialConstants.matIndex].Sample(g_sampler, input.uv);
}
