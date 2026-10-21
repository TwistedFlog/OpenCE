"""Draw-local honeycomb uniforms must preserve colours and clear between passes."""
import os
from pathlib import Path
import shutil
import subprocess

import pytest
from harness import function

ROOT = Path(__file__).resolve().parents[1]


def test_draw_uniform_lifetime(tmp_path):
    cc = shutil.which("clang")
    if not cc:
        pytest.skip("clang required")
    renderer = (ROOT / "port/linux/src/d3d8_gl.c").read_text()
    source = """
#include <assert.h>
#include <string.h>
static float shield_honeycomb_value[3][4];
""" + function(renderer, "d3d8_gl_set_shield_honeycomb") + """
int main(void) {
    float difference[4] = {.1f, -.2f, .3f, 0.f};
    float parallel[4] = {.5f, .6f, .7f, .47f};
    float saved[4];
    memcpy(saved, parallel, sizeof(saved));
    assert(shield_honeycomb_value[0][3] == 0.f);
    d3d8_gl_set_shield_honeycomb(18.f, 12.f, difference, parallel);
    assert(shield_honeycomb_value[0][3] == 18.f);
    assert(!memcmp(shield_honeycomb_value[1], difference, 3 * sizeof(float)));
    assert(shield_honeycomb_value[1][3] == 12.f);
    assert(!memcmp(shield_honeycomb_value[2], saved, sizeof(saved)));
    assert(!memcmp(parallel, saved, sizeof(saved)));
    d3d8_gl_set_shield_honeycomb(0.f, 0.f, NULL, NULL);
    assert(shield_honeycomb_value[0][3] == 0.f);
    parallel[0] = 0.f; parallel[1] = 1.f; parallel[2] = 0.f;
    d3d8_gl_set_shield_honeycomb(45.f, 13.f, difference, parallel);
    assert(!memcmp(shield_honeycomb_value[2], parallel, sizeof(parallel)));
    assert(shield_honeycomb_value[0][3] == 45.f);
    d3d8_gl_set_shield_honeycomb(0.f, 0.f, NULL, NULL);
    assert(shield_honeycomb_value[0][3] == 0.f);
    return 0;
}
"""
    path = tmp_path / "uniforms.c"
    path.write_text(source)
    exe = tmp_path / ("uniforms.exe" if os.name == "nt" else "uniforms")
    subprocess.run([cc, "-std=gnu89", "-Wall", "-Werror", str(path), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


def test_only_owned_plasma_draws_activate_surface():
    source = (ROOT / "source/rasterizer/xbox/rasterizer_xbox_plasma_energy.c").read_text()
    draw = function(source, "rasterizer_plasma_energy_draw")
    start = draw.index("honeycomb_scale = shield_look_honeycomb_scale(group->shader);")
    submit = draw.index("rasterizer_transparent_geometry_group_draw__internal(", start)
    clear = draw.index("d3d8_gl_set_shield_honeycomb(0.f", submit)
    assert draw.index("overshield_effect_color(") < start < submit < clear
    assert "color_constants[1], color_constants[2]" in draw[start:submit]
    assert "D3DCULL_CCW" in draw[start:submit]
    assert "D3DCULL_NONE" in draw[submit:clear]
    glsl = (ROOT / "port/linux/src/shield_honeycomb_gl.h").read_text()
    assert "if (shield_honeycomb[0].w == 0.0) return original;" in glsl
