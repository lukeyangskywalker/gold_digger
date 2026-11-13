"""Host checks with the Zig toolchain cached by ../Spartan (or --zig):
core tests, then the headless UI smoke test that renders the real Passport UI
and writes screenshots to build/shots/ (PPM plus a contact sheet PNG)."""
import argparse
import struct
import subprocess
import sys
import zlib
from pathlib import Path

root = Path(__file__).resolve().parent.parent


def zig_cc(zig, out_dir, args):
    out_dir.mkdir(parents=True, exist_ok=True)
    rsp = out_dir / 'compile.rsp'
    rsp.write_text(subprocess.list2cmdline(args), encoding='utf-8')
    subprocess.run([str(zig), 'cc', '@' + str(rsp)], cwd=root, check=True)


def contact_sheet(shots):
    files = sorted(shots.glob('*.ppm'))
    if not files:
        return
    cols, cw, ch = 5, 250, 330
    w, h = cols * cw, ((len(files) + cols - 1) // cols) * ch
    px = bytearray(b'\x28' * (w * h * 3))
    for i, f in enumerate(files):
        data = f.read_bytes().split(b'\n', 3)[3]
        ox, oy = (i % cols) * cw + 5, (i // cols) * ch + 5
        for y in range(320):
            o = ((oy + y) * w + ox) * 3
            px[o:o + 720] = data[y * 720:(y + 1) * 720]
    raw = b''.join(b'\0' + bytes(px[y * w * 3:(y + 1) * w * 3]) for y in range(h))

    def chunk(t, b):
        return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b))
    (shots / 'sheet.png').write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                                      chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--zig', type=Path, default=root.parent / 'Spartan/build/host-tools/ziglang/zig.exe')
    p.add_argument('--lvgl', type=Path, default=root / 'managed_components/lvgl__lvgl')
    a = p.parse_args()
    lvgl = a.lvgl if (a.lvgl / 'lvgl.h').exists() else root.parent / 'Spartan/managed_components/lvgl__lvgl'
    if not (lvgl / 'lvgl.h').exists():
        sys.exit('LVGL not found: run an ESP-IDF build once or pass --lvgl')
    target = 'x86_64-windows-gnu' if sys.platform == 'win32' else 'native'
    exe = '.exe' if sys.platform == 'win32' else ''
    out = root / 'build/host'
    zig_cc(a.zig, out, ['-std=c11', '-O1', '-Wall', '-Wextra', '-Werror', '-UNDEBUG', '-Icore', '-target', target,
                        'core/digger.c', 'tests/core_tests.c', '-o', str(out / f'core_tests{exe}')])
    subprocess.run([str(out / f'core_tests{exe}')], check=True)
    version = (root / 'version.txt').read_text(encoding='utf-8').strip()
    ui = sorted(str(f.relative_to(root)) for f in (root / 'platforms/passport/main/ui').glob('*.c'))
    zig_cc(a.zig, out, ['-std=c11', '-O1', '-Wall', '-UNDEBUG', '-DLV_CONF_INCLUDE_SIMPLE', f'-DGD_VERSION="{version}"',
                        '-Iplatforms/common/include', '-Iplatforms/passport/main', '-Icore', f'-I{lvgl}',
                        '-target', target, 'platforms/common/tests/host_smoke.c', 'platforms/common/src/gd_host.c',
                        'platforms/common/src/host_audio.c', 'platforms/passport/main/gd_synth.c', 'core/digger.c'] +
           ui + [str(s) for s in sorted((lvgl / 'src').rglob('*.c'))] + ['-o', str(out / f'host_smoke{exe}')])
    shots = root / 'build/shots'
    shots.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(out / f'host_smoke{exe}'), str(shots)], check=True, timeout=300)
    contact_sheet(shots)
    print('Screenshots:', shots / 'sheet.png')


if __name__ == '__main__':
    main()
