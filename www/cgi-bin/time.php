#!/usr/bin/env php
<?php
header("Content-Type: text/html");

$now = new DateTime();
$utc = new DateTime('now', new DateTimeZone('UTC'));
?>
<!DOCTYPE html>
<html>
<head>
    <title>Current Time - PHP CGI</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #4a148c 0%, #880e4f 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            margin: 0;
            color: #fff;
        }
        .container {
            text-align: center;
            padding: 40px;
        }
        .clock {
            font-size: 6em;
            font-weight: 200;
            font-family: 'Monaco', 'Consolas', monospace;
            margin: 30px 0;
            text-shadow: 0 0 30px rgba(206,147,216,0.5);
        }
        .date {
            font-size: 1.5em;
            color: rgba(255,255,255,0.7);
            margin-bottom: 40px;
        }
        .details {
            background: rgba(255,255,255,0.1);
            padding: 25px 40px;
            border-radius: 15px;
            display: inline-block;
        }
        .details p {
            margin: 12px 0;
            font-size: 1.1em;
        }
        .details strong {
            color: #ce93d8;
        }
        a {
            color: #ce93d8;
            text-decoration: none;
            display: inline-block;
            margin-top: 30px;
        }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🕐 Server Time (PHP)</h1>
        
        <div class="clock"><?php echo $now->format('H:i:s'); ?></div>
        <div class="date"><?php echo $now->format('l, F j, Y'); ?></div>
        
        <div class="details">
            <p><strong>Local:</strong> <?php echo $now->format('Y-m-d H:i:s'); ?></p>
            <p><strong>UTC:</strong> <?php echo $utc->format('Y-m-d H:i:s'); ?></p>
            <p><strong>Timestamp:</strong> <?php echo time(); ?></p>
            <p><strong>Timezone:</strong> <?php echo date_default_timezone_get(); ?></p>
            <p><strong>Week:</strong> <?php echo $now->format('W'); ?> of <?php echo $now->format('Y'); ?></p>
        </div>
        
        <a href="/"><- Back to Test Suite</a>
    </div>
</body>
</html>
