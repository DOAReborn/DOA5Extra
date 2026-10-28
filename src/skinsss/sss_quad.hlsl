float4 Offset : register(c0);
struct Out { float4 p : POSITION; float2 uv : TEXCOORD0; };
Out main(float3 p : POSITION, float2 uv : TEXCOORD0)
{
    Out o;
    o.p = float4(p.xy + Offset.xy, 0, 1);
    o.uv = uv;
    return o;
}
