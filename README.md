# WebServ

*This project has been created as part of the 42 curriculum by talin, juhtoo-h*

---

## Table of Contents

- [Description](#description)
- [Instructions](#instructions)
  - [Requirements](#requirements)
  - [Compilation](#compilation)
  - [Configuration](#configuration)
  - [Usage](#usage)
  - [Testing](#testing)
- [Resources](#resources)
- [Technical Documentation](#technical-documentation)
- [Screenshots](#screenshots)

---

## Description

WebServ is a high-performance HTTP/1.1 web server implementation built from scratch in C++98. The project demonstrates advanced network programming concepts and provides a deep understanding of how web servers work at a fundamental level.

### Project Goals

The primary objective is to implement a fully functional web server that:
- Handles HTTP/1.1 protocol communication
- Manages multiple concurrent connections efficiently
- Serves static files and executes dynamic CGI scripts
- Processes file uploads with proper multipart parsing
- Implements virtual host support and URL routing

### Key Features

**Core HTTP/1.1 Implementation**
- ✅ Full support for GET, POST, and DELETE methods
- ✅ Persistent connections (keep-alive) for efficient resource usage
- ✅ Chunked transfer encoding for request body
- ✅ Proper HTTP request/response parsing and generation

**Advanced I/O Management**
- ⚡ Epoll-based event-driven architecture for scalability
- ⚡ Non-blocking I/O to handle thousands of concurrent connections
- ⚡ Single-threaded design with efficient multiplexing
- ⚡ Timeout management for idle connections

**Dynamic Content Generation**
- CGI support for Python, PHP, and Shell scripts
- Environment variable passing to CGI processes
- POST data handling for form submissions
- Configurable timeout management for CGI execution

**File Management**
- Multipart form-data parsing for file uploads
- Directory listing with autoindex support
- Custom error pages per server configuration
- File size limits and permission checks

**Configuration & Routing**
- Nginx-style configuration file parsing
- Virtual host support (multiple servers on different ports)
- Location-based routing with prefix matching
- HTTP redirects (301/302) via return directive
- Method restrictions per location

### Technical Highlights

The implementation showcases:
- **Socket Programming**: Low-level TCP socket operations for client-server communication
- **Epoll Multiplexing**: Linux's efficient I/O event notification for handling concurrent connections
- **State Machine Design**: Incremental HTTP parsing to handle partial data
- **Process Management**: Fork/exec pattern for CGI execution with proper cleanup
- **Configuration Parser**: Custom lexer/parser implementing nginx-style syntax with inheritance

---

## Instructions

### Requirements

**System Requirements**
- Operating System: Ubuntu/Linux (tested on Ubuntu 24)
- Compiler: g++ with C++98 support
- Build System: GNU Make

**Required Packages**

```bash
# Install required interpreters for CGI functionality
sudo apt-get update
sudo apt-get install python3 php-cli

# Optional: for testing
sudo apt-get install curl
```

### Compilation

**Basic Compilation**

```bash
# Clone the repository
git clone https://github.com/School42/webserv
cd webserv

# Compile the project
make

# Remove object files
make clean

# Remove object files and executable
make fclean

# Recompile from scratch
make re
```

**Verify Installation**

```bash
./webserv
# Should display: Usage: ./webserv <config_file>
```

### Configuration

WebServ uses nginx-style configuration files with `.conf` extension.

**Basic Configuration Structure**

```nginx
server {
    listen 8080;
    server_name localhost;
    
    root /var/www/html;
    index index.html;
    
    location / {
        allowed_methods GET POST;
        autoindex on;
    }
}
```

**Configuration Directives Reference**

| Directive | Scope | Description | Example |
|-----------|-------|-------------|---------|
| `listen` | Server | Port and optional interface | `listen 8080;`<br>`listen 127.0.0.1:8080;` |
| `server_name` | Server | Virtual host names | `server_name example.com;` |
| `error_page` | Server | Custom error page | `error_page 404 /errors/404.html;` |
| `root` | Both | Document root directory | `root /var/www/html;` |
| `index` | Both | Default index files | `index index.html index.htm;` |
| `autoindex` | Both | Directory listing | `autoindex on;` |
| `client_max_body_size` | Both | Max request body | `client_max_body_size 100M;` |
| `allowed_methods` | Location | Permitted HTTP methods | `allowed_methods GET POST;` |
| `return` | Location | HTTP redirect | `return 301 /new-page;` |
| `cgi_pass` | Location | CGI interpreter | `cgi_pass /usr/bin/python3;` |
| `cgi_extension` | Location | CGI file extensions | `cgi_extension .py .php;` |
| `upload_store` | Location | Upload directory | `upload_store /uploads;` |

**Complete Configuration Example**

```nginx
server {
    listen 8080;
    server_name localhost;
    
    root /var/www/html;
    index index.html index.htm;
    client_max_body_size 10M;
    
    error_page 404 /errors/404.html;
    error_page 500 502 503 504 /errors/50x.html;
    
    # Static files with directory listing
    location / {
        allowed_methods GET;
        autoindex on;
    }
    
    # CGI scripts
    location /cgi-bin/ {
        root /var/www;
        allowed_methods GET POST;
        cgi_extension .py .php .sh;
        cgi_pass /usr/bin/python3;
    }
    
    # File uploads
    location /upload {
        allowed_methods POST;
        upload_store /var/www/uploads;
        client_max_body_size 100M;
    }
    
    # Redirects
    location /old-page {
        return 301 /new-page.html;
    }
}
```

**Configuration Inheritance**

Location blocks inherit settings from their server:

```nginx
server {
    root /var/www/html;              # Inherited by all locations
    client_max_body_size 10M;        # Inherited by all locations
    
    location /api/ {
        # Inherits: root, client_max_body_size
        allowed_methods GET POST;
    }
    
    location /uploads/ {
        client_max_body_size 100M;    # Overrides server-level
    }
}
```

### Usage

**Starting the Server**

```bash
./webserv <config_file.conf>
```

Example:
```bash
./webserv config/default.conf
```

**Expected Output**

```
✓ Configuration parsed successfully!
✓ Router initialized
✓ File server initialized
✓ CGI handler initialized
✓ Upload handler initialized
✓ Listening on 0.0.0.0:8080

=== Server is running. Press Ctrl+C to stop ===
```

**Making Requests**

Static file serving:
```bash
# GET request
curl http://localhost:8080/

# View headers
curl -I http://localhost:8080/index.html

# Download file
curl -O http://localhost:8080/file.pdf
```

CGI execution:
```bash
# Python CGI with query string
curl "http://localhost:8080/cgi-bin/calc.py?a=10&b=5&op=add"

# PHP CGI with POST data
curl -X POST -d "name=John&email=john@example.com" \
  http://localhost:8080/cgi-bin/form.php

# Shell script
curl http://localhost:8080/cgi-bin/sysinfo.sh
```

File uploads:
```bash
# Single file
curl -X POST -F "file=@document.pdf" \
  http://localhost:8080/upload

# Multiple files
curl -X POST \
  -F "file1=@image1.jpg" \
  -F "file2=@image2.jpg" \
  http://localhost:8080/upload
```

**Stopping the Server**

Press `Ctrl+C` for graceful shutdown:
```
^C
Received shutdown signal...
Shutting down...
✓ Server stopped gracefully
```

### Testing

**Automated Test Suite**

The project includes a comprehensive test suite with an interactive web dashboard.

1. Start the test server:
```bash
./webserv config/test.conf
```

2. Open browser to: `http://localhost:8080/`

3. The test dashboard provides interactive tests for:
   - Static file serving (HTML, text, JSON)
   - Directory listing (autoindex)
   - CGI scripts (Python, PHP, Shell)
   - File uploads (single, multiple, with form data)
   - HTTP redirects
   - Error pages
   - Method restrictions

**Manual Testing with cURL**

```bash
# Test static file
curl http://localhost:8080/test.txt

# Test directory listing
curl http://localhost:8080/images/

# Test CGI with query string
curl "http://localhost:8080/cgi-bin/calc.py?a=100&b=25&op=div"

# Test POST to CGI
curl -X POST -d "name=Test&message=Hello" \
  http://localhost:8080/cgi-bin/form.py

# Test file upload
curl -X POST -F "file=@testfile.txt" \
  http://localhost:8080/upload

# Test 404 error
curl http://localhost:8080/nonexistent

# Test redirect (follow redirects)
curl -L http://localhost:8080/old-page

# Verbose output (see full request/response)
curl -v http://localhost:8080/
```

**Test Coverage**

The test suite validates:
- ✅ Static file serving (all MIME types)
- ✅ Directory listing (autoindex on/off)
- ✅ Custom error pages (404, 500, etc.)
- ✅ URL redirects (301, 302)
- ✅ Python CGI (GET/POST, query strings, forms)
- ✅ PHP CGI (GET/POST, phpinfo, time, queries)
- ✅ Shell CGI (hello, system info, date/time)
- ✅ Single file upload
- ✅ Multiple file upload
- ✅ Multipart form data parsing
- ✅ Upload size limits (413 error)
- ✅ Method restrictions (405 error)
- ✅ Keep-alive connections
- ✅ Request timeouts
- ✅ Virtual hosts (server_name matching)

---

## Resources

### Classic References

**Books**

1. **Unix Network Programming, Volume 1: The Sockets Networking API** by W. Richard Stevens
   - Chapter 4: Elementary TCP Sockets
   - Chapter 5: TCP Client/Server Example
   - Chapter 6: I/O Multiplexing (select, poll, epoll)
   - Chapter 16: Nonblocking I/O
   
   *Essential for understanding socket programming and epoll implementation.*

2. **HTTP: The Definitive Guide** by David Gourley & Brian Totty
   - Chapter 1: Overview of HTTP
   - Chapter 3: HTTP Messages
   - Chapter 4: Connection Management
   - Chapter 17: Content Negotiation
   
   *Comprehensive reference for HTTP protocol implementation.*

**RFCs (Internet Standards)**

- [RFC 2616 - HTTP/1.1](https://www.rfc-editor.org/rfc/rfc2616) - Original HTTP/1.1 specification
- [RFC 7230-7235](https://www.rfc-editor.org/rfc/rfc7230) - Updated HTTP/1.1 specification
- [RFC 3875 - CGI 1.1](https://datatracker.ietf.org/doc/html/rfc3875) - CGI specification

**Online Documentation**

- [Epoll Manual](https://man7.org/linux/man-pages/man7/epoll.7.html) - Linux epoll documentation
- [Nginx Documentation](https://nginx.org/en/docs/) - Configuration syntax reference
- [CGI: Getting Started](https://www.w3.org/CGI/) - W3C CGI introduction

**Tutorials & Articles**

- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/) - Socket programming tutorial
- [The C10K Problem](http://www.kegel.com/c10k.html) - Understanding scalable I/O
- [HTTP Made Really Easy](https://www.jmarshall.com/easy/http/) - HTTP basics

### AI Usage in This Project

**Development Assistance**

AI tools were used for the following tasks:

1. **Code Review & Optimization**
   - Reviewed socket and epoll implementation for best practices
   - Suggested optimizations for HTTP parser state machine
   - Identified potential memory leaks in CGI process cleanup

2. **Documentation Generation**
   - Generated comprehensive README structure and content
   - Created detailed code comments for complex sections
   - Wrote technical explanations for architecture concepts

3. **Testing Strategy**
   - Suggested comprehensive test cases for HTTP edge cases
   - Helped design the test suite dashboard structure
   - Generated test CGI scripts (Python, PHP, Shell)

4. **Configuration Parser Design**
   - Discussed nginx-style configuration inheritance patterns
   - Reviewed lexer/parser implementation approach
   - Suggested error handling strategies for config validation

5. **Debugging Assistance**
   - Helped diagnose issues with chunked transfer encoding
   - Provided insights for CGI timeout handling

**Parts Implemented Without AI**

The following core components were implemented entirely by the team without AI assistance:
- Socket class and low-level socket operations
- Epoll wrapper and event loop architecture
- HTTP request parser state machine
- Router and request routing logic
- File server implementation
- CGI execution and process management
- Upload handler and multipart parsing
- Configuration lexer and parser

**Learning Approach**

AI was used as a **teaching assistant** and **rubber duck** rather than a code generator. The team:
- Wrote all code manually after understanding concepts
- Used AI to explain complex networking concepts
- Asked AI to review code for potential issues
- Requested AI to suggest alternative approaches for comparison

This approach ensured deep understanding while benefiting from AI's knowledge base.

---

## Technical Documentation

### HTTP Protocol Overview

**HTTP (Hypertext Transfer Protocol)** is the foundation of web communication. It follows a request-response model:

```
Client                          Server
  |  1. TCP Connection            |
  |------------------------------>|
  |  2. HTTP Request              |
  |  GET /index.html HTTP/1.1     |
  |------------------------------>|
  |  3. HTTP Response             |
  |  HTTP/1.1 200 OK              |
  |  <html>...</html>             |
  |<------------------------------|
  |  4. Keep-alive/Close          |
```

**Request Structure**
```
GET /path?query=value HTTP/1.1          ← Request Line
Host: localhost:8080                     ← Headers
User-Agent: curl/7.68.0
                                         ← Blank Line
[Request Body for POST]                  ← Body (optional)
```

**Response Structure**
```
HTTP/1.1 200 OK                          ← Status Line
Content-Type: text/html                  ← Headers
Content-Length: 1234

<html>...</html>                         ← Body
```

### Socket Programming Fundamentals

**TCP Socket Flow**

```
Server Side                    Client Side
----------                     -----------
socket()                       socket()
  ↓                              ↓
bind()                         connect()
  ↓                              ↓
listen()                         |
  ↓                              |
accept() <-------------------->  |
  ↓                              ↓
recv/send <------------------> send/recv
  ↓                              ↓
close()                        close()
```

**Non-Blocking I/O**

WebServ uses non-blocking sockets to prevent blocking the entire server:

```cpp
// Set socket to non-blocking mode
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

### Epoll: Scalable I/O Multiplexing

**Why Epoll?**

| Method | Max Connections | Performance | Complexity |
|--------|----------------|-------------|------------|
| select() | 1024 | O(n) | High |
| poll() | Unlimited | O(n) | Medium |
| **epoll()** | **Unlimited** | **O(1)** | **Low** |

**Epoll Architecture**

```
                    Epoll Instance
                         |
       +-----------------+-----------------+
       |                 |                 |
    Socket 1          Socket 2         Socket 3
    (Listen)          (Client)         (Client)
       |                 |                 |
    EPOLLIN           EPOLLIN          EPOLLOUT
   (Readable)        (Readable)       (Writable)
```

**Event-Driven Flow**

```
1. epoll_wait() returns events
       |
       ├─ Listen socket ready? → accept() new client
       |
       ├─ Client readable? → read request
       |                   → parse request
       |                   → process (file/CGI)
       |                   → switch to write mode
       |
       └─ Client writable? → write response
                           → keep-alive or close
```

### CGI Implementation

**CGI Architecture**

```
Browser          WebServ          CGI Script
   |                |                  |
   | HTTP Request   |                  |
   |--------------->|                  |
   |                | fork()+execve()  |
   |                |----------------->|
   |                | Environment +    |
   |                | POST body        |
   |                |----------------->|
   |                |      Output      |
   |                |<-----------------|
   |  HTTP Response |                  |
   |<---------------|                  |
```

**CGI Environment Variables**

```bash
REQUEST_METHOD=POST
REQUEST_URI=/cgi-bin/form.py
QUERY_STRING=name=John&age=25
SERVER_NAME=localhost
SERVER_PORT=8080
REMOTE_ADDR=127.0.0.1
CONTENT_TYPE=application/x-www-form-urlencoded
CONTENT_LENGTH=42
HTTP_USER_AGENT=Mozilla/5.0
```

**CGI Output Format**

```
Content-Type: text/html
Status: 200 OK
                              ← Blank line
<!DOCTYPE html>
<html>...</html>
```

### State Machine Design

**HTTP Parser States**

```
PARSE_REQUEST_LINE → GET /path HTTP/1.1
         ↓
PARSE_HEADERS      → Header: Value\r\n
         ↓
PARSE_BODY         → [request body]
         ↓
PARSE_COMPLETE     → Ready to process

Transitions:
- Incomplete data → PARSE_INCOMPLETE (wait)
- Valid data      → Next state
- Invalid data    → PARSE_FAILED (400 error)
```

**Chunked Transfer Encoding**

```
5\r\n              ← Chunk size (hex)
Hello\r\n          ← Chunk data
6\r\n
World!\r\n
0\r\n              ← Last chunk
\r\n
```

### Project Architecture

**Class Diagram (Simplified)**

```
                    main()
                      |
        +-------------+-------------+
        |             |             |
    Socket         Epoll      ClientManager
        |             |             |
        |             +----Event----+
        |                           |
        +----------Client-----------+
                      |
        +-------------+-------------+
        |             |             |
    HttpRequest    Router      Response
                      |
        +-------------+-------------+
        |             |             |
    FileServer   CgiHandler   UploadHandler
```

**Request Processing Flow**

```
1. Epoll detects readable socket
         ↓
2. Read data into buffer
         ↓
3. Parse HTTP request
         ↓
4. Route to handler (File/CGI/Upload)
         ↓
5. Generate HTTP response
         ↓
6. Write response to client
         ↓
7. Keep-alive or close connection
```

---

## Screenshots

### Test Dashboard
![Interactive test dashboard](https://github.com/user-attachments/assets/6b5fe3c6-511f-4b29-ae4d-69f10b3028ae)

*The test suite provides an interactive web interface for testing all server features.*

### CGI Execution
![Python CGI execution example](https://github.com/user-attachments/assets/fc65477a-f937-4184-ae05-31487836e241)

*Example of Python CGI script execution showing dynamic content generation.*

### File Upload Interface
![File upload success page](https://github.com/user-attachments/assets/04ac4930-6da1-4bed-b47c-73e8fa015785)

*Successful file upload with confirmation of uploaded files.*

### Server Console Output
![Server console showing request processing](https://github.com/user-attachments/assets/56084fbf-d146-4fbb-aab3-34c94aeb5225)

*Server console displaying real-time request processing and connection management.*

---

## 🤝 Contributing

This project is part of the 42 school curriculum. While primarily educational, suggestions and feedback are welcome through issues or pull requests.

---

## 📄 License

This project follows 42 school's academic policies and is intended for educational purposes.

---

**Developed with ❤️ at 42 School**