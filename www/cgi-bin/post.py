#!/usr/bin/env python3
# CGI script to display raw POST data

import os
import sys

print("Content-Type: text/html")
print("")

# Get POST data
content_length = int(os.environ.get('CONTENT_LENGTH', 0))
content_type = os.environ.get('CONTENT_TYPE', 'Not specified')
post_data = sys.stdin.read(content_length) if content_length > 0 else ""

# Escape HTML
def escape_html(text):
    return text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

print(f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>POST Data Received</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            padding: 40px 20px;
            margin: 0;
            color: #eee;
        }}
        .container {{
            max-width: 800px;
            margin: 0 auto;
        }}
        h1 {{
            color: #00d4ff;
            text-align: center;
            margin-bottom: 30px;
        }}
        .info-card {{
            background: rgba(255,255,255,0.1);
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 20px;
        }}
        .info-card h2 {{
            color: #00d4ff;
            font-size: 1.1em;
            margin-bottom: 15px;
        }}
        .info-row {{
            display: flex;
            justify-content: space-between;
            padding: 10px 0;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }}
        .info-row:last-child {{
            border-bottom: none;
        }}
        .label {{
            color: #888;
        }}
        .value {{
            color: #00c853;
            font-family: 'Monaco', 'Consolas', monospace;
        }}
        .data-box {{
            background: rgba(0,0,0,0.4);
            padding: 20px;
            border-radius: 10px;
            font-family: 'Monaco', 'Consolas', monospace;
            font-size: 0.9em;
            white-space: pre-wrap;
            word-break: break-all;
            max-height: 400px;
            overflow-y: auto;
        }}
        .empty {{
            color: #888;
            font-style: italic;
        }}
        a {{
            color: #00d4ff;
            text-decoration: none;
            display: inline-block;
            margin-top: 20px;
        }}
        a:hover {{ text-decoration: underline; }}
        .test-form {{
            margin-top: 30px;
            padding: 20px;
            background: rgba(255,255,255,0.05);
            border-radius: 10px;
        }}
        .test-form textarea {{
            width: 100%;
            height: 100px;
            padding: 10px;
            border: 1px solid rgba(255,255,255,0.2);
            border-radius: 8px;
            background: rgba(0,0,0,0.3);
            color: #eee;
            font-family: 'Monaco', 'Consolas', monospace;
            resize: vertical;
        }}
        .test-form button {{
            margin-top: 10px;
            padding: 10px 20px;
            background: linear-gradient(90deg, #00d4ff, #7b2ff7);
            border: none;
            border-radius: 8px;
            color: white;
            cursor: pointer;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>📥 POST Data Received</h1>
        
        <div class="info-card">
            <h2>Request Information</h2>
            <div class="info-row">
                <span class="label">Content-Type:</span>
                <span class="value">{escape_html(content_type)}</span>
            </div>
            <div class="info-row">
                <span class="label">Content-Length:</span>
                <span class="value">{content_length} bytes</span>
            </div>
            <div class="info-row">
                <span class="label">Request Method:</span>
                <span class="value">{os.environ.get('REQUEST_METHOD', 'Unknown')}</span>
            </div>
        </div>
        
        <div class="info-card">
            <h2>POST Body</h2>
            <div class="data-box">{"<span class='empty'>(No data received)</span>" if not post_data else escape_html(post_data)}</div>
        </div>
        
        <div class="test-form">
            <h2 style="color: #888; margin-bottom: 15px;">Test Another POST</h2>
            <form method="post" action="/cgi-bin/post.py">
                <textarea name="data" placeholder="Enter data to POST..."></textarea>
                <button type="submit">Send POST Request</button>
            </form>
        </div>
        
        <a href="/"><- Back to Test Suite</a>
    </div>
</body>
</html>""")
