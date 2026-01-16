#!/usr/bin/env php
<?php
header("Content-Type: text/html");
?>
<!DOCTYPE html>
<html>
<head>
	<meta charset="UTF-8">
    <title>PHP Information</title>
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
            color: #7b1fa2;
            text-align: center;
            margin-bottom: 30px;
        }
        h2 {
            color: #00d4ff;
            margin-top: 30px;
            padding-bottom: 10px;
            border-bottom: 1px solid #333;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            background: rgba(255,255,255,0.05);
            border-radius: 10px;
            overflow: hidden;
            margin-bottom: 20px;
        }
        th, td {
            padding: 12px 15px;
            text-align: left;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        th {
            background: rgba(123,31,162,0.3);
            color: #ce93d8;
            font-weight: 600;
        }
        tr:hover {
            background: rgba(255,255,255,0.05);
        }
        td:first-child {
            font-family: 'Monaco', 'Consolas', monospace;
            color: #7b1fa2;
            font-weight: 500;
            width: 40%;
        }
        td:last-child {
            font-family: 'Monaco', 'Consolas', monospace;
            word-break: break-all;
            color: #aaa;
        }
        .info-card {
            background: rgba(255,255,255,0.1);
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 20px;
        }
        .info-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }
        .info-item {
            background: rgba(0,0,0,0.3);
            padding: 15px;
            border-radius: 8px;
        }
        .info-item .label {
            color: #888;
            font-size: 0.85em;
            margin-bottom: 5px;
        }
        .info-item .value {
            color: #00d4ff;
            font-family: 'Monaco', 'Consolas', monospace;
            font-size: 1.1em;
        }
        a {
            color: #00d4ff;
            text-decoration: none;
            display: inline-block;
            margin-top: 20px;
        }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🐘 PHP Information</h1>
        
        <div class="info-card">
            <div class="info-grid">
                <div class="info-item">
                    <div class="label">PHP Version</div>
                    <div class="value"><?php echo phpversion(); ?></div>
                </div>
                <div class="info-item">
                    <div class="label">Server API</div>
                    <div class="value"><?php echo php_sapi_name(); ?></div>
                </div>
                <div class="info-item">
                    <div class="label">System</div>
                    <div class="value"><?php echo PHP_OS; ?></div>
                </div>
                <div class="info-item">
                    <div class="label">Current Time</div>
                    <div class="value"><?php echo date('H:i:s'); ?></div>
                </div>
            </div>
        </div>
        
        <h2>Server Variables ($_SERVER)</h2>
        <table>
            <tr><th>Variable</th><th>Value</th></tr>
            <?php
            $important_vars = [
                'SERVER_SOFTWARE', 'SERVER_NAME', 'SERVER_PORT',
                'REQUEST_METHOD', 'REQUEST_URI', 'QUERY_STRING',
                'SCRIPT_NAME', 'SCRIPT_FILENAME', 'PATH_INFO',
                'REMOTE_ADDR', 'REMOTE_PORT', 'CONTENT_TYPE', 'CONTENT_LENGTH',
                'HTTP_HOST', 'HTTP_USER_AGENT', 'HTTP_ACCEPT'
            ];
            
            foreach ($important_vars as $var) {
                if (isset($_SERVER[$var])) {
                    $value = htmlspecialchars($_SERVER[$var]);
                    echo "<tr><td>$var</td><td>$value</td></tr>";
                }
            }
            ?>
        </table>
        
        <h2>Environment Variables</h2>
        <table>
            <tr><th>Variable</th><th>Value</th></tr>
            <?php
            $env_vars = ['GATEWAY_INTERFACE', 'SERVER_PROTOCOL', 'DOCUMENT_ROOT', 'REDIRECT_STATUS'];
            foreach ($env_vars as $var) {
                $value = getenv($var);
                if ($value !== false) {
                    $value = htmlspecialchars($value);
                    echo "<tr><td>$var</td><td>$value</td></tr>";
                }
            }
            ?>
        </table>
        
        <h2>Loaded Extensions</h2>
        <div class="info-card">
            <?php echo implode(', ', get_loaded_extensions()); ?>
        </div>
        
        <a href="/"><- Back to Test Suite</a>
    </div>
</body>
</html>
