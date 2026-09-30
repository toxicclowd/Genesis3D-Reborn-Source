"""Generates PBR versions of the World Editor's material library.

For every bitmap material in bin/GlobalMaterials (<name>.bmp) this writes <name>PBR.jmat
next to it, a PBR material with BC-compressed, mipmapped DDS maps in the "PBR" pak
(bin/GlobalMaterials/PBR/). The editor lists them with the other materials; the originals
are not touched.

Each texture is decoded by G3DTexImport (-decode, which also reads the engine's own
compressed bitmaps and turns a color key into alpha). From it:
  - a tangent-space normal map (DirectX convention) from the luminance used as a height map,
  - textures that are not a power of two are scaled up to the next one,
  - an ORM map: occlusion from the height map's cavities, roughness and metalness from the
    material's name (metal, stone, wood, glass, ...; see PRESETS), roughness varied a little
    by the texture so the surface is not uniform,
and G3DTexImport -material builds the material (color-keyed textures become cutout).

These are derived maps, a starting point rather than authored PBR: re-import a material
with real maps from the World Editor's Material Editor (right-click it in the Textures panel).

Needs Pillow, NumPy and a built G3DTexImport (bin/G3DTexImport.exe or G3DTexImportd.exe; the
engine DLL next to it reads the engine's bitmaps).
Usage: python Tools/TexImport/make_pbr_library.py [-only name,...] [-force] [-jobs n] [-tool path]
  -only   just these materials (base names, comma separated)
  -force  rebuild materials that already exist (default: skip them)
"""
import argparse
import concurrent.futures
import glob
import os
import shutil
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
BIN = os.path.join(ROOT, 'bin')
MATERIALS = os.path.join(BIN, 'GlobalMaterials')
PAK = 'PBR'
SUFFIX = 'PBR'

# (name keywords, roughness, metalness, normal strength), first match wins; lower case.
PRESETS = [
    (('sky', 'mist', 'flame', 'smoke', 'fx0', 'sparkly', 'splash', 'a_trans', 'bloodsplat', 'light'),
        1.0, 0.0, 0.0),
    (('glass', 'crystal', 'water', 'ripples', 'lily', 'hotmud', 'lava'),
        0.08, 0.0, 0.6),
    (('silver', 'gold', 'chrome', 'crome', 'metal', 'mtl', 'patina', 'coil', 'pipe', 'container', 'machine',
      'bomber', 'aagun', 'missle', 'engine', 'turret', 'greeblies', 'tech_', 'compartment', 'buldfans', 'rail',
      'trim', 'vfsidepanel', 'grndunderbelly', 'trains', 'door'),
        0.4, 1.0, 1.0),
    (('marb', 'marble', 'obelisk', 'tile', 'embtiles', 'rings'),
        0.3, 0.0, 0.8),
    (('wood', 'plank', 'crate', 'gate', 'bolt', 'woodrk', 'woodzig'),
        0.7, 0.0, 1.5),
    (('snow', 'ice'),
        0.55, 0.0, 1.5),
    (('skin', 'feather', 'canvas', 'france', 'rhine'),
        0.6, 0.0, 1.0),
    (('stone', 'ston', 'rock', 'brick', 'brik', 'wall', 'sand', 'ground', 'grass', 'gravel', 'mud', 'dirt',
      'crete', 'pave', 'cinder', 'blok', 'block', 'stucco', 'pyramid', 'terrain', 'roots', 'jungle', 'jung',
      'ruff', 'kingwall', 'dusty', 'blkrock', 'grnstone', 'hallway', 'minel', 'pot0'),
        0.9, 0.0, 2.5),
]
DEFAULT = (0.8, 0.0, 1.5)


def preset(name):
    lower = name.lower()
    for keywords, roughness, metal, strength in PRESETS:
        if any(k in lower for k in keywords):
            return roughness, metal, strength
    return DEFAULT


def blur(a, passes=2):
    for _ in range(passes):
        a = (a + np.roll(a, 1, 0) + np.roll(a, -1, 0) + np.roll(a, 1, 1) + np.roll(a, -1, 1)) / 5.0
    return a


def to_image(channels):
    data = np.clip(np.stack(channels, -1) * 255.0 + 0.5, 0, 255).astype(np.uint8)
    return Image.fromarray(data, 'RGB')


def make_sources(base_png, stem, roughness, metal, strength):
    """Writes <stem>_n.png and <stem>_orm.png next to the base; returns True if it has a color key."""
    image = Image.open(base_png).convert('RGBA')
    rgba = np.asarray(image, dtype=np.float32) / 255.0
    keyed = bool((rgba[..., 3] < 0.5).any())
    height = blur(rgba[..., :3] @ np.array([0.2126, 0.7152, 0.0722], dtype=np.float32), 1)

    # Wrapping central differences; x right (+u), y down (+v)
    hx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) * 0.5
    hy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) * 0.5
    n = np.stack([-hx * strength * 8.0, -hy * strength * 8.0, np.ones_like(height)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    to_image([n[..., 0] * 0.5 + 0.5, n[..., 1] * 0.5 + 0.5, n[..., 2] * 0.5 + 0.5]).save(stem + '_n.png')

    cavity = np.clip((blur(height, 4) - height) * 4.0, 0.0, 1.0) if strength > 0.0 else np.zeros_like(height)
    occlusion = 1.0 - cavity
    rough = np.clip(roughness + (0.5 - height) * 0.15 + cavity * 0.2, 0.02, 1.0)
    metalness = np.full_like(height, metal)
    to_image([occlusion, rough, metalness]).save(stem + '_orm.png')
    return keyed


def find_tool(path):
    if path:
        return path
    tools = [os.path.join(BIN, exe) for exe in ('G3DTexImport.exe', 'G3DTexImportd.exe')]
    tools = [t for t in tools if os.path.exists(t)]
    if not tools:
        sys.exit('make_pbr_library.py: build G3DTexImport first (Tools/TexImport)')
    return max(tools, key=os.path.getmtime)


def build(tool, bmp, work):
    name = os.path.splitext(os.path.basename(bmp))[0]
    material = name + SUFFIX
    stem = os.path.join(work, name)
    base = stem + '.png'
    subprocess.run([tool, '-decode', bmp, base], check=True, stdout=subprocess.DEVNULL, cwd=BIN)
    # A few textures are not a power of two (512x510...); the engine's texture coordinates
    # expect one, and block compression needs its mipmaps, so scale up to the next one.
    image = Image.open(base)
    pot = tuple(1 << (size - 1).bit_length() for size in image.size)
    if pot != image.size:
        image.resize(pot, Image.LANCZOS).save(base)
    roughness, metal, strength = preset(name)
    keyed = make_sources(base, stem, roughness, metal, strength)
    args = [tool, '-material', material, base, '-outdir', MATERIALS, '-pak', PAK, '-matdir', MATERIALS,
            '-normal', stem + '_n.png', '-orm', stem + '_orm.png', '-fast']
    if keyed:
        args += ['-cutout', '0.5']
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, cwd=BIN)
    return '%s (roughness %.2f, metal %.0f%s)' % (material, roughness, metal, ', cutout' if keyed else '')


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('-only', default='')
    parser.add_argument('-force', action='store_true')
    parser.add_argument('-jobs', type=int, default=max(1, (os.cpu_count() or 4) // 2))
    parser.add_argument('-tool', default='')
    args = parser.parse_args()
    tool = find_tool(args.tool)

    only = {n.strip().lower() for n in args.only.split(',') if n.strip()}
    bitmaps = []
    for bmp in sorted(glob.glob(os.path.join(MATERIALS, '*.bmp'))):
        name = os.path.splitext(os.path.basename(bmp))[0]
        if name.endswith(SUFFIX) or (only and name.lower() not in only):
            continue
        if not args.force and os.path.exists(os.path.join(MATERIALS, name + SUFFIX + '.jmat')):
            continue
        bitmaps.append(bmp)

    os.makedirs(os.path.join(MATERIALS, PAK), exist_ok=True)
    work = tempfile.mkdtemp(prefix='g3d_pbr_')
    failed = []
    try:
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
            jobs = {pool.submit(build, tool, bmp, work): bmp for bmp in bitmaps}
            for done, job in enumerate(concurrent.futures.as_completed(jobs), 1):
                bmp = jobs[job]
                try:
                    print('[%d/%d] %s' % (done, len(bitmaps), job.result()), flush=True)
                except Exception as error:
                    failed.append(os.path.basename(bmp))
                    print('[%d/%d] FAILED %s: %s' % (done, len(bitmaps), os.path.basename(bmp), error), flush=True)
    finally:
        shutil.rmtree(work, ignore_errors=True)

    print('%d materials written, %d failed%s' % (len(bitmaps) - len(failed), len(failed),
                                                ': ' + ', '.join(failed) if failed else ''))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
