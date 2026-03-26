import http.server
import socketserver
import threading
import time
import json
import datetime
import math
import os

PORT = 8000
memory_hog = []
memory_hog_lock = threading.Lock()


def env_flag(name, default=False):
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in ("1", "true", "yes", "on")

class DemoHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == '/allocate':
            # Allocate ~10MB of memory
            with memory_hog_lock:
                memory_hog.append(' ' * (10 * 1024 * 1024))
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"Allocated 10MB")
        elif self.path == '/cpu':
            # Busy wait for 2 seconds to burn CPU cycles
            end_time = time.time() + 2
            while time.time() < end_time:
                math.factorial(100)
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"Burned CPU for 2s")
        elif self.path == '/ping':
            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            response_data = {
                "status": "ok",
                "timestamp": datetime.datetime.now().isoformat()
            }
            self.wfile.write(json.dumps(response_data).encode('utf-8'))
        else:
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"Chaos Target Running")

def logger_thread():
    while True:
        with memory_hog_lock:
            hog_size_mb = len(memory_hog) * 10
        log_entry = {
            "status": "processing",
            "time": datetime.datetime.now().isoformat(),
            "mem_hog_size_mb": hog_size_mb
        }
        print(json.dumps(log_entry))
        time.sleep(1)

if __name__ == "__main__":
    threaded_mode = env_flag("CHAOS_THREADED", default=True)
    server_class = socketserver.ThreadingTCPServer if threaded_mode else socketserver.TCPServer

    class DemoTCPServer(server_class):
        allow_reuse_address = True
        if threaded_mode:
            daemon_threads = True

    # Start the background logger thread
    t = threading.Thread(target=logger_thread, daemon=True)
    t.start()
    
    # Start the HTTP server
    with DemoTCPServer(("", PORT), DemoHandler) as httpd:
        mode_name = "ThreadingTCPServer" if threaded_mode else "TCPServer"
        print(f"Starting target application on port {PORT} with {mode_name}...")
        httpd.serve_forever()
