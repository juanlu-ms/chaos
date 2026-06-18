import http.server
import socketserver
import json
import datetime
import time
import os
from urllib.parse import urlparse

PORT = int(os.getenv("DOWNSTREAM_PORT", "8001"))
JSON_CONTENT_TYPE = "application/json"


class DownstreamHandler(http.server.SimpleHTTPRequestHandler):
    def _send_json(self, payload):
        self.send_response(200)
        self.send_header("Content-type", JSON_CONTENT_TYPE)
        self.end_headers()
        self.wfile.write(json.dumps(payload).encode("utf-8"))

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path

        if path == "/ping":
            self._send_json({
                "status": "ok",
                "service": "chaos-demo-downstream",
                "timestamp": datetime.datetime.now().isoformat()
            })
        elif path == "/data":
            delay = float(os.getenv("DOWNSTREAM_DELAY_MS", "50")) / 1000.0
            time.sleep(delay)
            self._send_json({
                "status": "ok",
                "service": "chaos-demo-downstream",
                "data": {"fraud_score": 0.03, "transaction_id": "tx-abc-123"},
                "timestamp": datetime.datetime.now().isoformat()
            })
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"Not Found")


class ReusableTCPServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    with ReusableTCPServer(("", PORT), DownstreamHandler) as httpd:
        print(f"Downstream server running on port {PORT}")
        httpd.serve_forever()
