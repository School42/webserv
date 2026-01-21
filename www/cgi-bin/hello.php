#!/usr/bin/env php
<?php
// Output CGI header
echo "Content-Type: text/html\r\n";
echo "\r\n";

// Output HTML
echo "<!DOCTYPE html>\n";
echo "<html>\n";
echo "<head><title>Hello from PHP CGI</title></head>\n";
echo "<body>\n";
echo "<h1 style='color: #7b1fa2;'>Hello from PHP CGI!</h1>\n";
echo "<p>PHP Version: " . phpversion() . "</p>\n";
echo "<p><a href='/'>Back to Test Suite</a></p>\n";
echo "</body>\n";
echo "</html>\n";
?>
