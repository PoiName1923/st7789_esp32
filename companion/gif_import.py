"""Normalize GIF files for the 240x240 LVGL decoder."""

import base64
import zlib
from io import BytesIO

from PIL import Image, ImageSequence, UnidentifiedImageError


SLOTS = ("robot_idle", "robot_blink", "robot_happy", "robot_sleepy", "robot_surprised",
         "robot_look_left", "robot_look_right", "robot_weather_sunny", "robot_weather_rain") + tuple(
             f"user_{i:02d}" for i in range(32))
MAX_GIF = 2 * 1024 * 1024


def normalize_gif(source):
    try:
        original = Image.open(BytesIO(source))
        if original.format != "GIF":
            raise ValueError("Tệp không phải GIF")
        if original.width > 2048 or original.height > 2048:
            raise ValueError("GIF nguồn vượt quá 2048×2048")
        frames, durations = [], []
        for frame in ImageSequence.Iterator(original):
            if len(frames) >= 500:
                raise ValueError("GIF có quá nhiều khung hình")
            rgba = frame.convert("RGBA")
            rgba.thumbnail((240, 240), Image.Resampling.LANCZOS)
            canvas = Image.new("RGBA", (240, 240), "black")
            canvas.alpha_composite(rgba, ((240 - rgba.width) // 2, (240 - rgba.height) // 2))
            frames.append(canvas.convert("RGB").quantize(colors=64, method=Image.Quantize.MEDIANCUT))
            durations.append(max(50, frame.info.get("duration", 100) or 100))
        if not frames:
            raise ValueError("GIF không có khung hình")
        output = BytesIO()
        frames[0].save(output, format="GIF", save_all=True, append_images=frames[1:],
                       duration=durations, loop=0, optimize=True, disposal=1)
        result = output.getvalue()
        if len(result) > MAX_GIF:
            raise ValueError("GIF sau chuyển đổi vượt quá 2 MB")
        return result
    except (UnidentifiedImageError, OSError, Image.DecompressionBombError) as exc:
        raise ValueError("Không đọc được GIF") from exc


def upload_gif(robot, slot, data):
    if type(slot) is not int or not 0 <= slot < len(SLOTS):
        raise ValueError("Ô GIF không hợp lệ")
    converted = normalize_gif(data)
    def checked(op, **params):
        result = robot.request(op, **params)
        if not result.get("ok"):
            raise RuntimeError(result.get("error", "Bot từ chối GIF"))
    checked("gif_begin", slot=slot, size=len(converted), crc32=zlib.crc32(converted) & 0xffffffff)
    for start in range(0, len(converted), 192):
        checked("gif_chunk", data=base64.b64encode(converted[start:start + 192]).decode("ascii"))
    checked("gif_finish")
    return {"ok": True, "slot": SLOTS[slot], "bytes": len(converted)}
