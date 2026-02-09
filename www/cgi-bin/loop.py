#!/usr/bin/env python3
import time
import sys

# Required CGI headers
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("Cache-Control: no-cache\r\n\r\n")
sys.stdout.flush()

i = 0
while True:
    sys.stdout.write(f"tick {i}\n")
    sys.stdout.flush()   # VERY important for non-blocking tests
    i += 1
    time.sleep(1)