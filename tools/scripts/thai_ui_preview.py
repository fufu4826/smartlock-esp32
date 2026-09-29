"""Local, read-only embedded-asset preview. Never connects to the ESP32.

?layout=1 disables scripts and exposes hidden sections for layout review.
All POSTs are denied. No Owner or credential is provisioned.
"""
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import urlsplit, parse_qs
import re

ROOT = Path(__file__).resolve().parents[1]
ROUTES = {'/': 'Index', '/setup': 'Setup', '/manage': 'Manage',
          '/enroll': 'Enroll', '/owner-bootstrap': 'OwnerBootstrap',
          '/verify-owner': 'VerifyOwner', '/reset': 'Reset', '/access': 'Access'}

class Preview(BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def send(self, body, status=200, mime='text/html; charset=utf-8'):
        encoded = body.encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', mime)
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def do_GET(self):
        url = urlsplit(self.path)
        if url.path == '/api/status':
            return self.send('{"configured":false,"state":"setup"}',
                             mime='application/json; charset=utf-8')
        name = ROUTES.get(url.path)
        if not name:
            return self.send('{"error":"preview_only"}', 403,
                             'application/json; charset=utf-8')
        source = (ROOT / 'src/web/WebAssets.h').read_text(encoding='utf-8-sig')
        match = re.search(r'static const char k' + name +
                          r'\[\] PROGMEM = R"HTML\((.*?)\)HTML";', source, re.S)
        if not match:
            return self.send('Missing preview asset', 500)
        html = match.group(1)
        if parse_qs(url.query).get('layout') == ['1']:
            html = re.sub(r'<script>.*?</script>', '', html, flags=re.S)
            html = re.sub(r'\s+hidden(?=[\s>])', '', html)
        self.send(html)

    def do_POST(self):
        self.send('{"error":"preview_read_only"}', 403,
                  'application/json; charset=utf-8')

if __name__ == '__main__':
    print('Read-only Thai UI preview: http://127.0.0.1:8765', flush=True)
    HTTPServer(('127.0.0.1', 8765), Preview).serve_forever()
