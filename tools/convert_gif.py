#!/usr/bin/env python3
"""
Import GIFs for the desktop robot: fit to the display, cap FPS, reduce colours,
and write them to assets/gifs/<name>.gif where the firmware picks them up.

    pip install pillow
    python tools/convert_gif.py my_blink.gif --name robot_blink
    python tools/convert_gif.py idle.gif --name robot_idle --fps 20 --colors 64
    python tools/convert_gif.py --check          # validate what's in assets/gifs

After adding/removing files: `idf.py build` (or `idf.py reconfigure` if the new file
isn't picked up). Only use GIFs you have the right to redistribute.
"""
import argparse
import sys
from pathlib import Path

NAMES = ["robot_idle", "robot_blink", "robot_happy", "robot_sleepy", "robot_surprised",
         "robot_look_left", "robot_look_right", "robot_weather_sunny", "robot_weather_rain"]
OUT_DIR = Path(__file__).resolve().parent.parent / "assets" / "gifs"


def need_pillow():
    try:
        from PIL import Image, ImageSequence  # noqa: F401
    except ImportError:
        sys.exit("Pillow is required: pip install pillow")


def convert(src: Path, name: str, size: int, fps: int, colors: int, bg: str) -> Path:
    from PIL import Image, ImageSequence

    im = Image.open(src)
    min_ms = 1000 // fps
    frames, durations, acc = [], [], 0
    for frame in ImageSequence.Iterator(im):
        dur = frame.info.get("duration", im.info.get("duration", 100)) or 100
        acc += dur
        if frames and acc < min_ms:          # drop frames faster than the FPS cap,
            durations[-1] += dur             # keep total timing
            continue
        rgba = frame.convert("RGBA")
        rgba.thumbnail((size, size), Image.LANCZOS)
        canvas = Image.new("RGBA", (size, size), bg)
        canvas.alpha_composite(rgba, ((size - rgba.width) // 2, (size - rgba.height) // 2))
        frames.append(canvas.convert("RGB").quantize(colors=colors, method=Image.Quantize.MEDIANCUT))
        durations.append(dur)
        acc = 0

    if not frames:
        raise ValueError(f"{src}: no frames")
    out = OUT_DIR / f"{name}.gif"
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=durations,
                   loop=0, optimize=True, disposal=1)
    print(f"{src.name} -> {out.relative_to(OUT_DIR.parent.parent)}: {len(frames)} frames, "
          f"{out.stat().st_size / 1024:.0f} KB flash, ~{size * size * 3 / 1024:.0f} KB PSRAM when loaded")
    return out


def check(size: int) -> int:
    from PIL import Image
    bad = 0
    for name in NAMES:
        p = OUT_DIR / f"{name}.gif"
        if not p.exists():
            print(f"  {name:22s} missing  -> built-in drawn face")
            continue
        try:
            im = Image.open(p)
            n = getattr(im, "n_frames", 1)
            ok = im.format == "GIF" and im.size == (size, size)
            print(f"  {name:22s} {im.size[0]}x{im.size[1]} {n:3d} frames {p.stat().st_size / 1024:6.0f} KB"
                  f"{'' if ok else '  <- re-run convert (size/format)'}")
            bad += not ok
        except Exception as e:  # corrupt file: firmware will fall back, but flag it here
            print(f"  {name:22s} UNREADABLE ({e})")
            bad += 1
    for extra in sorted(set(p.stem for p in OUT_DIR.glob("*.gif")) - set(NAMES)):
        print(f"  {extra:22s} ignored (unknown name; valid: {', '.join(NAMES)})")
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src", nargs="?", type=Path, help="input GIF")
    ap.add_argument("--name", choices=NAMES, help="animation slot (default: input file name)")
    ap.add_argument("--size", type=int, default=240, help="display size in px (default 240)")
    ap.add_argument("--fps", type=int, default=20, help="max frame rate (default 20)")
    ap.add_argument("--colors", type=int, default=64, help="palette size 2-256 (default 64)")
    ap.add_argument("--bg", default="#000000", help="background for transparent/padded areas")
    ap.add_argument("--check", action="store_true", help="validate assets/gifs and exit")
    a = ap.parse_args()
    need_pillow()

    if a.check or not a.src:
        print(f"assets in {OUT_DIR}:")
        sys.exit(1 if check(a.size) else 0)
    name = a.name or a.src.stem.lower()
    if name not in NAMES:
        sys.exit(f"unknown slot '{name}'; use --name with one of: {', '.join(NAMES)}")
    if not 2 <= a.colors <= 256 or a.fps <= 0:
        sys.exit("--colors must be 2..256 and --fps > 0")
    convert(a.src, name, a.size, a.fps, a.colors, a.bg)


if __name__ == "__main__":
    main()
