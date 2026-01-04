#!/usr/bin/env python3
# CGI script to handle POST form data

import os
import sys
import urllib.parse

print("Content-Type: text/html")
print("")

# Get POST data
content_length = int(os.environ.get('CONTENT_LENGTH', 0))
post_data = sys.stdin.read(content_length) if content_length > 0 else ""

# Parse form data
params = urllib.parse.parse_qs(post_data)

name = params.get('name', ['Anonymous'])[0]
message = params.get('message', ['No message'])[0]

# Escape HTML
def escape_html(text):
    return text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;').replace('"', '&quot;')

name = escape_html(name)
message = escape_html(message)

print(f"""<!DOCTYPE html>
<html>
<head>
    <title>Form Submission Result</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #11998e 0%, #38ef7d 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            margin: 0;
        }}
        .container {{
            background: white;
            padding: 40px 60px;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            text-align: center;
            max-width: 500px;
        }}
        h1 {{ color: #11998e; margin-bottom: 20px; }}
        .success-icon {{ font-size: 4em; margin-bottom: 20px; }}
        .data-box {{
            background: #f5f5f5;
            padding: 20px;
            border-radius: 10px;
            text-align: left;
            margin: 20px 0;
        }}
        .data-box p {{
            margin: 10px 0;
            color: #333;
        }}
        .data-box strong {{
            color: #11998e;
        }}
        .raw-data {{
            background: #2d2d2d;
            color: #eee;
            padding: 15px;
            border-radius: 8px;
            font-family: 'Monaco', 'Consolas', monospace;
            font-size: 0.85em;
            text-align: left;
            word-break: break-all;
            margin-top: 20px;
        }}
        a {{
            color: #11998e;
            text-decoration: none;
            display: inline-block;
            margin-top: 20px;
        }}
        a:hover {{ text-decoration: underline; }}
    </style>
</head>
<body>
    <div class="container">
        <div class="success-icon">✅</div>
        <h1>Form Submitted Successfully!</h1>
        
        <div class="data-box">
            <p><strong>Name:</strong> {name}</p>
            <p><strong>Message:</strong> {message}</p>
        </div>
        
        <div class="raw-data">
            <strong>Raw POST Data:</strong><br>
            {escape_html(post_data) if post_data else "(empty)"}
        </div>
        
        <p style="color: #888; margin-top: 20px;">
            Content-Length: {content_length} bytes
        </p>
        
        <a href="/">← Back to Test Suite</a>
    </div>
</body>
</html>""")
