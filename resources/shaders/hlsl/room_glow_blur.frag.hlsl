/*
 * The room glow's blur: one direction of one of its three passes
 * (effect_obj.c SetRoomGlow, kind 0x10; see ReiPan_RendererDrawRoomGlow in
 * reipan_renderer.h).
 *
 * Each PS2 pass drew the buffer eight times additively: four copies offset
 * diagonally by r and four along the axes by 1.4r, every one at the same
 * alpha.  Per axis that star spreads the light with a variance of 0.99 r^2,
 * and its eight taps together gain 8 * alpha / 128.  This is a Gaussian of
 * that same variance -- uStep.z is its sigma, already converted into this
 * buffer's texels -- applied across then down, with the gain taken in the
 * second direction only, so the clamp at white falls where the PS2's did:
 * once per pass, on the finished sum.
 *
 * Outside the frame is black, as it was in the work buffer: the shifted
 * copies the PS2 drew left the frame's edges with fewer taps, so the glow
 * thins toward the edge of the screen.  The weights still count the taps that
 * fall outside, which is what reproduces that.
 *
 * Taps go in bilinear pairs, so a kernel of half-width n costs n / 2 + 1 reads
 * per direction.
 */

cbuffer RoomGlowBlurUniforms : register(b0, space3)
{
    /* xy = one texel along the blur's direction in UV, z = sigma in texels,
     * w = the gain applied to the result. */
    float4 uStep;
    /* x = the kernel's half-width in texels, yzw spare. */
    float4 uKernel;
};

Texture2D uTexture : register(t0, space2);
SamplerState uTextureSampler : register(s0, space2);

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
};

float3 Tap(float2 uv)
{
    const float inside = (all(uv >= 0.0.xx) && all(uv <= 1.0.xx)) ? 1.0 : 0.0;
    return uTexture.SampleLevel(uTextureSampler, uv, 0.0).rgb * inside;
}

float4 main(PSInput input) : SV_Target0
{
    const float sigma = max(uStep.z, 1.0e-3);
    const float falloff = -0.5 / (sigma * sigma);
    const int radius = (int)uKernel.x;

    float3 sum = Tap(input.vUV);
    float total = 1.0;

    [loop]
    for (int i = 1; i <= radius; i += 2)
    {
        const float w1 = exp(falloff * (float)(i * i));
        const float w2 = (i + 1 <= radius)
            ? exp(falloff * (float)((i + 1) * (i + 1))) : 0.0;
        const float w = w1 + w2;
        /* Where between texels i and i + 1 one bilinear read returns their
         * weighted sum. */
        const float offset = ((float)i * w1 + (float)(i + 1) * w2) / w;
        const float2 d = uStep.xy * offset;

        sum += (Tap(input.vUV + d) + Tap(input.vUV - d)) * w;
        total += 2.0 * w;
    }

    return float4(min(sum / total * uStep.w, 1.0.xxx), 1.0);
}
