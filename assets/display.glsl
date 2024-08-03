@vs display_vs

in vec2 in_pos;
in vec2 in_uv;

out vec2 uv;

void main() {
    gl_Position = vec4(in_pos, 0, 1);
    uv = in_uv;
}

@end

@fs display_fs

uniform texture2D OffscreenRT;
uniform sampler Sampler;

in vec2 uv;
out vec4 frag_colour;

#if 1

void main() {
    frag_colour = texture(sampler2D(OffscreenRT, Sampler), uv);
}

#else

// Based on https://www.shadertoy.com/view/ltKBDd by battlebottle

float w0(float a) {
    return (1.0 / 6.0) * (a * (a * (-a + 3.0) - 3.0) + 1.0);
}

float w1(float a) {
    return (1.0 / 6.0) * (a * a * (3.0 * a - 6.0) + 4.0);
}

float w2(float a) {
    return (1.0 / 6.0) * (a * (a * (-3.0 * a + 3.0) + 3.0) + 1.0);
}

float w3(float a) {
    return (1.0 / 6.0) * (a * a * a);
}

// g0 and g1 are the two amplitude functions
float g0(float a) {
    return w0(a) + w1(a);
}

float g1(float a) {
    return w2(a) + w3(a);
}

// h0 and h1 are the two offset functions
float h0(float a) {
    return -1.0 + w1(a) / (w0(a) + w1(a));
}

float h1(float a) {
    return 1.0 + w3(a) / (w2(a) + w3(a));
}

vec4 sampleTex(vec2 st, float lod) {
    return textureLod(sampler2D(OffscreenRT, Sampler), st, lod);
}

vec4 texture_bicubic(vec2 st, vec4 texelSize, vec2 fullSize, float lod) {
	st = st * texelSize.zw + 0.5;
	vec2 iuv = floor(st);
	vec2 fuv = fract(st);

    float g0x = g0(fuv.x);
    float g1x = g1(fuv.x);
    float h0x = h0(fuv.x);
    float h1x = h1(fuv.x);
    float h0y = h0(fuv.y);
    float h1y = h1(fuv.y);

	vec2 p0 = (vec2(iuv.x + h0x, iuv.y + h0y) - 0.5) * texelSize.xy;
	vec2 p1 = (vec2(iuv.x + h1x, iuv.y + h0y) - 0.5) * texelSize.xy;
	vec2 p2 = (vec2(iuv.x + h0x, iuv.y + h1y) - 0.5) * texelSize.xy;
	vec2 p3 = (vec2(iuv.x + h1x, iuv.y + h1y) - 0.5) * texelSize.xy;

    vec2 lodFudge = pow(1.95, lod) / fullSize;

    return 
        g0(fuv.y) * 
        (
            g0x * sampleTex(p0, lod) + 
            g1x * sampleTex(p1, lod)
        ) + 
        g1(fuv.y) * 
        (
            g0x * sampleTex(p2, lod) + 
            g1x * sampleTex(p3, lod)
        );
}

vec2 getSize(int lod) {
    return textureSize(OffscreenRT, lod);
}

vec4 textureBicubic(float lod) {
    vec2 lodSizeFloor = vec2(getSize(int(lod)));
    vec2 lodSizeCeil = vec2(getSize(int(lod + 1.0)));
    vec2 fullSize = vec2(getSize(0));
    vec4 floorSample = texture_bicubic(uv, vec4(1.0 / lodSizeFloor.x, 1.0 / lodSizeFloor.y, lodSizeFloor.x, lodSizeFloor.y), fullSize, floor(lod));
    vec4 ceilSample = texture_bicubic(uv, vec4(1.0 / lodSizeCeil.x, 1.0 / lodSizeCeil.y, lodSizeCeil.x, lodSizeCeil.y), fullSize, ceil(lod));
    return mix(floorSample, ceilSample, fract(lod));
}

void main() {
    vec4 res = textureBicubic(0.5);

    //vec4 color = res;
    frag_colour = sampleTex(uv, 0.);
}
#endif

@end

@program display display_vs display_fs
