import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

from firmware_manager import FirmwareManager, STORAGE_SIZE


class FirmwareTests(unittest.TestCase):
    def test_incomplete_backup_stops_before_flash(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp)
            (project / "CMakeLists.txt").write_text("project(robot)")
            (project / "idf").mkdir()
            (project / "idf" / "tools").mkdir()
            (project / "idf" / "tools" / "idf.py").write_text("# fake")
            calls = []

            def runner(args, **kwargs):
                calls.append(args)
                return SimpleNamespace(returncode=0, stdout="ok")

            manager = FirmwareManager(project, project / "python.exe", project / "idf", runner)
            with self.assertRaisesRegex(RuntimeError, "Sao lưu"):
                manager.flash("COM5", project / "backup.bin")
            self.assertEqual(len(calls), 2)
            self.assertIn("read_flash", calls[-1])

    def test_rejects_untrusted_port_before_tool_invocation(self):
        with tempfile.TemporaryDirectory() as temp:
            manager = FirmwareManager(temp, Path(temp) / "python.exe", Path(temp) / "idf",
                                      lambda *a, **kw: self.fail("tool invoked"))
            with self.assertRaises(ValueError):
                manager.backup_storage("COM5; erase_flash", Path(temp) / "backup.bin")

    def test_flash_after_backup_and_verifies_restored_storage(self):
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp)
            (project / "CMakeLists.txt").write_text("project(robot)")
            (project / "idf").mkdir()
            (project / "idf" / "tools").mkdir()
            (project / "idf" / "tools" / "idf.py").write_text("# fake")
            calls = []

            def runner(args, **kwargs):
                calls.append(args)
                if "read_flash" in args:
                    Path(args[-1]).write_bytes(b"A" * STORAGE_SIZE)
                return SimpleNamespace(returncode=0, stdout="ok")

            manager = FirmwareManager(project, project / "python.exe", project / "idf", runner)
            manager.flash("COM5", project / "backup.bin")
            self.assertEqual(len([args for args in calls if "read_flash" in args]), 3)
            self.assertEqual(len([args for args in calls if "write_flash" in args]), 1)
            self.assertEqual((project / "backup.bin").stat().st_size, STORAGE_SIZE)

    def test_portable_python_home_tracks_distribution_location(self):
        with tempfile.TemporaryDirectory() as temp:
            python_root = Path(temp) / "python"
            (python_root / "Scripts").mkdir(parents=True)
            config = python_root / "pyvenv.cfg"
            config.write_text("home = C:\\old\\path\nportable-desktop-robot = true\n")
            manager = FirmwareManager(temp, python_root / "Scripts" / "python.exe", Path(temp) / "idf")
            manager._ensure_python_home()
            self.assertIn(f"home = {manager.python.parent.parent}", config.read_text())


if __name__ == "__main__":
    unittest.main()
