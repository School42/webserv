#!/usr/bin/env php
<?php
header("Content-Type: text/html");

$username = isset($_POST['username']) ? htmlspecialchars($_POST['username']) : '';
$email = isset($_POST['email']) ? htmlspecialchars($_POST['email']) : '';
$raw_post = file_get_contents('php://input');
?>
<!DOCTYPE html>
<html>
<head>
	<meta charset="UTF-8">
    <title>Form Submission - PHP</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #00695c 0%, #26a69a 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            margin: 0;
        }
        .container {
            background: white;
            padding: 40px 50px;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            text-align: center;
            max-width: 500px;
            width: 90%;
        }
        h1 { color: #00695c; margin-bottom: 10px; }
        .icon { font-size: 4em; margin-bottom: 20px; }
        .result-card {
            background: #f5f5f5;
            padding: 20px;
            border-radius: 10px;
            text-align: left;
            margin: 20px 0;
        }
        .result-card p {
            margin: 10px 0;
            color: #333;
        }
        .result-card strong {
            color: #00695c;
        }
        .raw-data {
            background: #263238;
            color: #aed581;
            padding: 15px;
            border-radius: 8px;
            font-family: 'Monaco', 'Consolas', monospace;
            font-size: 0.85em;
            text-align: left;
            margin-top: 20px;
            word-break: break-all;
        }
        .meta {
            color: #888;
            font-size: 0.9em;
            margin-top: 20px;
        }
        a {
            color: #00695c;
            text-decoration: none;
            display: inline-block;
            margin-top: 20px;
        }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <div class="icon">✅</div>
        <h1>Form Submitted!</h1>
        <p style="color: #666;">Your data has been received by PHP CGI</p>
        
        <div class="result-card">
            <p><strong>Username:</strong> <?php echo $username ?: '<em>Not provided</em>'; ?></p>
            <p><strong>Email:</strong> <?php echo $email ?: '<em>Not provided</em>'; ?></p>
        </div>
        
        <div class="result-card">
            <p><strong>All POST Data:</strong></p>
            <?php if (empty($_POST)): ?>
                <p><em>No POST data received</em></p>
            <?php else: ?>
                <?php foreach ($_POST as $key => $value): ?>
                    <p><strong><?php echo htmlspecialchars($key); ?>:</strong> <?php echo htmlspecialchars($value); ?></p>
                <?php endforeach; ?>
            <?php endif; ?>
        </div>
        
        <div class="raw-data">
            <strong>Raw POST Body:</strong><br><br>
            <?php echo htmlspecialchars($raw_post) ?: '<em>(empty)</em>'; ?>
        </div>
        
        <p class="meta">
            Content-Length: <?php echo isset($_SERVER['CONTENT_LENGTH']) ? $_SERVER['CONTENT_LENGTH'] : '0'; ?> bytes<br>
            Content-Type: <?php echo isset($_SERVER['CONTENT_TYPE']) ? htmlspecialchars($_SERVER['CONTENT_TYPE']) : 'Not specified'; ?>
        </p>
        
        <a href="/"><- Back to Test Suite</a>
    </div>
</body>
</html>
