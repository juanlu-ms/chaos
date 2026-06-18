import http.server
import socketserver
import threading
import multiprocessing
import time
import json
import datetime
import math
import os
import urllib.request
from urllib.parse import urlparse, parse_qs

PORT = 8000
JSON_CONTENT_TYPE = 'application/json'
memory_hog = []
memory_hog_lock = threading.Lock()
cpu_hogs = []
cpu_hogs_lock = threading.Lock()


def env_flag(name, default=False):
    raw = os.getenv(name)
    if raw is None:
        return default
    return raw.strip().lower() in ("1", "true", "yes", "on")


def env_int(name, default):
    raw = os.getenv(name)
    if raw is None:
        return default
    try:
        value = int(raw.strip())
        return value if value > 0 else default
    except ValueError:
        return default


def burn_cpu(seconds=None):
    end_time = None if seconds is None else time.time() + seconds
    while True:
        # Tight loop with arithmetic work to keep CPU busy.
        math.factorial(100)
        if end_time is not None and time.time() >= end_time:
            return


def start_cpu_hogs(workers):
    started = []
    for _ in range(workers):
        p = multiprocessing.Process(target=burn_cpu, args=(None,), daemon=True)
        p.start()
        started.append(p)

    with cpu_hogs_lock:
        cpu_hogs.extend(started)

    return len(started)


def stop_cpu_hogs():
    with cpu_hogs_lock:
        running = list(cpu_hogs)
        cpu_hogs.clear()

    stopped = 0
    for p in running:
        if p.is_alive():
            p.terminate()
            p.join(timeout=1)
            stopped += 1
    return stopped


def active_cpu_hogs():
    with cpu_hogs_lock:
        alive = [p for p in cpu_hogs if p.is_alive()]
        cpu_hogs[:] = alive
        return len(alive)

class DemoHandler(http.server.SimpleHTTPRequestHandler):
    def _query_positive_int(self, query, key, default):
        if key not in query:
            return default
        try:
            return max(1, int(query[key][0]))
        except ValueError:
            return default

    def _send_json(self, payload):
        self.send_response(200)
        self.send_header('Content-type', JSON_CONTENT_TYPE)
        self.end_headers()
        self.wfile.write(json.dumps(payload).encode('utf-8'))

    def _send_text(self, payload):
        self.send_response(200)
        self.end_headers()
        self.wfile.write(payload)

    def _handle_allocate(self):
        # Allocate ~10MB of memory
        with memory_hog_lock:
            memory_hog.append(' ' * (10 * 1024 * 1024))
        self._send_text(b"Allocated 10MB")

    def _handle_cpu(self, query):
        seconds = self._query_positive_int(query, "seconds", env_int("CHAOS_CPU_SECONDS", 2))
        workers = self._query_positive_int(query, "workers", env_int("CHAOS_CPU_WORKERS", 1))

        # Use processes (not threads) to bypass the GIL and stress multiple cores.
        procs = []
        for _ in range(workers):
            p = multiprocessing.Process(target=burn_cpu, args=(seconds,))
            p.start()
            procs.append(p)
        for p in procs:
            p.join()

        self._send_json({
            "status": "ok",
            "action": "cpu",
            "seconds": seconds,
            "workers": workers
        })

    def _handle_cpu_start(self, query):
        workers = self._query_positive_int(query, "workers", env_int("CHAOS_CPU_WORKERS", 1))
        started = start_cpu_hogs(workers)
        self._send_json({
            "status": "ok",
            "action": "cpu_start",
            "started": started,
            "active": active_cpu_hogs()
        })

    def _handle_cpu_stop(self):
        stopped = stop_cpu_hogs()
        self._send_json({
            "status": "ok",
            "action": "cpu_stop",
            "stopped": stopped,
            "active": active_cpu_hogs()
        })

    def _handle_ping(self):
        self._send_json({
            "status": "ok",
            "timestamp": datetime.datetime.now().isoformat(),
            "active_cpu_hogs": active_cpu_hogs()
        })

    def _handle_call_downstream(self):
        downstream_url = os.getenv("DOWNSTREAM_URL", "http://127.0.0.1:8001/data")
        timeout_s = float(os.getenv("DOWNSTREAM_TIMEOUT", "5.0"))
        try:
            req = urllib.request.Request(downstream_url)
            with urllib.request.urlopen(req, timeout=timeout_s) as resp:
                body = resp.read().decode("utf-8")
            self._send_json({
                "status": "ok",
                "action": "call_downstream",
                "downstream_status": resp.status,
                "downstream_response": body[:200]
            })
        except Exception as e:
            self._send_json({
                "status": "error",
                "action": "call_downstream",
                "error": str(e)
            })

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        query = parse_qs(parsed.query)

        if path == '/allocate':
            self._handle_allocate()
        elif path == '/cpu':
            self._handle_cpu(query)
        elif path == '/cpu/start':
            self._handle_cpu_start(query)
        elif path == '/cpu/stop':
            self._handle_cpu_stop()
        elif path == '/ping':
            self._handle_ping()
        elif path == '/call-downstream':
            self._handle_call_downstream()
        else:
            self._send_text(b"Chaos Target Running")

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
