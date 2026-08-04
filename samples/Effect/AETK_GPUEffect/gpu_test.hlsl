Texture2D<float4> InputTexture : register(t0);
RWTexture2D<float4> OutputTexture : register(u0);

cbuffer Params : register(b0)
{
    uint width;
    uint height;
    float invert_ratio;
    float pad0;
};

[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    if (dispatchThreadID.x >= width || dispatchThreadID.y >= height)
        return;

    float4 color = InputTexture[dispatchThreadID.xy];
    
    // Invert RGB channels, keeping Alpha (.a / .w) unchanged
    float3 inverted = 1.0f - color.rgb;
    color.rgb = lerp(color.rgb, inverted, invert_ratio);
    
    OutputTexture[dispatchThreadID.xy] = color;
}
