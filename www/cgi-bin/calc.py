#!/usr/bin/env python3
# CGI calculator script - demonstrates query string parsing

import os
import urllib.parse

print("Content-Type: text/html")
print("")

# Parse query string
query_string = os.environ.get('QUERY_STRING', '')
params = urllib.parse.parse_qs(query_string)

a = params.get('a', ['0'])[0]
b = params.get('b', ['0'])[0]
op = params.get('op', ['add'])[0]

result = None
error = None

try:
    a_num = float(a)
    b_num = float(b)
    
    if op == 'add':
        result = a_num + b_num
        symbol = '+'
    elif op == 'sub':
        result = a_num - b_num
        symbol = '-'
    elif op == 'mul':
        result = a_num * b_num
        symbol = '×'
    elif op == 'div':
        if b_num == 0:
            error = "Division by zero!"
        else:
            result = a_num / b_num
            symbol = '÷'
    elif op == 'mod':
        if b_num == 0:
            error = "Division by zero!"
        else:
            result = a_num % b_num
            symbol = '%'
    elif op == 'pow':
        result = a_num ** b_num
        symbol = '^'
    else:
        error = f"Unknown operation: {op}"
        symbol = '?'
except ValueError:
    error = "Invalid number format"
    symbol = '?'

print(f"""<!DOCTYPE html>
<html>
<head>
    <title>CGI Calculator</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            margin: 0;
            color: #fff;
        }}
        .calculator {{
            background: rgba(255,255,255,0.1);
            padding: 40px;
            border-radius: 20px;
            text-align: center;
            min-width: 400px;
        }}
        h1 {{ color: #00d4ff; margin-bottom: 30px; }}
        .display {{
            background: rgba(0,0,0,0.3);
            padding: 20px;
            border-radius: 10px;
            font-size: 2em;
            font-family: 'Monaco', 'Consolas', monospace;
            margin-bottom: 20px;
        }}
        .result {{
            color: #00c853;
            font-size: 2.5em;
            margin: 20px 0;
        }}
        .error {{
            color: #f44336;
            font-size: 1.2em;
        }}
        form {{
            margin-top: 30px;
            text-align: left;
        }}
        label {{
            display: block;
            margin: 10px 0 5px;
            color: #888;
        }}
        input, select {{
            width: 100%;
            padding: 12px;
            border: none;
            border-radius: 8px;
            background: rgba(0,0,0,0.3);
            color: #fff;
            font-size: 1em;
            margin-bottom: 10px;
        }}
        button {{
            width: 100%;
            padding: 15px;
            background: linear-gradient(90deg, #00d4ff, #7b2ff7);
            border: none;
            border-radius: 8px;
            color: white;
            font-size: 1.1em;
            cursor: pointer;
            margin-top: 10px;
        }}
        button:hover {{ opacity: 0.9; }}
        a {{
            color: #00d4ff;
            text-decoration: none;
            display: inline-block;
            margin-top: 20px;
        }}
        a:hover {{ text-decoration: underline; }}
    </style>
</head>
<body>
    <div class="calculator">
        <h1>🔢 CGI Calculator</h1>
        
        <div class="display">
            {a} {symbol} {b}
        </div>
        
        {"<div class='error'>" + error + "</div>" if error else f"<div class='result'>= {result}</div>"}
        
        <form method="get" action="/cgi-bin/calc.py">
            <label>First Number (a):</label>
            <input type="text" name="a" value="{a}" required>
            
            <label>Second Number (b):</label>
            <input type="text" name="b" value="{b}" required>
            
            <label>Operation:</label>
            <select name="op">
                <option value="add" {"selected" if op == "add" else ""}>Addition (+)</option>
                <option value="sub" {"selected" if op == "sub" else ""}>Subtraction (-)</option>
                <option value="mul" {"selected" if op == "mul" else ""}>Multiplication (×)</option>
                <option value="div" {"selected" if op == "div" else ""}>Division (÷)</option>
                <option value="mod" {"selected" if op == "mod" else ""}>Modulo (%)</option>
                <option value="pow" {"selected" if op == "pow" else ""}>Power (^)</option>
            </select>
            
            <button type="submit">Calculate</button>
        </form>
        
        <a href="/"><- Back to Test Suite</a>
    </div>
</body>
</html>""")
