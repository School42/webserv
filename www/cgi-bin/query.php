#!/usr/bin/env php
<?php
header("Content-Type: text/html");

$name = isset($_GET['name']) ? htmlspecialchars($_GET['name']) : 'Not provided';
$value = isset($_GET['value']) ? htmlspecialchars($_GET['value']) : 'Not provided';
$query_string = isset($_SERVER['QUERY_STRING']) ? htmlspecialchars($_SERVER['QUERY_STRING']) : '';
?>
<!DOCTYPE html>
<html>
<head>
	<meta charset="UTF-8">
    <title>Query String Parser - PHP</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            margin: 0;
            color: #eee;
        }
        .container {
            background: rgba(255,255,255,0.1);
            padding: 40px;
            border-radius: 20px;
            min-width: 450px;
        }
        h1 { 
            color: #ce93d8; 
            text-align: center;
            margin-bottom: 30px;
        }
        .data-card {
            background: rgba(0,0,0,0.3);
            padding: 20px;
            border-radius: 10px;
            margin-bottom: 20px;
        }
        .data-card h2 {
            color: #888;
            font-size: 0.9em;
            margin-bottom: 15px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .param {
            display: flex;
            justify-content: space-between;
            padding: 12px 0;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        .param:last-child { border-bottom: none; }
        .param-name { color: #ce93d8; font-weight: 500; }
        .param-value { 
            color: #00c853; 
            font-family: 'Monaco', 'Consolas', monospace;
        }
        .raw-query {
            background: rgba(0,0,0,0.5);
            padding: 15px;
            border-radius: 8px;
            font-family: 'Monaco', 'Consolas', monospace;
            font-size: 0.9em;
            word-break: break-all;
        }
        form {
            margin-top: 20px;
        }
        label {
            display: block;
            margin: 10px 0 5px;
            color: #888;
        }
        input {
            width: 100%;
            padding: 12px;
            border: none;
            border-radius: 8px;
            background: rgba(0,0,0,0.3);
            color: #eee;
            font-size: 1em;
            margin-bottom: 10px;
            box-sizing: border-box;
        }
        button {
            width: 100%;
            padding: 15px;
            background: linear-gradient(90deg, #7b1fa2, #ce93d8);
            border: none;
            border-radius: 8px;
            color: white;
            font-size: 1.1em;
            cursor: pointer;
            margin-top: 10px;
        }
        button:hover { opacity: 0.9; }
        a {
            color: #ce93d8;
            text-decoration: none;
            display: block;
            text-align: center;
            margin-top: 20px;
        }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔍 Query String Parser</h1>
        
        <div class="data-card">
            <h2>Parsed Parameters ($_GET)</h2>
            <?php if (empty($_GET)): ?>
                <p style="color: #888; font-style: italic;">No parameters received</p>
            <?php else: ?>
                <?php foreach ($_GET as $key => $val): ?>
                    <div class="param">
                        <span class="param-name"><?php echo htmlspecialchars($key); ?></span>
                        <span class="param-value"><?php echo htmlspecialchars($val); ?></span>
                    </div>
                <?php endforeach; ?>
            <?php endif; ?>
        </div>
        
        <div class="data-card">
            <h2>Raw Query String</h2>
            <div class="raw-query">
                <?php echo $query_string ?: '<em style="color:#888">Empty</em>'; ?>
            </div>
        </div>
        
        <form method="get" action="/cgi-bin/query.php">
            <h2 style="color: #888; font-size: 0.9em; margin-bottom: 15px;">TRY IT</h2>
            <label>Name:</label>
            <input type="text" name="name" placeholder="Enter name" value="<?php echo $name !== 'Not provided' ? $name : ''; ?>">
            <label>Value:</label>
            <input type="text" name="value" placeholder="Enter value" value="<?php echo $value !== 'Not provided' ? $value : ''; ?>">
            <label>Extra:</label>
            <input type="text" name="extra" placeholder="Enter extra data">
            <button type="submit">Submit Query</button>
        </form>
        
        <a href="/"><- Back to Test Suite</a>
    </div>
</body>
</html>
