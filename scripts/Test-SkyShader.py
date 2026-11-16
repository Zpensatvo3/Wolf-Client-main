"""Compile and render the shipping Sky GLSL using a headless EGL context.

Requires moderngl and an EGL driver; does not launch Minecraft.
"""
from pathlib import Path
import math
import re
import struct
import moderngl

ROOT = Path(__file__).resolve().parent.parent
source = (ROOT / "src/modules/visual/sky_panorama.cpp").read_text(encoding="utf-8")
shaders = dict(re.findall(r'const char\* (vertex_source|fragment_source)=R"\((.*?)\)";', source, re.S))
context = moderngl.create_standalone_context(require=320, backend="egl")
program = context.program(vertex_shader=shaders["vertex_source"], fragment_shader=shaders["fragment_source"])
array = context.vertex_array(program, [])
texture = context.texture((4, 4), 3, bytes([25, 40, 65]) * 16)
texture.build_mipmaps()
texture.use(0)
next_texture = context.texture((4, 4), 3, bytes([170, 60, 130]) * 16)
next_texture.build_mipmaps()
next_texture.use(1)
target = context.texture((64, 64), 4, dtype="f4")
frame = context.framebuffer([target])
frame.use()
program["Panorama"] = 0
program["PanoramaNext"] = 1
program["InverseViewProjection"].write(struct.pack("16f", *[1. if i % 5 == 0 else 0. for i in range(16)]))
for name, value in dict(Rotation=0., Exposure=0., Opacity=.85, SkyCrop=1., Stylized=0.,
                        FogVisibility=1., HazeHeight=.14, MistHeight=.5, MotionTime=0.,
                        AtmosphereDensity=.7, Atmosphere=0., PanoramaBlend=0.,
                        NextCrop=1., NextStylized=0.).items():
    program[name] = value
program["FogColor"] = (.2, .3, .4)


def render(style, time=0., density=.7):
    program["Atmosphere"] = style
    program["MotionTime"] = time
    program["AtmosphereDensity"] = density
    array.render(mode=moderngl.TRIANGLES, vertices=3)
    values = struct.unpack("16384f", frame.read(components=4, dtype="f4"))
    assert all(math.isfinite(v) for v in values)
    assert all(0. <= v <= 1. for v in values)
    assert all(abs(values[i] - .85) < .00001 for i in range(3, len(values), 4))
    return values


plain = render(0.)
for style in (2., 3., 4.):
    still, moving = render(style), render(style, 10.)
    assert max(abs(a-b) for a, b in zip(plain, still)) > .001
    assert max(abs(a-b) for a, b in zip(still, moving)) > .001
    assert render(style, 10., 0.) == plain
    # Lower half is below the horizon: the fog seam must stay exact.
    for i in range(0, 64 * 32 * 4, 4):
        assert max(abs(still[i+c] - (.2, .3, .4)[c]) for c in range(3)) < .00001
    assert render(style, 10.) == moving  # Frozen clock produces a stable sky.

# Texture crossfade keeps opacity and brightness continuous, including endpoints.
program["PanoramaBlend"] = 1.
second = render(0.)
program["PanoramaBlend"] = .5
middle = render(0.)
assert max(abs(m-(a+b)*.5) for a, b, m in zip(plain, second, middle)) < .00001
assert max(abs(a-b) for a, b in zip(plain, second)) > .1
program["PanoramaBlend"] = 0.

# Bright artwork plus aurora must remain finite and within display range.
for exposure in (-2., 2.):
    program["Exposure"] = exposure
    for height in (.08, .65):
        program["MistHeight"] = height
        for stylized in (0., 1.):
            program["Stylized"] = stylized
            for style in (0., 2., 3., 4.):
                render(style, 628.3, 1.)
program["Exposure"] = 0.
program["Stylized"] = 0.
program["MistHeight"] = .5

# Shared phase wrap is continuous: .09/.11/.13/.17/.25 all divide this period.
for style in (2., 3., 4.):
    before, after = render(style, 628.3185307179586-.0001), render(style, .0001)
    assert max(abs(a-b) for a, b in zip(before, after)) < .0001

# Dense vanilla fog must mask the entire atmosphere (e.g. low visibility).
program["FogVisibility"] = 0.
masked = render(3., 10., 1.)
for i in range(0, len(masked), 4):
    assert max(abs(masked[i+c] - (.2, .3, .4)[c]) for c in range(3)) < .00001
program["FogVisibility"] = 1.

# The fullscreen sky must not paint over geometry with nearer depth.
depth = context.depth_renderbuffer((64, 64))
occluded = context.framebuffer([target], depth)
occluded.use()
occluded.clear(.13, .17, .23, 1., depth=.5)
context.enable(moderngl.DEPTH_TEST)
context.depth_func = "<="
array.render(mode=moderngl.TRIANGLES, vertices=3)
pixels = struct.unpack("16384f", occluded.read(components=4, dtype="f4"))
for i in range(0, len(pixels), 4):
    assert max(abs(pixels[i+c] - (.13, .17, .23, 1.)[c]) for c in range(4)) < .00001

print("PASS Sky shader: compile/link, atmosphere animation, crossfade, extremes, phase wrap, fog and terrain depth")
