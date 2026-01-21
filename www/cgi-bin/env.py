#!/usr/bin/env python3
# CGI script to display all environment variables

import os

print("Content-Type: text/html")
print("")

print("""<!DOCTYPE html>
<html>
<head>
      <meta charset="UTF-8">
    <title>CGI Environment Variables</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1a1a2e;
            color: #eee;
            margin: 0;
            padding: 20px;
        }
        .container {
            max-width: 1000px;
            margin: 0 auto;
        }
        h1 {
            color: #00d4ff;
            text-align: center;
            margin-bottom: 30px;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            background: rgba(255,255,255,0.05);
            border-radius: 10px;
            overflow: hidden;
        }
        th, td {
            padding: 12px 15px;
            text-align: left;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        th {
            background: rgba(0,212,255,0.2);
            color: #00d4ff;
            font-weight: 600;
        }
        tr:hover {
            background: rgba(255,255,255,0.05);
        }
        td:first-child {
            font-family: 'Monaco', 'Consolas', monospace;
            color: #7b2ff7;
            font-weight: 500;
        }
        td:last-child {
            font-family: 'Monaco', 'Consolas', monospace;
            word-break: break-all;
            color: #aaa;
        }
        .cgi-var td:first-child { color: #00c853; }
        .http-var td:first-child { color: #ff9800; }
        .server-var td:first-child { color: #00d4ff; }
        a {
            color: #00d4ff;
            text-decoration: none;
            display: inline-block;
            margin-top: 20px;
        }
        a:hover { text-decoration: underline; }
        .section { margin-top: 30px; }
        .section h2 { color: #888; font-size: 1em; margin-bottom: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔍 CGI Environment Variables</h1>
""")

# Separate variables into categories
cgi_vars = ['GATEWAY_INTERFACE', 'SERVER_PROTOCOL', 'SERVER_SOFTWARE', 'REQUEST_METHOD',
            'SCRIPT_NAME', 'SCRIPT_FILENAME', 'PATH_INFO', 'PATH_TRANSLATED',
            'QUERY_STRING', 'REQUEST_URI', 'CONTENT_TYPE', 'CONTENT_LENGTH',
            'DOCUMENT_ROOT', 'REDIRECT_STATUS']

server_vars = ['SERVER_NAME', 'SERVER_PORT', 'REMOTE_ADDR', 'REMOTE_PORT']

http_vars = [k for k in os.environ.keys() if k.startswith('HTTP_')]

other_vars = [k for k in os.environ.keys() 
              if k not in cgi_vars and k not in server_vars and k not in http_vars]

def print_table(title, vars_list, css_class=""):
    if not any(v in os.environ for v in vars_list):
        return
    print(f'<div class="section"><h2>{title}</h2>')
    print('<table>')
    print('<tr><th>Variable</th><th>Value</th></tr>')
    for key in sorted(vars_list):
        if key in os.environ:
            value = os.environ[key]
            # Escape HTML
            value = value.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
            print(f'<tr class="{css_class}"><td>{key}</td><td>{value}</td></tr>')
    print('</table></div>')

print_table("CGI Variables", cgi_vars, "cgi-var")
print_table("Server Variables", server_vars, "server-var")
print_table("HTTP Headers", http_vars, "http-var")
print_table("Other Variables", other_vars[:20])  # Limit other vars

print("""
        <a href="/"><- Back to Test Suite</a>
    </div>
</body>
</html>""")
