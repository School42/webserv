#!/bin/sh
# System Information Shell CGI script

echo "Content-Type: text/html"
echo ""

cat << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <title>System Information - Shell CGI</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            padding: 40px 20px;
            margin: 0;
            color: #eee;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
        }
        h1 {
            color: #4caf50;
            text-align: center;
            margin-bottom: 30px;
        }
        .info-card {
            background: rgba(255,255,255,0.1);
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 20px;
        }
        .info-card h2 {
            color: #4caf50;
            font-size: 1.1em;
            margin-bottom: 15px;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .info-row {
            display: flex;
            justify-content: space-between;
            padding: 10px 0;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        .info-row:last-child {
            border-bottom: none;
        }
        .label {
            color: #888;
        }
        .value {
            color: #aed581;
            font-family: 'Monaco', 'Consolas', monospace;
        }
        .terminal {
            background: rgba(0,0,0,0.5);
            padding: 15px;
            border-radius: 8px;
            font-family: 'Monaco', 'Consolas', monospace;
            font-size: 0.85em;
            white-space: pre-wrap;
            overflow-x: auto;
            color: #aed581;
        }
        a {
            color: #4caf50;
            text-decoration: none;
            display: inline-block;
            margin-top: 20px;
        }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🖥️ System Information</h1>
        
        <div class="info-card">
            <h2>📊 Basic Info</h2>
            <div class="info-row">
                <span class="label">Hostname</span>
                <span class="value">
EOF
hostname 2>/dev/null || echo "Unknown"
cat << 'EOF'
                </span>
            </div>
            <div class="info-row">
                <span class="label">Current User</span>
                <span class="value">
EOF
whoami 2>/dev/null || echo "Unknown"
cat << 'EOF'
                </span>
            </div>
            <div class="info-row">
                <span class="label">Shell</span>
                <span class="value">
EOF
echo "$SHELL"
cat << 'EOF'
                </span>
            </div>
            <div class="info-row">
                <span class="label">Working Directory</span>
                <span class="value">
EOF
pwd
cat << 'EOF'
                </span>
            </div>
        </div>
        
        <div class="info-card">
            <h2>💻 System</h2>
            <div class="terminal">
EOF
uname -a 2>/dev/null || echo "uname not available"
cat << 'EOF'
            </div>
        </div>
        
        <div class="info-card">
            <h2>📅 Date & Time</h2>
            <div class="terminal">
EOF
date 2>/dev/null || echo "date not available"
cat << 'EOF'
            </div>
        </div>
        
        <div class="info-card">
            <h2>⏰ Uptime</h2>
            <div class="terminal">
EOF
uptime 2>/dev/null || echo "uptime not available"
cat << 'EOF'
            </div>
        </div>
        
        <div class="info-card">
            <h2>🔧 Environment Variables</h2>
            <div class="terminal">
EOF
env | grep -E "^(SERVER_|REQUEST_|SCRIPT_|GATEWAY_|PATH=)" | sort | head -20
cat << 'EOF'
            </div>
        </div>
        
        <a href="/">← Back to Test Suite</a>
    </div>
</body>
</html>
EOF
