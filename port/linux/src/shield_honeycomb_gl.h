/* Procedural shield surface. Included by the pixel shader translator; a zero
scale leaves the original fragment untouched. One grid follows the model UVs,
without blending overlapping projections. Derivative widths soften subpixel
edges. No extracted Infinite textures or shader code. */
#ifndef SHIELD_HONEYCOMB_GL_H
#define SHIELD_HONEYCOMB_GL_H

static const char shield_honeycomb_glsl[] =
	"in vec2 shield_uv;\n"
	"uniform vec4 shield_honeycomb[3];\n"
	"float shield_hex(vec2 p, float time)\n"
	"{\n"
	"    vec2 period = vec2(1.7320508, 3.0);\n"
	"    vec2 a = mod(p, period) - period * 0.5;\n"
	"    vec2 b = mod(p - period * 0.5, period) - period * 0.5;\n"
	"    vec2 q = dot(a,a) < dot(b,b) ? a : b;\n"
	"    vec2 cell = p - q;\n"
	"    q = abs(q);\n"
	"    float edge = 0.8660254 - max(q.x, dot(q, vec2(0.5,0.8660254)));\n"
	"    float aa = max(fwidth(edge), 0.002);\n"
	"    float line = 1.0 - smoothstep(0.045-aa, 0.045+aa, edge);\n"
	"    float glow = exp(-max(edge,0.0)*12.0);\n"
	"    float pulse = pow(0.5 + 0.5*sin(dot(cell,vec2(1.71,2.37)) + time*2.2), 8.0);\n"
	"    return (0.012 + line*0.7 + glow*0.24) * (0.65 + 1.5*pulse);\n"
	"}\n"
	"vec4 shield_honeycomb_surface(vec4 original)\n"
	"{\n"
	"    if (shield_honeycomb[0].w == 0.0) return original;\n"
	"    float time = shield_honeycomb[1].w;\n"
	"    float pattern = shield_hex(shield_uv * shield_honeycomb[0].w, time);\n"
	"    return vec4(shield_honeycomb[2].rgb + shield_honeycomb[1].rgb*0.65, shield_honeycomb[2].a*xD0.a*pattern);\n"
	"}\n";

#endif
