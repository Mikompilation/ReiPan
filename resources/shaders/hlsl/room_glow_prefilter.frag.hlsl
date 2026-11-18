/*
 * The room glow, pass one: the frame shrunk into the glow buffer, then the
 * bright pass taken off it (effect_obj.c SetRoomGlow, kind 0x10; see
 * ReiPan_RendererDrawRoomGlow in reipan_renderer.h).
 *
 * The PS2 drew the whole frame into about 141x99 pixels of a work buffer, so
 * each of those pixels held the average of a box roughly 4.5 frame pixels on
 * a side, and only then was the colour subtracted.  The order matters,
 * because the subtract is a threshold: a highlight smaller than the box is
 * averaged down before it is tested, and does not glow on its own.  This pass
 * keeps that box -- uSource.zw is it, in source texels, whatever the render
 * resolution -- and only samples it more finely, so the glow buffer can be far
 * denser than the PS2's without a single extra highlight crossing the
 * threshold.
 *
 * The bright pass is the GS's own arithmetic on 0..1 values: the boost added
 * (Cs + Cd, clamped at white), then the colour subtracted (Cd - Cs, clamped at
 * black).  uBright.rgb already holds the area colour plus the boost, clamped,
 * exactly as SetRoomGlow hands it to the subtract; with the boost added first
 * that caps every channel's glow at 1 - uBright.rgb.
 */

cbuffer RoomGlowPrefilterUniforms : register(b0, space3)
{
    /* xy = one source texel in UV, zw = the box one glow texel averages, in
     * source texels. */
    float4 uSource;
    /* xy = taps across and down the box, zw spare. */
    float4 uTaps;
    /* rgb = the colour subtracted, a = the boost added first; both 0..1. */
    float4 uBright;
};

Texture2D uTexture : register(t0, space2);
SamplerState uTextureSampler : register(s0, space2);

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    const int nx = max((int)uTaps.x, 1);
    const int ny = max((int)uTaps.y, 1);

    /* Taps spread evenly over the box, each a bilinear read, so a box of 2n
     * texels is covered by n taps and every texel in it counts. */
    const float2 box = uSource.zw * uSource.xy;
    const float2 spacing = box / float2(nx, ny);
    const float2 origin = input.vUV - 0.5 * box + 0.5 * spacing;

    float3 sum = 0.0.xxx;
    [loop]
    for (int j = 0; j < ny; j++)
    {
        [loop]
        for (int i = 0; i < nx; i++)
        {
            sum += uTexture.SampleLevel(uTextureSampler,
                                        origin + float2(i, j) * spacing,
                                        0.0).rgb;
        }
    }
    const float3 shrunk = sum / (float)(nx * ny);

    const float3 glow = max(min(shrunk + uBright.a, 1.0.xxx) - uBright.rgb,
                            0.0.xxx);
    return float4(glow, 1.0);
}
