struct PSInput
{
    float4 position : SV_POSITION;
    float4 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct MaterialConstants
{
    uint matIndex;
};

ConstantBuffer<MaterialConstants> materialConstants : register(b0, space0);
Texture2D g_texture[2] : register(t0);
SamplerState g_sampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
   return g_texture[materialConstants.matIndex].Sample(g_sampler, input.uv);
}