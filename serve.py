#!/usr/bin/env python3
"""支持 Range 请求的简易 HTTP 服务器"""
import http.server
import os
import re

class RangeHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def send_head(self):
        path = self.translate_path(self.path)
        if os.path.isdir(path):
            return super().send_head()
        
        if not os.path.exists(path):
            self.send_error(404, "File not found")
            return None
        
        file_size = os.path.getsize(path)
        range_header = self.headers.get('Range')
        
        if range_header:
            # 解析 Range: bytes=start-end
            match = re.match(r'bytes=(\d*)-(\d*)', range_header)
            if match:
                start = int(match.group(1)) if match.group(1) else 0
                end = int(match.group(2)) if match.group(2) else file_size - 1
                
                if start >= file_size:
                    self.send_error(416, "Range Not Satisfiable")
                    return None
                
                end = min(end, file_size - 1)
                content_length = end - start + 1
                
                self.send_response(206)
                self.send_header("Content-Type", self.guess_type(path))
                self.send_header("Content-Length", content_length)
                self.send_header("Content-Range", f"bytes {start}-{end}/{file_size}")
                self.send_header("Accept-Ranges", "bytes")
                self.end_headers()
                
                f = open(path, 'rb')
                f.seek(start)
                return _RangeFile(f, content_length)
        
        # 正常请求
        self.send_response(200)
        self.send_header("Content-Type", self.guess_type(path))
        self.send_header("Content-Length", file_size)
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()
        return open(path, 'rb')

class _RangeFile:
    """限制读取长度的文件包装器"""
    def __init__(self, f, length):
        self.f = f
        self.remaining = length
    
    def read(self, n=-1):
        if self.remaining <= 0:
            return b''
        if n < 0 or n > self.remaining:
            n = self.remaining
        data = self.f.read(n)
        self.remaining -= len(data)
        return data
    
    def close(self):
        self.f.close()

if __name__ == '__main__':
    import sys
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    
    os.chdir(os.path.dirname(os.path.abspath(__file__)) + '/sample')
    
    with http.server.HTTPServer(('', port), RangeHTTPRequestHandler) as httpd:
        print(f"Serving at http://0.0.0.0:{port}/ (with Range support)")
        print(f"Directory: {os.getcwd()}")
        httpd.serve_forever()
