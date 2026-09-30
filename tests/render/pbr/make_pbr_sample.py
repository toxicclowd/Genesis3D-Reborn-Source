"""Builds the PBR sample materials (roadmap Phase 2) for the render regression.

For each material below it derives source maps from the shipped bitmap and writes them as
PNG files to bin/GlobalMaterials/PBRSample/src/:
  <name>.png       the base color (the shipped bitmap)
  <name>_n.png     tangent-space normal map (DirectX convention, +Y down the image),
                   from the base texture's luminance used as a height map
  <name>_orm.png   R = occlusion (from the height map's cavities), G = roughness, B = metalness
                   (wallsectionb gets separate _ao, _rough and _metal maps instead, to test
                   G3DTexImport's packing; tech_blue also gets an _e emissive map)
Then G3DTexImport -material turns each set into BC-compressed, mipmapped DDS textures in
bin/GlobalMaterials/PBRSample/ (the "PBRSample" pak) and a version 2 <name>.jmat there.

The originals are not touched. Point the engine at the overrides with
    G3D_MATERIAL_OVERRIDES=GlobalMaterials\\PBRSample     (relative to bin/)
and a level that uses these materials (tutorial1) renders with them.

Needs Pillow, NumPy and a built G3DTexImport (bin/G3DTexImport.exe or G3DTexImportd.exe).
Usage: python tests/render/pbr/make_pbr_sample.py [path to G3DTexImport]
"""
import os
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
BIN = os.path.join(ROOT, 'bin')
MATERIALS = os.path.join(BIN, 'GlobalMaterials')
PAK = 'PBRSample'
OUT = os.path.join(MATERIALS, PAK)
SRC = os.path.join(OUT, 'src')

# name: (roughness, metal, normal strength, base tint)
PRESETS = {
    'silver':       (0.25, 1.0, 1.0, (1.0, 1.0, 1.0)),
    'door':         (0.45, 1.0, 2.0, (1.0, 0.85, 0.6)),	# brass
    'tech_blue':    (0.35, 1.0, 1.5, (1.0, 1.0, 1.0)),
    'marble':       (0.12, 0.0, 0.5, (1.0, 1.0, 1.0)),
    'wallsectionb': (0.85, 0.0, 3.0, (1.0, 1.0, 1.0)),
}
SEPARATE_ORM = {'wallsectionb'}
EMISSIVE = {'tech_blue'}


def blur(a, passes=2):
    for _ in range(passes):
        a = (a + np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 5.0
    return a


def to_image(channels):
    data = np.clip(np.stack(channels, -1) * 255.0 + 0.5, 0, 255).astype(np.uint8)
    return Image.fromarray(data, 'RGB')


def make_sources(name, roughness, metal, strength):
    base = Image.open(os.path.join(MATERIALS, name + '.bmp')).convert('RGB')
    base.save(os.path.join(SRC, name + '.png'))

    rgb = np.asarray(base, dtype=np.float32) / 255.0
    height = blur(rgb @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32), 1)
    # Wrapping central differences; x right (+u), y down (+v).
    hx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5
    hy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5
    n = np.stack([-hx * strength * 8.0, -hy * strength * 8.0, np.ones_like(height)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    to_image([n[..., 0] * 0.5 + 0.5, n[..., 1] * 0.5 + 0.5, n[..., 2] * 0.5 + 0.5]).save(
        os.path.join(SRC, name + '_n.png'))

    cavity = np.clip((blur(height, 4) - height) * 4.0, 0.0, 1.0)
    occlusion = 1.0 - cavity
    rough = np.clip(roughness + (0.5 - height) * 0.2 + cavity * 0.3, 0.0, 1.0)
    metalness = np.full_like(height, metal)
    if name in SEPARATE_ORM:
        for suffix, channel in (('_ao', occlusion), ('_rough', rough), ('_metal', metalness)):
            gray = np.clip(channel * 255.0 + 0.5, 0, 255).astype(np.uint8)
            Image.fromarray(gray, 'L').save(os.path.join(SRC, name + suffix + '.png'))
    else:
        to_image([occlusion, rough, metalness]).save(os.path.join(SRC, name + '_orm.png'))

    if name in EMISSIVE:
        glow = np.clip((height - 0.55) * 4.0, 0.0, 1.0)
        to_image([glow * 0.1, glow * 0.6, glow]).save(os.path.join(SRC, name + '_e.png'))


def find_tool(argv):
    if len(argv) > 1:
        return argv[1]
    for exe in ('G3DTexImport.exe', 'G3DTexImportd.exe'):
        path = os.path.join(BIN, exe)
        if os.path.exists(path):
            return path
    sys.exit('make_pbr_sample.py: build G3DTexImport first (Tools/TexImport)')


def main(argv):
    tool = find_tool(argv)
    os.makedirs(SRC, exist_ok=True)
    for name, (roughness, metal, strength, tint) in PRESETS.items():
        make_sources(name, roughness, metal, strength)
        args = [tool, '-material', name, os.path.join(SRC, name + '.png'),
                '-outdir', MATERIALS, '-pak', PAK, '-matdir', OUT, '-fast',
                '-tint', str(tint[0]), str(tint[1]), str(tint[2])]
        if name in EMISSIVE:
            args += ['-intensity', '2']
        subprocess.run(args, check=True, stdout=subprocess.DEVNULL)
        print('wrote', name)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
