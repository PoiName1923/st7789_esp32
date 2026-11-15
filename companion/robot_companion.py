"""Local USB control bridge for Desktop Robot (development runner)."""

import argparse
from collections import deque
import json
import os
import re
import secrets
import sys
import threading
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit
from pathlib import Path

from serial import Serial, SerialException
from serial.tools import list_ports
from firmware_manager import FirmwareManager
from gif_import import SLOTS, upload_gif
from page import PAGE as CONTROL_PAGE


class EventLog:
    """Short-lived, safe diagnostics for the local control session."""

    def __init__(self, limit=200):
        self.events = deque(maxlen=limit)
        self.lock = threading.Lock()

    def add(self, source, action, outcome, detail=""):
        detail = re.sub(r"(?i)(password|token)=[^\s,;]+", r"\1=[hidden]", str(detail))
        event = {"time": __import__("time").strftime("%H:%M:%S"), "source": source,
                 "action": action, "outcome": outcome, "detail": detail[:240]}
        with self.lock:
            self.events.append(event)

    def snapshot(self):
        with self.lock:
            return list(self.events)


class RobotSerial:
    def __init__(self, port=None):
        self.port = port
        self.device = None
        self.chip_id = None
        self.lock = threading.RLock()
        self.next_id = 0

    def close(self):
        if self.device:
            self.device.close()
            self.device = None

    def request(self, op, **params):
        with self.lock:
            if not self.device:
                self.connect()
            self.next_id += 1
            request_id = self.next_id
            payload = json.dumps({"id": request_id, "op": op, **params}, separators=(",", ":"))
            if len(payload.encode("utf-8")) > 512:
                raise ValueError("Lệnh quá dài")
            try:
                self.device.write(payload.encode("utf-8") + b"\n")
                for _ in range(20):
                    line = self.device.readline()
                    if not line:
                        continue
                    try:
                        result = json.loads(line)
                    except (UnicodeDecodeError, ValueError):
                        continue  # boot log from a legacy firmware
                    if isinstance(result, dict) and result.get("id") == request_id:
                        return result
            except SerialException as exc:
                self.close()
                raise ConnectionError("Mất kết nối USB") from exc
            self.close()
            raise TimeoutError("Bot không trả lời")

    def connect(self):
        with self.lock:
            self.close()
            ports = ([self.port] if self.port else []) + [p.device for p in list_ports.comports() if p.device != self.port]
            for port in ports:
                try:
                    candidate = Serial(port, 115200, timeout=0.15, write_timeout=2)
                    self.device = candidate
                    hello = self.request("hello")
                    found_id = hello.get("chip_id")
                    if hello.get("device") == "desktop-robot" and (self.chip_id is None or found_id == self.chip_id):
                        self.port = port
                        self.chip_id = found_id
                        return port
                    self.close()
                except (OSError, SerialException, ConnectionError, TimeoutError, ValueError):
                    self.close()
        raise ConnectionError("Không tìm thấy bot qua USB")


def create_server(robot, firmware=None, port=0, open_browser=False):
    token = secrets.token_urlsafe(32)
    firmware_lock = threading.Lock()
    events = EventLog()

    class Handler(BaseHTTPRequestHandler):
        def respond(self, status, data):
            body = json.dumps(data).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def authorized(self):
            return self.headers.get("X-Robot-Token") == token

        def event(self, action, outcome, detail=""):
            events.add("bridge", action, outcome, detail)

        def do_GET(self):
            url = urlsplit(self.path)
            if url.path == "/" and parse_qs(url.query).get("token") == [token]:
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Cache-Control", "no-store")
                body = CONTROL_PAGE.replace("__TOKEN__", token).replace("__FIRMWARE_AVAILABLE__", "true" if firmware else "false").encode("utf-8")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
                return
            if not self.authorized():
                return self.respond(403, {"error": "Truy cập bị từ chối"})
            if url.path == "/api/log":
                return self.respond(200, {"events": events.snapshot()})
            if url.path == "/api/status":
                return self.execute("get_status")
            if url.path == "/api/settings":
                return self.execute("get_settings")
            if url.path == "/api/gif":
                slot = parse_qs(url.query).get("slot", [""])[0]
                if slot.isdecimal() and int(slot) < len(SLOTS):
                    return self.execute("gif_status", slot=int(slot))
                return self.respond(400, {"error": "Ô GIF không hợp lệ"})
            self.respond(404, {"error": "Không tìm thấy"})

        def do_POST(self):
            if not self.authorized():
                return self.respond(403, {"error": "Truy cập bị từ chối"})
            url = urlsplit(self.path)
            if url.path == "/api/gif/upload":
                slot = parse_qs(url.query).get("slot", [""])[0]
                size = self.headers.get("Content-Length", "")
                if not slot.isdecimal() or int(slot) >= len(SLOTS) or not size.isdecimal() or int(size) > 8 * 1024 * 1024:
                    return self.respond(413, {"error": "GIF quá lớn hoặc ô không hợp lệ"})
                try:
                    self.event("gif upload", "started", f"slot {slot}; received {size} bytes")
                    result = upload_gif(robot, int(slot), self.rfile.read(int(size)))
                    self.event("gif upload", "ok", f"{result['slot']}; {result['bytes']} bytes")
                    return self.respond(200, result)
                except (ValueError, RuntimeError) as exc:
                    self.event("gif upload", "error", exc)
                    return self.respond(400, {"error": str(exc)})
                except (ConnectionError, TimeoutError, SerialException) as exc:
                    self.event("gif upload", "error", exc)
                    return self.respond(503, {"error": str(exc)})
            size = self.headers.get("Content-Length", "")
            if not size.isdecimal() or int(size) > 512:
                return self.respond(413, {"error": "Lệnh quá dài"})
            try:
                data = json.loads(self.rfile.read(int(size)))
                if not isinstance(data, dict):
                    raise ValueError()
            except ValueError:
                return self.respond(400, {"error": "JSON không hợp lệ"})
            if self.path == "/api/connect":
                try:
                    found = robot.connect()
                    self.event("connect", "ok", found)
                    return self.respond(200, {"ok": True, "port": found})
                except ConnectionError as exc:
                    self.event("connect", "error", exc)
                    return self.respond(503, {"error": str(exc)})
            if self.path in ("/api/firmware/build", "/api/firmware/flash"):
                if not firmware:
                    return self.respond(503, {"error": "Chưa có bộ công cụ build đi kèm"})
                if not firmware_lock.acquire(blocking=False):
                    return self.respond(409, {"error": "Đang build hoặc nạp firmware"})
                try:
                    if self.path.endswith("/build"):
                        firmware.build()
                        return self.respond(200, {"ok": True, "message": "Build thành công"})
                    if not robot.port:
                        raise ConnectionError("Chưa xác định cổng USB của bot")
                    with robot.lock:
                        port_name = robot.port
                        robot.close()
                        backup_dir = Path(os.getenv("LOCALAPPDATA", str(Path.home()))) / "DesktopRobot" / "backups"
                        backup = backup_dir / f"{robot.chip_id or port_name}-{secrets.token_hex(8)}.bin"
                        digest = firmware.flash(port_name, backup)
                        try:
                            robot.connect()
                        except ConnectionError:
                            pass
                    return self.respond(200, {"ok": True, "backup": str(backup), "sha256": digest})
                except (ConnectionError, FileNotFoundError, RuntimeError, ValueError) as exc:
                    return self.respond(503, {"error": str(exc)})
                finally:
                    firmware_lock.release()
            if self.path == "/api/screen" and type(data.get("screen")) is int and data["screen"] in (1, 2, 3):
                return self.execute("show_screen", screen=data["screen"])
            if self.path == "/api/animation" and type(data.get("animation")) is int and data["animation"] in range(len(SLOTS)):
                return self.execute("show_animation", animation=data["animation"])
            if self.path == "/api/gif/delete" and type(data.get("slot")) is int and data["slot"] in range(len(SLOTS)):
                return self.execute("gif_delete", slot=data["slot"])
            if self.path == "/api/wifi" and isinstance(data.get("ssid"), str) and isinstance(data.get("password"), str):
                params = {"ssid": data["ssid"]}
                if data.get("open_network") is True:
                    params["password"] = ""
                elif data["password"]:
                    params["password"] = data["password"]
                return self.execute("set_wifi", **params)
            if self.path == "/api/weather" and all(isinstance(data.get(key), str) for key in ("latitude", "longitude", "timezone")) and type(data.get("interval")) is int:
                return self.execute("set_weather", **data)
            self.respond(400, {"error": "Lệnh không hợp lệ"})

        def execute(self, op, **params):
            try:
                result = robot.request(op, **params)
                self.event(op, "ok" if result.get("ok") else "error", result.get("error", ""))
                self.respond(200 if result.get("ok") else 400, result)
            except (ConnectionError, TimeoutError, ValueError, SerialException) as exc:
                self.event(op, "error", exc)
                self.respond(503, {"error": str(exc)})

        def log_message(self, fmt, *args):
            pass

    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    address = f"http://127.0.0.1:{server.server_port}/?token={token}"
    if open_browser:
        webbrowser.open(address)
    return server, address


def run_server(robot, firmware=None, port=0, open_browser=True):
    server, address = create_server(robot, firmware, port, open_browser)
    print(f"Điều khiển bot: {address}")
    try:
        server.serve_forever()
    finally:
        server.server_close()


PAGE = """<!doctype html><html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Desktop Robot USB</title><style>body{font:17px system-ui;max-width:640px;margin:2rem auto;padding:0 1rem;background:#0b1016;color:#e8eef2}button{padding:1rem;margin:.3rem;border:0;border-radius:.6rem;background:#2ed8ff;color:#05121a;font-weight:bold}select{padding:.6rem}p{line-height:1.5}</style>
<h1>Desktop Robot</h1><p id="status">Đang đọc trạng thái…</p><button onclick="send('/api/connect',{})">Kết nối lại USB</button><h2>Màn hình</h2>
<button onclick="send('/api/screen',{screen:1})">Khuôn mặt</button><button onclick="send('/api/screen',{screen:2})">Thời tiết</button><button onclick="send('/api/screen',{screen:3})">Cảm biến</button>
<h2>Biểu cảm</h2><select id="face"><option value="0">Idle</option><option value="1">Blink</option><option value="2">Happy</option><option value="3">Sleepy</option><option value="4">Surprised</option><option value="5">Look left</option><option value="6">Look right</option><option value="7">Sunny</option><option value="8">Rain</option></select><button onclick="send('/api/animation',{animation:+document.querySelector('#face').value})">Hiển thị</button>
<h2>Nhập GIF</h2><p>Chọn một ô biểu cảm, rồi nhập GIF. Bot sẽ dùng GIF đó cho ô này.</p><input id="gifFile" type="file" accept="image/gif"><button onclick="upload()">Nhập và nạp GIF</button><button onclick="if(confirm('Xóa GIF ở ô đang chọn?'))send('/api/gif/delete',{slot:+document.querySelector('#face').value})">Xóa GIF</button>
<h2>Wi-Fi thời tiết</h2><input id="ssid" placeholder="Tên Wi-Fi"><input id="password" type="password" placeholder="Mật khẩu"><label><input id="openNetwork" type="checkbox"> Mạng mở, không mật khẩu</label><button onclick="send('/api/wifi',{ssid:document.querySelector('#ssid').value,password:document.querySelector('#password').value,open_network:document.querySelector('#openNetwork').checked})">Lưu Wi-Fi</button>
<h2>Vị trí thời tiết</h2><input id="latitude" placeholder="Vĩ độ"><input id="longitude" placeholder="Kinh độ"><input id="timezone" placeholder="Múi giờ"><input id="interval" type="number" min="5" max="180" value="15"><button onclick="send('/api/weather',{latitude:document.querySelector('#latitude').value,longitude:document.querySelector('#longitude').value,timezone:document.querySelector('#timezone').value,interval:+document.querySelector('#interval').value})">Lưu thời tiết</button>
<section id="firmware"><h2>Firmware</h2><p>Trước khi nạp, ứng dụng sao lưu toàn bộ phân vùng GIF của bot. Giữ cáp USB trong suốt quá trình.</p><button onclick="send('/api/firmware/build',{})">Build firmware</button><button onclick="if(confirm('Build và nạp firmware qua USB?'))send('/api/firmware/flash',{})">Build và nạp</button></section>
<script>for(let i=0;i<32;i++){const o=document.createElement('option');o.value=i+9;o.textContent=`GIF ${String(i+1).padStart(2,'0')}`;document.querySelector('#face').append(o)}</script>
<p id="message"></p><script>const token='__TOKEN__';if(!__FIRMWARE_AVAILABLE__)document.querySelector('#firmware').textContent='Chưa có bộ công cụ build đi kèm.';async function call(path,data){const r=await fetch(path,{method:data?'POST':'GET',headers:{'X-Robot-Token':token,'Content-Type':'application/json'},body:data?JSON.stringify(data):undefined});const j=await r.json();if(!r.ok)throw Error(j.error);return j}async function send(path,data){try{await call(path,data);document.querySelector('#message').textContent='Đã gửi lệnh';await refresh()}catch(e){document.querySelector('#message').textContent=e.message}}async function upload(){const file=document.querySelector('#gifFile').files[0];if(!file){document.querySelector('#message').textContent='Hãy chọn GIF';return}document.querySelector('#message').textContent='Đang chuyển đổi và nạp GIF…';try{const slot=+document.querySelector('#face').value;const r=await fetch('/api/gif/upload?slot='+slot,{method:'POST',headers:{'X-Robot-Token':token},body:file});const j=await r.json();if(!r.ok)throw Error(j.error);document.querySelector('#message').textContent=`Đã nạp ${j.slot} (${j.bytes} byte)`}catch(e){document.querySelector('#message').textContent=e.message}}async function refresh(){try{const s=await call('/api/status');document.querySelector('#status').textContent=`Màn hình ${s.screen}; biểu cảm ${s.animation}; Wi-Fi ${s.network}; nhiệt độ ${s.temperature??'—'} °C; độ ẩm ${s.humidity??'—'} %`}catch(e){document.querySelector('#status').textContent=e.message}}async function load(){try{const s=await call('/api/settings');for(const id of ['ssid','latitude','longitude','timezone'])document.getElementById(id).value=s[id];document.getElementById('interval').value=s.weather_interval}catch(e){document.querySelector('#message').textContent=e.message}}refresh();load();setInterval(refresh,5000)</script></html>"""


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Desktop Robot USB companion")
    parser.add_argument("--port", help="COM port, nếu không có sẽ tự tìm")
    parser.add_argument("--project", help="Thư mục mã nguồn firmware")
    parser.add_argument("--idf-path", help="Thư mục ESP-IDF 5.5")
    parser.add_argument("--python", help="Python của bộ công cụ ESP-IDF")
    args = parser.parse_args()
    robot = RobotSerial(args.port)
    try:
        print("Đã kết nối:", robot.connect())
    except ConnectionError as exc:
        print(exc, "— mở trang điều khiển rồi bấm Kết nối lại USB")
    base = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parents[1]
    project = Path(args.project) if args.project else base / "firmware"
    idf_path = Path(args.idf_path) if args.idf_path else base / "toolchain" / "esp-idf"
    python = Path(args.python) if args.python else base / "toolchain" / "python" / "Scripts" / "python.exe"
    manager = FirmwareManager(project, python, idf_path) if project.is_dir() and idf_path.is_dir() and python.is_file() else None
    run_server(robot, manager)
