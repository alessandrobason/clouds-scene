@vs vs

in vec2 pos;

void main() {
    gl_Position = vec4(pos, 0, 1);
}
@end

@fs fs

uniform uniforms {
    vec2 Resolution;
    float Time;
};

uniform sampler NoiseSampler;
uniform texture2D NoiseTex;
uniform texture2D BlueNoiseTex;

out vec4 frag_colour;

#define MAX_STEPS  100
#define MARCH_SIZE 0.16

#define FLARE_BRIGHTNESS -4.
#define FLARE_WOOBLE .1

#define SUNPOS vec3(0.6, .3, -1)
#define SPEED .2

#define DAY cos(Time * (SPEED * .2)) * .5 + .5

#define LIGHTCOL   mix(vec3(0.3, 0.6, 1.), vec3(1, 0.6, 0.3), DAY)
#define SHADOWCOL  mix(vec3(0.25, 0.25, 0.35), vec3(0.6, 0.6, 0.75), DAY)
#define TOPCOL     mix(vec3(0.0, 0.0, 0.11), vec3(0.7, 0.7, 1.9), DAY)
#define HORCOL     mix(vec3(0.1, 0.1, 0.21), vec3(0.12, 0.05, 0.01), DAY)
#define GRADIENT   mix(vec3(0.1, 0.0, 0.2), vec3(0.9, 0.65, 0.), DAY)
#define SUNCOL     mix(vec3(0.64, 0.72, 0.8), vec3(1, 0.5, 0.3), DAY)

// quadratic polynomial (from iq)
float smin(float a, float b, float k) {
    k *= 4.0;
    float h = max(k-abs(a-b), 0.0)/k;
    return min(a,b) - h*h*k*(1.0/4.0);
}

// sdf of a box (from iq)
float sdf_box(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float noise(float t) {
	return texture(sampler2D(NoiseTex, NoiseSampler), vec2(t, .0) / 256.).x;
}

float noise(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3. - 2. * f);

	vec2 uv = (p.xy + vec2(37, 239) * p.z) + f.xy;
    vec2 tex = textureLod(sampler2D(NoiseTex, NoiseSampler), (uv +  .5) / 256.,  0.).yx;

    return mix(tex.x, tex.y, f.z) * 2. - 1.;
}

float fbm(vec3 p, int lod) {
    vec3 q = p + Time * .5 * vec3(1, -.2, -1);
    float g = noise(q);

    float f = 0.5 * noise(q);
    q *= 2.02;

    if (lod > 1) {
        f += 0.25 * noise(q);
        q *= 2.23;
    }

    if (lod > 2) {
        f += 0.125 * noise(q); 
        q = q * 2.41;
    }

    if (lod > 3) {
        f += 0.0625 * noise(q); 
        q = q * 2.62;
    }

    if (lod > 4) {
        f += 0.03125 * noise(q); 
    }

    return f;
}

float scene(vec3 p, int lod) {
    float scale = (cos(Time * .5 + .5) * 3. + 1.);
    float dist = sdf_box(p, vec3(10, 0.5, 10)) * mix(1, -1, 0.);
    float f = fbm(p, lod);

    return -dist + f;
}

vec4 raymarch(vec3 ro, vec3 rd, float offset) {
    vec4 sum = vec4(0.0);
    float depth = MARCH_SIZE * offset;
    vec3 sundir = normalize(SUNPOS);

    for (int i = 0; i < MAX_STEPS; ++i) {
        vec3 p = ro + rd * depth;
        int lod = 6 - int(log2(1.0 + depth * 0.5))*0;

        float density = scene(p, lod);
        if (density > 0.0) {
            float diffuse = clamp((scene(p, lod) - scene(p + sundir * 0.3, lod)) / 0.3, 0.0, 1.0);

            vec3 light = SHADOWCOL + LIGHTCOL * diffuse * 1.5;

            vec4 colour = vec4(mix(vec3(1), vec3(0), density), density);
            colour.rgb *= light * colour.a;
            sum += colour * (1.0 - sum.a);
        }

        if (sum.a >= 0.99) {
            break;
        }

        depth += MARCH_SIZE;
    }
    
    return sum;
}

float lensflare(vec2 uv, vec2 pos) {
	vec2 main = uv-pos;
	
    // atan(y, x) gives the angle between the vector and the x axis
    // by inverting the parameters, it's instead getting the angle to the y axis
	float ang = atan(main.x, main.y);

    float bloom = 1.0 / (length(main) * 16. + 1.);

    float flares = (
            sin(
                // sin(ang) will return 1 when y > 0, by multiplying it by n, it creates n cones
                // by negating it, it also negates the cones
                // using this, putting together sin and cos we can create varied looking cones
                // by adding pos.x and y, we create a moving effect when the light moves

                // the noise function gives "random" values to these streaks, but because the values are close together,
                // it creates soft streaks
                noise(sin(ang * 2.) * 4.0 - cos(ang * 3.)) 
                // interpolate the frequency to gives it a cool glowing effect :)
                * mix(6., 16, (cos(Time) * .5 + .5) * FLARE_WOOBLE)
            ) 
            // change range from [-1, 1] to [ .7, .9 ]
            * .1 + .9
        );
    
    // put together the bloom and lens flare
    flares *= bloom;
    bloom = pow(bloom + flares, 2. - FLARE_BRIGHTNESS);

	return clamp(bloom, 0, 1);
}

vec4 render(vec3 ro, vec3 rd, vec2 uv) {
    vec3 sundir = normalize(SUNPOS);
    float sun = clamp(dot(sundir, rd), 0.0, 1.0);

    // base sky colour
    vec3 col = clamp(vec3(lensflare(uv, SUNPOS.xy)), 0, 1);
    col += TOPCOL;
    // vertical gradient
    col -= GRADIENT * 0.8 * rd.y;
    // sun colour in the sky
    col += SUNCOL * 0.1;

    float dist = length(SUNPOS - rd) - .0;
    float hori = abs((ro.y - rd.y) * 5.);

    dist = max(1.0 - smin(hori, dist, .2), 0);

    col += HORCOL * dist;
    col = clamp(col, 0, 1);

    float blue_noise = texture(sampler2D(BlueNoiseTex, NoiseSampler), gl_FragCoord.xy / 1024.0).r;
    float offset = fract(blue_noise + fract(Time) * 128);

    vec4 res = raymarch(ro, rd, offset);
    col = col * (1.0 - res.w) + res.xyz;

    return vec4(col, 1.);
}

void main() {
    float aspect_ratio = Resolution.x / Resolution.y;
    vec2 uv = (gl_FragCoord.xy/Resolution.xy) - .5;
    uv.x *= aspect_ratio;

    float ypos = mix(-1.15, 1.15, cos(Time * SPEED) *.5 + .5);
    vec3 ro = vec3(0, ypos, 5);
    vec3 rd = normalize(vec3(uv, -1.0));

    frag_colour = render(ro, rd, uv);
}

@end

@program shader vs fs