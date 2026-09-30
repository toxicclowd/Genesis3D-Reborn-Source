"""Builds the PBR sample materials (roadmap Phase 2) for the render regression.

For each material below it writes, under bin/GlobalMaterials/PBRSample/:
  <name>_n.bmp    tangent-space normal map (DirectX convention, +Y down the image),
                  derived from the base texture's luminance used as a height map
  <name>_orm.bmp  R = occlusion (from the height map's cavities), G = roughness, B = metalness
  <name>.jmat     a version 2 copy of GlobalMaterials/<name>.jmat with the PBR block and
                  the two layers above ("PBRSample:<name>_n", "PBRSample:<name>_orm")

The originals are not touched. Point the engine at the overrides with
    G3D_MATERIAL_OVERRIDES=GlobalMaterials\\PBRSample     (relative to bin/)
and a level that uses these materials (tutorial1) renders with them.

Needs Pillow and NumPy. Usage: python tests/render/pbr/make_pbr_sample.py
"""
import os
import struct
import sys

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
MATERIALS = os.path.join(ROOT, 'bin', 'GlobalMaterials')
OUT = os.path.join(MATERIALS, 'PBRSample')
PAK = 'PBRSample'

# name: (roughness, metal, normal strength, base tint)
PRESETS = {
    'silver':       (0.25, 1.0, 1.0, (1.0, 1.0, 1.0)),
    'door':         (0.45, 1.0, 2.0, (1.0, 0.85, 0.6)),	# brass
    'tech_blue':    (0.35, 1.0, 1.5, (1.0, 1.0, 1.0)),
    'marble':       (0.12, 0.0, 0.5, (1.0, 1.0, 1.0)),
    'wallsectionb': (0.85, 0.0, 3.0, (1.0, 1.0, 1.0)),
}

# .jmat layout (grMaterialSpec.cpp)
TAG = b'JMAT'
FLAG_DIFFUSE, FLAG_SPECULAR, FLAG_AMBIENT, FLAG_EMISSIVE = 0x1, 0x2, 0x4, 0x8
FLAG_SHADER, FLAG_THUMBS, FLAG_SIZE, FLAG_PBR = 0x10, 0x20, 0x40, 0x80
NAME_SIZE = 256
XFORM_SIZE = 48
RESOURCE_BITMAP = 0x0002
LAYER_NORMAL, LAYER_ORM = 3, 4


def read_jmat(path):
    d = open(path, 'rb').read()
    assert d[:4] == TAG, path
    version, flags, count = d[4], struct.unpack_from('<H', d, 5)[0], d[7]
    assert version == 1 and not flags & FLAG_PBR, path
    p = 8
    for bit in (FLAG_DIFFUSE, FLAG_SPECULAR, FLAG_AMBIENT, FLAG_EMISSIVE):
        if flags & bit:
            p += 16
    if flags & FLAG_SHADER:
        p += NAME_SIZE
    if flags & FLAG_SIZE:
        p += 4
    header = d[8:p]
    layers = []
    for _ in range(count):
        layers.append(d[p:p + 4 + NAME_SIZE + XFORM_SIZE])
        p += 4 + NAME_SIZE + XFORM_SIZE
    return flags, header, layers, d[p:]


def layer(kind, layer_type, name, xform):
    return struct.pack('<HBB', kind, layer_type, 0) + name.encode().ljust(NAME_SIZE, b'\0') + xform


def pbr_block(roughness, metal, tint):
    # grMaterialSpec_PBR: BaseColor[4], Roughness, Metal, Emissive[3], EmissiveIntensity,
    # AlphaCutoff, AlphaMode, Reserved, Flags
    return struct.pack('<4f2f3f2fBBH', tint[0], tint[1], tint[2], 1.0, roughness, metal,
                       0.0, 0.0, 0.0, 1.0, 0.5, 0, 0, 0)


def blur(a, passes=2):
    for _ in range(passes):
        a = (a + np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 5.0
    return a


def make_maps(base, roughness, metal, strength):
    rgb = np.asarray(base.convert('RGB'), dtype=np.float32) / 255.0
    height = blur(rgb @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32), 1)
    # Wrapping central differences; x right (+u), y down (+v).
    hx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5
    hy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5
    n = np.stack([-hx * strength * 8.0, -hy * strength * 8.0, np.ones_like(height)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    normal = np.clip((n * 0.5 + 0.5) * 255.0 + 0.5, 0, 255).astype(np.uint8)

    cavity = np.clip((blur(height, 4) - height) * 4.0, 0.0, 1.0)
    occlusion = 1.0 - cavity
    rough = np.clip(roughness + (0.5 - height) * 0.2 + cavity * 0.3, 0.0, 1.0)
    orm = np.stack([occlusion, rough, np.full_like(height, metal)], -1)
    orm = np.clip(orm * 255.0 + 0.5, 0, 255).astype(np.uint8)
    return Image.fromarray(normal, 'RGB'), Image.fromarray(orm, 'RGB')


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, (roughness, metal, strength, tint) in PRESETS.items():
        flags, header, layers, tail = read_jmat(os.path.join(MATERIALS, name + '.jmat'))
        xform = layers[0][4 + NAME_SIZE:]
        normal, orm = make_maps(Image.open(os.path.join(MATERIALS, name + '.bmp')), roughness, metal, strength)
        normal.save(os.path.join(OUT, name + '_n.bmp'))
        orm.save(os.path.join(OUT, name + '_orm.bmp'))

        layers = layers + [layer(RESOURCE_BITMAP, LAYER_NORMAL, '%s:%s_n' % (PAK, name), xform),
                           layer(RESOURCE_BITMAP, LAYER_ORM, '%s:%s_orm' % (PAK, name), xform)]
        # The PBR block follows the colors, shader name and size, before the layers.
        data = (TAG + struct.pack('<BHB', 2, flags | FLAG_PBR, len(layers)) + header +
                pbr_block(1.0, 1.0, tint) + b''.join(layers) + tail)
        open(os.path.join(OUT, name + '.jmat'), 'wb').write(data)
        print('wrote', name)
    return 0


if __name__ == '__main__':
    sys.exit(main())
