from io import BytesIO
import unittest

from PIL import Image
from gif_import import SLOTS, normalize_gif, upload_gif


class FakeRobot:
    def __init__(self):
        self.calls = []

    def request(self, op, **params):
        self.calls.append((op, params))
        return {"ok": True}


class GifImportTests(unittest.TestCase):
    def test_has_32_extra_gif_slots(self):
        self.assertEqual(len(SLOTS), 41)
        self.assertEqual(SLOTS[9], "user_00")
        self.assertEqual(SLOTS[-1], "user_31")

    def test_normalizes_and_uploads_without_build(self):
        source = BytesIO()
        Image.new("RGB", (40, 20), "red").save(source, format="GIF")
        result = normalize_gif(source.getvalue())
        self.assertEqual(Image.open(BytesIO(result)).size, (240, 240))
        robot = FakeRobot()
        upload_gif(robot, 2, source.getvalue())
        self.assertEqual(robot.calls[0][0], "gif_begin")
        self.assertIn("crc32", robot.calls[0][1])
        self.assertEqual(robot.calls[-1][0], "gif_finish")

    def test_rejects_non_gif(self):
        with self.assertRaises(ValueError):
            normalize_gif(b"not a gif")


if __name__ == "__main__":
    unittest.main()
