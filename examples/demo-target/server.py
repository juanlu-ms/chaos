import http.server
import socketserver
import threading
import time
import json
import datetime
import math

PORT = 8000
memory_hog = []

class DemoHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        global memory_hog
        if self.path == '/allocate':
            # Allocate ~10MB of memory
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
        else:
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"Chaos Target Running")

def logger_thread():
    while True:
        log_entry = {
            "status": "processing",
            "time": datetime.datetime.now().isoformat(),
            "mem_hog_size_mb": len(memory_hog) * 10
        }
        print(json.dumps(log_entry))
        time.sleep(1)

if __name__ == "__main__":
    # Start the background logger thread
    t = threading.Thread(target=logger_thread, daemon=True)
    t.start()
    
    # Start the HTTP server
    with socketserver.TCPServer(("", PORT), DemoHandler) as httpd:
        print(f"Starting target application on port {PORT}...")
        httpd.serve_forever()
