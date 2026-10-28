sampler2D Diffuse : register(s0);
sampler2D Mask : register(s1);
sampler2D Albedo : register(s2);
sampler2D Spec : register(s3);
sampler2D Scene : register(s4);
float4 Limit : register(c3);
float4 Param : register(c0);
float4 Dir : register(c1);
float4 Range : register(c2);
float4 Kernel[20] : register(c4);

float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float id = tex2D(Mask, uv).a;
    clip(id > 0 ? 1 : -1);
    clip(abs(id - Param.w) < 0.01 ? 1 : -1);
    float4 b = tex2D(Albedo, uv);
    float3 a = tex2D(Diffuse, uv).rgb;
    float f = Range.x * b.a;
    float fade = f <= Range.y ? 1 : (f < Range.z ? (Range.z - f) / (Range.z - Range.y) : 0);
    float2 step = Dir.xy * (Param.z * fade * 0.012 / 3.0);
    float3 o;
    if (Param.x == 1) {
        o = a;
        if (Dir.y != 0) o = a * b.rgb + tex2D(Spec, uv).rgb;
    } else {
        float3 acc = Kernel[0].rgb * a, w = Kernel[0].rgb;
        [unroll] for (int i = 1; i < 20; i++) {
            if (i < Param.y) {
                float2 t = uv + Kernel[i].w * step;
                float s = saturate(1 - abs(2 * (id - tex2Dlod(Mask, float4(t, 0, 0)).a)));
                acc += tex2Dlod(Diffuse, float4(t, 0, 0)).rgb * Kernel[i].rgb * s;
                w += Kernel[i].rgb * s;
            }
        }
        o = acc / max(w, 0.0001);
        if (abs(Dir.y - 1) < 0.01) {
            float3 s = tex2D(Spec, uv).rgb;

            float3 expected = min(b.rgb * tex2D(Mask, uv).rgb + s, Limit.x);
            float3 scene = tex2D(Scene, uv).rgb;
            clip(all(abs(scene - expected) <= Limit.y * (1 + expected)) ? 1 : -1);
            o = b.rgb * o + s;
            if (Limit.z > 0) o = lerp(o, float3(1, 0, 1), 0.5);
        }
    }
    return float4(o, 1);
}
