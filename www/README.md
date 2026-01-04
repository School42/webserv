# Webserv Test Suite

A comprehensive test suite for testing all webserv functionality.

## Quick Start

1. **Setup the test environment:**
   ```bash
   chmod +x setup.sh
   ./setup.sh
   ```

2. **Run the server:**
   ```bash
   ./webserv /tmp/www/test.conf
   ```

3. **Open the test page:**
   ```
   http://localhost:8080/
   ```

## Test Features

### Static File Serving
- HTML files: `http://localhost:8080/`
- Text files: `http://localhost:8080/test.txt`
- JSON files: `http://localhost:8080/test.json`
- Directory listing: `http://localhost:8080/images/`

### CGI Scripts

#### Python CGI
- Hello World: `http://localhost:8080/cgi-bin/hello.py`
- Environment: `http://localhost:8080/cgi-bin/env.py`
- Time: `http://localhost:8080/cgi-bin/time.py`
- Calculator: `http://localhost:8080/cgi-bin/calc.py?a=10&b=5&op=add`
- POST Form: `http://localhost:8080/cgi-bin/form.py`
- POST Data: `http://localhost:8080/cgi-bin/post.py`

#### PHP CGI
- Hello World: `http://localhost:8080/cgi-bin/hello.php`
- PHP Info: `http://localhost:8080/cgi-bin/info.php`
- Time: `http://localhost:8080/cgi-bin/time.php`
- Query String: `http://localhost:8080/cgi-bin/query.php?name=Test&value=123`
- POST Form: `http://localhost:8080/cgi-bin/form.php`

#### Shell CGI
- Hello World: `http://localhost:8080/cgi-bin/hello.sh`
- System Info: `http://localhost:8080/cgi-bin/sysinfo.sh`
- Date/Time: `http://localhost:8080/cgi-bin/date.sh`

### File Uploads
- Upload form: Use the test page at `http://localhost:8080/`
- View uploads: `http://localhost:8080/uploads/`

### Error Pages
- 404 Not Found: `http://localhost:8080/nonexistent`
- Custom error pages in `/errors/`

### Redirects
- 301 Redirect: `http://localhost:8080/old-page`
- 302 Redirect: `http://localhost:8080/temp-redirect`

## Curl Commands

```bash
# GET request
curl http://localhost:8080/

# POST form data
curl -X POST -d "name=John&message=Hello" http://localhost:8080/cgi-bin/form.py

# File upload
curl -X POST -F "file=@myfile.txt" http://localhost:8080/upload

# Multiple file upload
curl -X POST -F "file1=@file1.txt" -F "file2=@file2.txt" http://localhost:8080/upload

# View headers
curl -I http://localhost:8080/

# Verbose output
curl -v http://localhost:8080/

# Test CGI with query string
curl "http://localhost:8080/cgi-bin/calc.py?a=100&b=25&op=div"
```

## Directory Structure

```
/tmp/www/
├── html/
│   ├── index.html          # Main test page
│   ├── test.txt            # Text file test
│   ├── test.json           # JSON file test
│   ├── new-page.html       # Redirect target
│   ├── errors/
│   │   ├── 404.html        # Custom 404 page
│   │   └── 500.html        # Custom 500 page
│   └── images/             # Directory listing test
├── cgi-bin/
│   ├── hello.py            # Python hello world
│   ├── env.py              # Environment viewer
│   ├── time.py             # Current time
│   ├── calc.py             # Calculator
│   ├── form.py             # Form handler
│   ├── post.py             # POST data viewer
│   ├── hello.php           # PHP hello world
│   ├── info.php            # PHP info
│   ├── time.php            # PHP time
│   ├── query.php           # Query string handler
│   ├── form.php            # PHP form handler
│   ├── hello.sh            # Shell hello world
│   ├── sysinfo.sh          # System information
│   └── date.sh             # Date/time
├── uploads/                # Upload storage
├── downloads/              # Downloadable files
└── test.conf               # Server configuration
```

## Configuration

The test configuration (`test.conf`) sets up:
- Main server on port 8080
- Virtual host `test.local` on port 8080
- Alternative server on port 9090
- CGI support for .py, .php, .sh extensions
- File upload with 100MB limit
- Custom error pages
- Directory autoindex

## Requirements

- Python 3 (for Python CGI scripts)
- PHP CLI (for PHP CGI scripts)
- Bash/sh (for shell scripts)

## Troubleshooting

**CGI scripts not executing?**
- Check that scripts have execute permission: `chmod +x /tmp/www/cgi-bin/*`
- Check that interpreters are installed: `which python3 php`

**File upload failing?**
- Check upload directory permissions: `chmod 777 /tmp/www/uploads`
- Check client_max_body_size in configuration

**404 errors?**
- Verify files exist in /tmp/www/html/
- Run setup.sh again to recreate files
