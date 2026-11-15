import json
import unittest
from html.parser import HTMLParser
from io import BytesIO
from threading import Thread
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from PIL import Image
from page import PAGE
from robot_companion import EventLog, RobotSerial, create_server
from serial import SerialException


class FakeSerial:
    def __init__(self, lines):
        self.lines = list(lines)
        self.sent = None
        self.closed = False

    def write(self, data):
        self.sent = json.loads(data)

    def readline(self):
        return self.lines.pop(0) if self.lines else b""

    def close(self):
        self.closed = True


class RobotSerialTests(unittest.TestCase):
    def test_matches_request_id_while_ignoring_boot_log(self):
        robot = RobotSerial()
        robot.device = FakeSerial([b"boot log\n", b'{"id":99,"ok":true}\n', b'{"id":1,"ok":true}\n'])
        self.assertTrue(robot.request("hello")["ok"])
        self.assertEqual(robot.device.sent, {"id": 1, "op": "hello"})

    def test_times_out_on_missing_reply(self):
        robot = RobotSerial()
        serial = FakeSerial([])
        robot.device = serial
        with self.assertRaises(TimeoutError):
            robot.request("hello")
        self.assertTrue(serial.closed)
        self.assertIsNone(robot.device)

    def test_request_reconnects_when_device_was_disconnected(self):
        robot = RobotSerial()
        serial = FakeSerial([b'{"id":1,"ok":true,"screen":1}\n'])

        def reconnect():
            robot.device = serial
            return "COM8"

        robot.connect = reconnect
        result = robot.request("get_status")

        self.assertEqual(result["screen"], 1)


class EventLogTests(unittest.TestCase):
    def test_keeps_newest_events_with_safe_details(self):
        events = EventLog(limit=2)
        events.add("server", "set_wifi", "ok", "ssid=Lab password=secret-token")
        events.add("server", "status", "ok", "connected")
        events.add("server", "gif", "error", "USB timeout")

        snapshot = events.snapshot()

        self.assertEqual([event["action"] for event in snapshot], ["status", "gif"])
        self.assertNotIn("secret-token", json.dumps(snapshot))


class UploadRobot:
    def request(self, op, **params):
        if op == "gif_begin":
            raise SerialException("port lost")
        return {"ok": True}


class BridgeApiTests(unittest.TestCase):
    def setUp(self):
        self.server, self.address = create_server(UploadRobot(), open_browser=False)
        self.base_url, self.token = self.address.split("/?token=", 1)
        self.thread = Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def request(self, path, data=None, content_type="application/json"):
        request = Request(self.base_url + path, data=data, method="POST" if data is not None else "GET")
        request.add_header("X-Robot-Token", self.token)
        if data is not None:
            request.add_header("Content-Type", content_type)
        try:
            with urlopen(request) as response:
                return response.status, json.loads(response.read())
        except HTTPError as error:
            return error.code, json.loads(error.read())

    def test_gif_serial_error_returns_json_and_is_logged(self):
        image = BytesIO()
        Image.new("RGB", (8, 8), "red").save(image, format="GIF")

        status, body = self.request("/api/gif/upload?slot=9", image.getvalue(), "image/gif")
        _, logs = self.request("/api/log")

        self.assertEqual(status, 503)
        self.assertIn("port lost", body["error"])
        self.assertTrue(any(event["action"] == "gif upload" and event["outcome"] == "error" for event in logs["events"]))


class TabParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.tabs = []
        self.panels = []

    def handle_starttag(self, tag, attrs):
        attributes = dict(attrs)
        if attributes.get("role") == "tab":
            self.tabs.append(attributes)
        if attributes.get("role") == "tabpanel":
            self.panels.append(attributes)


class PageMarkupTests(unittest.TestCase):
    def test_tabs_have_matching_accessible_panels(self):
        parser = TabParser()
        parser.feed(PAGE)

        self.assertEqual([tab["id"] for tab in parser.tabs], [
            "tab-overview", "tab-display", "tab-gifs", "tab-weather", "tab-firmware",
        ])
        self.assertEqual({panel["id"] for panel in parser.panels}, {
            "panel-overview", "panel-display", "panel-gifs", "panel-weather", "panel-firmware",
        })
        self.assertEqual([tab["aria-selected"] for tab in parser.tabs], ["true", "false", "false", "false", "false"])
        self.assertEqual({tab["aria-controls"] for tab in parser.tabs}, {panel["id"] for panel in parser.panels})

    def test_page_declares_mobile_and_reduced_motion_rules(self):
        self.assertIn("@media(max-width:600px)", PAGE)
        self.assertIn("overflow-x:auto", PAGE)
        self.assertIn("@media(prefers-reduced-motion:reduce)", PAGE)
        self.assertIn('aria-live="polite"', PAGE)

    def test_action_buttons_use_shared_feedback_contract(self):
        self.assertIn("async function runAction(button,pendingLabel,action)", PAGE)
        self.assertIn("button.disabled=true", PAGE)
        self.assertIn("button.setAttribute('aria-busy','true')", PAGE)
        self.assertIn("button.setAttribute('aria-busy','false')", PAGE)
        self.assertIn("button.classList.replace('is-pending','is-success')", PAGE)
        self.assertIn("button.classList.replace('is-pending','is-error')", PAGE)
        self.assertIn("onclick=\"send('/api/screen',{screen:1},this)\"", PAGE)

    def test_page_describes_expression_and_gif_batches(self):
        self.assertIn("Biểu cảm liên tục", PAGE)
        self.assertIn("GIF đã chọn sẽ lặp liên tục", PAGE)

    def test_page_keeps_command_success_independent_from_refresh(self):
        self.assertIn("void refresh().catch", PAGE)
        self.assertIn('id="clientLog"', PAGE)
        self.assertIn('id="serverLog"', PAGE)
        self.assertIn("recordClientEvent('gif upload'", PAGE)


if __name__ == "__main__":
    unittest.main()
