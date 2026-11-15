"""Build and flash orchestration. Never flash before a verified storage backup."""

import hashlib
import os
import re
import subprocess
from pathlib import Path


STORAGE_OFFSET = "0x710000"
STORAGE_SIZE = 0x8F0000


class FirmwareManager:
    def __init__(self, project, python, idf_path, runner=subprocess.run):
        self.project = Path(project).resolve()
        self.python = Path(python).resolve()
        self.idf_path = Path(idf_path).resolve()
        self.runner = runner

    def _run(self, args, *, env=None):
        result = self.runner(args, cwd=self.project, env=env, check=False,
                             text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(result.stdout[-3000:] if result.stdout else "Công cụ thất bại")
        return result.stdout

    def _idf_env(self):
        self._ensure_python_home()
        env = os.environ.copy()
        env["IDF_PATH"] = str(self.idf_path)
        env["IDF_PYTHON_ENV_PATH"] = str(self.python.parent.parent)
        bundled_tools = self.idf_path.parent
        if (bundled_tools / "tools").is_dir():
            env["IDF_TOOLS_PATH"] = str(bundled_tools)
            env["ESP_ROM_ELF_DIR"] = str(bundled_tools / "tools" / "esp-rom-elfs" / "20260528")
            env["GIT_CEILING_DIRECTORIES"] = str(self.project.parent)
            bins = [bundled_tools / "tools" / "xtensa-esp-elf" / "esp-14.2.0_20260121" / "xtensa-esp-elf" / "bin",
                    bundled_tools / "tools" / "cmake" / "3.30.2" / "bin",
                    bundled_tools / "tools" / "ninja" / "1.12.1",
                    bundled_tools / "tools" / "git" / "bin"]
            env["PATH"] = os.pathsep.join(map(str, bins)) + os.pathsep + env.get("PATH", "")
        env["CCACHE_DISABLE"] = "1"
        return env

    def _ensure_python_home(self):
        root = self.python.parent.parent
        config = root / "pyvenv.cfg"
        if config.is_file():
            text = config.read_text(encoding="utf-8-sig")
            if "portable-desktop-robot = true" not in text:
                return
            lines = [f"home = {root}" if line.startswith("home = ") else line for line in text.splitlines()]
            updated = "\n".join(lines) + "\n"
            if updated != text:
                config.write_text(updated, encoding="utf-8")

    def _idf(self, *args):
        script = self.idf_path / "tools" / "idf.py"
        if not script.is_file():
            raise FileNotFoundError("Thiếu ESP-IDF portable")
        return self._run([str(self.python), str(script), *args], env=self._idf_env())

    def build(self):
        if not (self.project / "CMakeLists.txt").is_file():
            raise FileNotFoundError("Thiếu mã nguồn firmware")
        return self._idf("build")

    def backup_storage(self, port, path):
        self._ensure_python_home()
        if not re.fullmatch(r"COM\d+", port, re.IGNORECASE):
            raise ValueError("Cổng COM không hợp lệ")
        target = Path(path).resolve()
        target.parent.mkdir(parents=True, exist_ok=True)
        self._run([str(self.python), "-m", "esptool", "--chip", "esp32s3", "-p", port,
                   "read_flash", STORAGE_OFFSET, hex(STORAGE_SIZE), str(target)])
        if not target.is_file() or target.stat().st_size != STORAGE_SIZE:
            raise RuntimeError("Sao lưu thư viện GIF không đầy đủ; đã dừng trước khi nạp")
        digest = hashlib.sha256(target.read_bytes()).hexdigest()
        second = target.with_suffix(".verify.bin")
        try:
            self._run([str(self.python), "-m", "esptool", "--chip", "esp32s3", "-p", port,
                       "read_flash", STORAGE_OFFSET, hex(STORAGE_SIZE), str(second)])
            if not second.is_file() or second.stat().st_size != STORAGE_SIZE or \
                    hashlib.sha256(second.read_bytes()).hexdigest() != digest:
                raise RuntimeError("Hai lần đọc thư viện GIF không khớp; đã dừng trước khi nạp")
        finally:
            second.unlink(missing_ok=True)
        return digest

    def flash(self, port, backup):
        """Build, back up bot storage, flash app, then restore the exact storage image."""
        self.build()
        digest = self.backup_storage(port, backup)
        self._idf("-p", port, "flash")
        self._run([str(self.python), "-m", "esptool", "--chip", "esp32s3", "-p", port,
                   "write_flash", STORAGE_OFFSET, str(Path(backup).resolve())])
        restored = Path(backup).resolve().with_suffix(".restored.bin")
        try:
            self._run([str(self.python), "-m", "esptool", "--chip", "esp32s3", "-p", port,
                       "read_flash", STORAGE_OFFSET, hex(STORAGE_SIZE), str(restored)])
            if not restored.is_file() or restored.stat().st_size != STORAGE_SIZE or \
                    hashlib.sha256(restored.read_bytes()).hexdigest() != digest:
                raise RuntimeError("Nạp firmware xong nhưng kiểm tra thư viện GIF thất bại; bản sao vẫn được giữ")
        finally:
            restored.unlink(missing_ok=True)
        return digest
