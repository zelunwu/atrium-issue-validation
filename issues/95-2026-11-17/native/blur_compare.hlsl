// Standalone filter comparison, not Atrium product shader code.
// The Kawase pass uses four diagonal bilinear samples at an increasing offset.
Texture2D Source : register(t0);
SamplerState Linear : register(s0);

cbuffer FilterParams : register(b0)
{
    float2 InvSourceSize;
    float Offset;
    float BlurLod;
};

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

float4 Baseline(VSOut i) : SV_Target
{
    float2 d = InvSourceSize * (exp2(BlurLod) * 0.75);
    float3 c = Source.SampleLevel(Linear, i.uv, BlurLod).rgb * 0.4;
    c += Source.SampleLevel(Linear, i.uv + float2(d.x, 0), BlurLod).rgb * 0.15;
    c += Source.SampleLevel(Linear, i.uv - float2(d.x, 0), BlurLod).rgb * 0.15;
    c += Source.SampleLevel(Linear, i.uv + float2(0, d.y), BlurLod).rgb * 0.15;
    c += Source.SampleLevel(Linear, i.uv - float2(0, d.y), BlurLod).rgb * 0.15;
    return float4(c, 1);
}

float4 Downsample(VSOut i) : SV_Target
{
    float2 d = InvSourceSize * 1.5;
    float3 c = Source.SampleLevel(Linear, i.uv + float2( d.x,  d.y), 0).rgb;
    c += Source.SampleLevel(Linear, i.uv + float2(-d.x,  d.y), 0).rgb;
    c += Source.SampleLevel(Linear, i.uv + float2( d.x, -d.y), 0).rgb;
    c += Source.SampleLevel(Linear, i.uv + float2(-d.x, -d.y), 0).rgb;
    return float4(c * 0.25, 1);
}

float4 Kawase(VSOut i) : SV_Target
{
    float2 d = InvSourceSize * Offset;
    float3 c = Source.SampleLevel(Linear, i.uv + float2( d.x,  d.y), 0).rgb;
    c += Source.SampleLevel(Linear, i.uv + float2(-d.x,  d.y), 0).rgb;
    c += Source.SampleLevel(Linear, i.uv + float2( d.x, -d.y), 0).rgb;
    c += Source.SampleLevel(Linear, i.uv + float2(-d.x, -d.y), 0).rgb;
    return float4(c * 0.25, 1);
}

float4 Upsample(VSOut i) : SV_Target
{
    return Source.SampleLevel(Linear, i.uv, 0);
}
