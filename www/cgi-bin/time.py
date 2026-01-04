#!/usr/bin/env python3
# CGI script to display current time

import datetime
import time

print("Content-Type: text/html")
print("")

now = datetime.datetime.now()
utc_now = datetime.datetime.utcnow()

print(f"""<!DOCTYPE html>
<html>
<head>
    <title>Current Time - Python CGI</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #0f0c29 0%, #302b63 50%, #24243e 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            margin: 0;
            color: #fff;
        }}
        .container {{
            text-align: center;
            padding: 40px;
        }}
        .time-display {{
            font-size: 5em;
            font-weight: 300;
            margin: 20px 0;
            font-family: 'Monaco', 'Consolas', monospace;
            text-shadow: 0 0 20px rgba(0,212,255,0.5);
        }}
        .date-display {{
            font-size: 1.5em;
            color: #888;
            margin-bottom: 40px;
        }}
        .info {{
            background: rgba(255,255,255,0.1);
            padding: 20px 40px;
            border-radius: 10px;
            display: inline-block;
        }}
        .info p {{
            margin: 10px 0;
            color: #aaa;
        }}
        .info strong {{
            color: #00d4ff;
        }}
        a {{
            color: #00d4ff;
            text-decoration: none;
            display: inline-block;
            margin-top: 30px;
        }}
        a:hover {{ text-decoration: underline; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🕐 Current Server Time</h1>
        <div class="time-display">{now.strftime('%H:%M:%S')}</div>
        <div class="date-display">{now.strftime('%A, %B %d, %Y')}</div>
        
        <div class="info">
            <p><strong>Local Time:</strong> {now.strftime('%Y-%m-%d %H:%M:%S')}</p>
            <p><strong>UTC Time:</strong> {utc_now.strftime('%Y-%m-%d %H:%M:%S')}</p>
            <p><strong>Timestamp:</strong> {int(time.time())}</p>
            <p><strong>Timezone:</strong> {time.tzname[0]}</p>
        </div>
        
        <a href="/">← Back to Test Suite</a>
    </div>
</body>
</html>""")
