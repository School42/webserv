# WebServ

A high-performance HTTP/1.1 web server implementation in C++98, featuring epoll-based I/O multiplexing, CGI support, and nginx-style configuration.

> **Note**: This is a team project developed as part of the 42 school curriculum.

## 📋 Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Requirements](#requirements)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Testing](#testing)
- [Architecture & Concepts](#architecture--concepts)
- [Resources](#resources)
- [Screenshots](#screenshots)

---

## 🌟 Overview

WebServ is a fully functional HTTP/1.1 web server built from scratch in C++98. It demonstrates advanced network programming concepts including non-blocking I/O, event-driven architecture, and proper HTTP protocol implementation.

### Key Technologies

- **Socket Programming**: Low-level TCP socket implementation for client-server communication
- **Epoll**: Linux's efficient I/O event notification mechanism for handling thousands of connections
- **CGI**: Common Gateway Interface support for dynamic content (Python, PHP, Shell scripts)
- **HTTP/1.1**: Full implementation of HTTP request/response cycle with persistent connections

---

## ✨ Features

### Core Functionality

- ✅ **HTTP/1.1 Protocol**: Full support for GET, POST, and DELETE methods
- ✅ **Persistent Connections**: Keep-alive support for efficient resource usage
- ✅ **Virtual Hosts**: Multiple server configurations on different ports
- ✅ **Epoll I/O Multiplexing**: Efficient handling of concurrent connections

### Dynamic Content

- 🐍 **CGI Support**: Execute Python, PHP, and Shell scripts
  - Environment variable passing
  - POST data handling
  - Query string processing
  - Timeout management

### File Management

- 📤 **File Uploads**: Multipart form-data parsing with size limits
- 📁 **Directory Listing**: Automatic index generation with autoindex
- 🎨 **Custom Error Pages**: Server-specific error page configuration

### Routing & Redirection

- 🔀 **Location-based Routing**: Nginx-style location blocks
- ↪️ **URL Redirects**: HTTP 301/302 redirects via `return` directive
- 📝 **Method Restrictions**: Per-location allowed methods

---

## 🔧 Requirements

### System Requirements

- **Operating System**: Ubuntu/Linux (tested on Ubuntu 24)
- **Compiler**: g++ with C++98 support
- **Build System**: GNU Make

### Required Packages

```bash
# Install required interpreters for CGI
sudo apt-get update
sudo apt-get install python3 php-cli
```

### Optional Tools

```bash
# For testing
sudo apt-get install curl
```

---

## 📦 Installation

### 1. Clone the Repository

```bash
git clone <repository-url>
cd webserv
```

### 2. Compile

```bash
make        # Compile the project
make clean  # Remove object files
make fclean # Remove object files and executable
make re     # Recompile from scratch
```

### 3. Verify Installation

```bash
./webserv --help  # Should display usage information
```

---

## ⚙️ Configuration

WebServ uses nginx-style configuration files with `.conf` extension.

### Basic Configuration Structure

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

### Configuration Directives

#### Server Block Directives

| Directive | Scope | Description | Example |
|-----------|-------|-------------|---------|
| `listen` | Server | Port and optional interface to bind | `listen 8080;`<br>`listen 127.0.0.1:8080;` |
| `server_name` | Server | Virtual host names | `server_name example.com www.example.com;` |
| `error_page` | Server | Custom error page mapping | `error_page 404 /errors/404.html;` |

#### Location Block Directives

| Directive | Scope | Description | Example |
|-----------|-------|-------------|---------|
| `return` | Location | HTTP redirect | `return 301 /new-page;` |
| `cgi_pass` | Location | CGI interpreter path | `cgi_pass /usr/bin/python3;` |
| `cgi_extension` | Location | CGI file extensions | `cgi_extension .py .php .sh;` |
| `upload_store` | Location | File upload directory | `upload_store /var/www/uploads;` |
| `allowed_methods` | Location | Permitted HTTP methods | `allowed_methods GET POST DELETE;` |

#### Inheritable Directives (Server & Location)

| Directive | Description | Default | Example |
|-----------|-------------|---------|---------|
| `root` | Document root directory | Required | `root /var/www/html;` |
| `index` | Default index files | `index.html` | `index index.html index.htm;` |
| `autoindex` | Directory listing | `off` | `autoindex on;` |
| `client_max_body_size` | Max request body size | 1M | `client_max_body_size 100M;` |

### Complete Example Configuration

```nginx
server {
    listen 8080;
    server_name localhost;
    
    root /var/www/html;
    index index.html index.htm;
    autoindex off;
    client_max_body_size 10M;
    
    error_page 404 /errors/404.html;
    error_page 500 502 503 504 /errors/50x.html;
    
    # Static files
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
    
    # Redirect old URLs
    location /old-page {
        return 301 /new-page.html;
    }
}

# Additional virtual host
server {
    listen 9090;
    server_name test.local;
    root /var/www/test;
    index index.html;
}
```

### Configuration Inheritance

Location blocks inherit settings from their server block:

```nginx
server {
    root /var/www/html;              # Server-level
    client_max_body_size 10M;        # Server-level
    
    location /api/ {
        # Inherits: root, client_max_body_size
        allowed_methods GET POST;     # Location-specific
    }
    
    location /uploads/ {
        client_max_body_size 100M;    # Overrides server-level
    }
}
```

---

## 🚀 Usage

### Starting the Server

```bash
./webserv <config_file.conf>
```

**Example:**

```bash
./webserv /path/to/server.conf
```

### Server Output

```
✓ Configuration parsed successfully!
✓ Router initialized
✓ File server initialized
✓ CGI handler initialized
✓ Upload handler initialized
✓ Listening on 0.0.0.0:8080
✓ Listening on 0.0.0.0:9090

=== Server is running. Press Ctrl+C to stop ===
```

### Making Requests

#### Static Files

```bash
# GET request
curl http://localhost:8080/

# View headers
curl -I http://localhost:8080/index.html

# Download file
curl -O http://localhost:8080/downloads/file.pdf
```

#### CGI Scripts

```bash
# Python CGI with query string
curl "http://localhost:8080/cgi-bin/calc.py?a=10&b=5&op=add"

# PHP CGI with POST data
curl -X POST -d "name=John&email=john@example.com" \
  http://localhost:8080/cgi-bin/form.php

# Shell script
curl http://localhost:8080/cgi-bin/sysinfo.sh
```

#### File Uploads

```bash
# Single file upload
curl -X POST -F "file=@document.pdf" \
  http://localhost:8080/upload

# Multiple files
curl -X POST \
  -F "file1=@image1.jpg" \
  -F "file2=@image2.jpg" \
  http://localhost:8080/upload

# With additional form fields
curl -X POST \
  -F "file=@data.csv" \
  -F "description=Sales data" \
  -F "category=reports" \
  http://localhost:8080/upload
```

### Stopping the Server

Press `Ctrl+C` for graceful shutdown:

```
^C
Received shutdown signal...
Shutting down...
✓ Server stopped gracefully
```

---

## 🧪 Testing

### Test Suite Setup

WebServ includes a comprehensive test suite for validating all functionality.

#### 1. Setup Test Environment

```bash
# Make setup script executable
chmod +x setup.sh

# Run setup (creates /tmp/www with test files)
./setup.sh
```

#### 2. Start Test Server

```bash
./webserv /tmp/www/test.conf
```

#### 3. Access Test Dashboard

Open your browser to: `http://localhost:8080/`

### Test Features

The test suite includes:

#### Static File Serving
- ✅ HTML files
- ✅ Text files
- ✅ JSON files
- ✅ Directory listing (autoindex)
- ✅ Custom error pages

#### CGI Testing
- ✅ Python scripts (hello, environment, calculator, forms)
- ✅ PHP scripts (info, time, query parsing, forms)
- ✅ Shell scripts (hello, system info, date/time)
- ✅ GET with query strings
- ✅ POST with form data
- ✅ POST with raw body

#### File Upload Testing
- ✅ Single file upload
- ✅ Multiple file upload
- ✅ Multipart form data with fields
- ✅ Size limit validation (413 error)

#### HTTP Features
- ✅ Method restrictions (405 error)
- ✅ URL redirects (301/302)
- ✅ Keep-alive connections
- ✅ Request timeouts

### Manual Testing with cURL

```bash
# Test static file
curl http://localhost:8080/test.txt

# Test directory listing
curl http://localhost:8080/images/

# Test CGI
curl "http://localhost:8080/cgi-bin/calc.py?a=100&b=25&op=div"

# Test POST to CGI
curl -X POST -d "name=Test&message=Hello" \
  http://localhost:8080/cgi-bin/form.py

# Test file upload
curl -X POST -F "file=@testfile.txt" \
  http://localhost:8080/upload

# Test 404 error
curl http://localhost:8080/nonexistent

# Test redirect
curl -L http://localhost:8080/old-page

# Verbose output
curl -v http://localhost:8080/
```

### Test Configuration

The test configuration (`/tmp/www/test.conf`) includes:

- Main server on port 8080
- Virtual host `test.local` on port 8080
- Alternative server on port 9090
- CGI support for `.py`, `.php`, `.sh`
- File upload with 100MB limit
- Custom error pages
- Directory autoindex enabled

---

## 🏗️ Architecture & Concepts

### HTTP Protocol

**HTTP (Hypertext Transfer Protocol)** is the foundation of web communication.

#### Request/Response Cycle

```
Client                          Server
  |                               |
  |  1. TCP Connection            |
  |------------------------------>|
  |                               |
  |  2. HTTP Request              |
  |  GET /index.html HTTP/1.1     |
  |  Host: localhost              |
  |------------------------------>|
  |                               |
  |  3. HTTP Response             |
  |  HTTP/1.1 200 OK              |
  |  Content-Type: text/html      |
  |  <html>...</html>             |
  |<------------------------------|
  |                               |
  |  4. Connection Close/Keep-alive
```

#### HTTP Request Structure

```
GET /path?query=value HTTP/1.1          ← Request Line
Host: localhost:8080                     ← Headers
User-Agent: curl/7.68.0
Accept: */*
                                         ← Blank Line
[Request Body for POST/PUT]              ← Body (optional)
```

#### HTTP Response Structure

```
HTTP/1.1 200 OK                          ← Status Line
Content-Type: text/html                  ← Headers
Content-Length: 1234
Connection: keep-alive

<html>...</html>                         ← Body
```

### Socket Programming

**Sockets** provide an endpoint for network communication.

#### TCP Socket Flow

```
Server Side                    Client Side
----------                     -----------
socket()                       socket()
  |                              |
bind()                           |
  |                              |
listen()                         |
  |                              |
accept() -------- connect() -----+
  |                              |
recv/send <------- send/recv ----+
  |                              |
close()                        close()
```

#### Key Socket Operations

```cpp
// 1. Create socket
int sockfd = socket(AF_INET, SOCK_STREAM, 0);

// 2. Bind to address:port
bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));

// 3. Listen for connections
listen(sockfd, backlog);

// 4. Accept client connection
int client_fd = accept(sockfd, ...);

// 5. Read/Write data
recv(client_fd, buffer, size, 0);
send(client_fd, data, size, 0);

// 6. Close connection
close(client_fd);
```

#### Non-Blocking Sockets

WebServ uses **non-blocking sockets** to avoid blocking the entire server:

```cpp
// Set socket to non-blocking mode
int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);

// Read returns immediately (even if no data)
ssize_t n = recv(fd, buffer, size, 0);
if (n < 0 && errno == EAGAIN) {
    // No data available, try again later
}
```

### Epoll: Event-Driven I/O

**Epoll** is Linux's scalable I/O event notification mechanism.

#### Why Epoll?

Traditional approaches don't scale:

| Method | Max FDs | Performance | Complexity |
|--------|---------|-------------|------------|
| `select()` | 1024 | O(n) | High |
| `poll()` | Unlimited | O(n) | Medium |
| **`epoll()`** | **Unlimited** | **O(1)** | **Low** |

#### Epoll Architecture

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

#### Epoll Operations

```cpp
// 1. Create epoll instance
int epfd = epoll_create1(0);

// 2. Add socket to monitor
struct epoll_event ev;
ev.events = EPOLLIN | EPOLLOUT;  // Monitor read/write
ev.data.fd = socket_fd;
epoll_ctl(epfd, EPOLL_CTL_ADD, socket_fd, &ev);

// 3. Wait for events (non-blocking)
struct epoll_event events[MAX_EVENTS];
int nfds = epoll_wait(epfd, events, MAX_EVENTS, timeout);

// 4. Process events
for (int i = 0; i < nfds; i++) {
    if (events[i].events & EPOLLIN) {
        // Socket ready to read
    }
    if (events[i].events & EPOLLOUT) {
        // Socket ready to write
    }
}
```

#### Event-Driven Flow in WebServ

```
1. epoll_wait() returns events
       |
       ├─ Listen socket ready? → accept() new client
       |
       ├─ Client socket readable? → read HTTP request
       |                           → parse request
       |                           → process (file/CGI/upload)
       |                           → switch to write mode
       |
       └─ Client socket writable? → write HTTP response
                                   → keep-alive or close
```

### CGI (Common Gateway Interface)

**CGI** enables web servers to execute external programs and return dynamic content.

#### CGI Architecture

```
Browser                 WebServ                CGI Script
   |                       |                        |
   | HTTP Request          |                        |
   |---------------------->|                        |
   |                       |                        |
   |                       | fork() + execve()      |
   |                       |----------------------->|
   |                       |                        |
   |                       | Environment Variables  |
   |                       | + Request Body         |
   |                       |----------------------->|
   |                       |                        |
   |                       |        Output          |
   |                       |<-----------------------|
   |                       |                        |
   |     HTTP Response     |                        |
   |<----------------------|                        |
```

#### CGI Environment Variables

WebServ passes HTTP request information via environment variables:

```bash
# Request Information
REQUEST_METHOD=POST
REQUEST_URI=/cgi-bin/form.py
QUERY_STRING=name=John&age=25
PATH_INFO=/extra/path

# Server Information
SERVER_NAME=localhost
SERVER_PORT=8080
SERVER_PROTOCOL=HTTP/1.1
SERVER_SOFTWARE=webserv/1.0

# Client Information
REMOTE_ADDR=127.0.0.1
REMOTE_PORT=54321

# Content Information
CONTENT_TYPE=application/x-www-form-urlencoded
CONTENT_LENGTH=42

# HTTP Headers (converted to HTTP_*)
HTTP_USER_AGENT=Mozilla/5.0
HTTP_ACCEPT=text/html
HTTP_HOST=localhost:8080
```

#### CGI Process Flow

```cpp
// 1. Create pipes for communication
pipe(stdin_pipe);   // Parent writes, child reads
pipe(stdout_pipe);  // Child writes, parent reads

// 2. Fork process
pid_t pid = fork();

if (pid == 0) {
    // Child process
    
    // Redirect stdin/stdout to pipes
    dup2(stdin_pipe[0], STDIN_FILENO);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    
    // Execute CGI script
    execve("/usr/bin/python3", argv, envp);
}
else {
    // Parent process
    
    // Send request body to CGI
    write(stdin_pipe[1], request_body, size);
    close(stdin_pipe[1]);
    
    // Read CGI output
    read(stdout_pipe[0], response_buffer, size);
    
    // Wait for CGI to finish
    waitpid(pid, &status, 0);
}
```

#### CGI Output Format

CGI scripts must output headers followed by body:

```
Content-Type: text/html
Status: 200 OK
X-Custom-Header: value
                              ← Blank line separates headers from body
<!DOCTYPE html>
<html>
<body>Dynamic content</body>
</html>
```

#### CGI in WebServ

```cpp
// Detect CGI request
if (path.endsWith(".py") || path.endsWith(".php")) {
    
    // Build environment
    vector<string> env = buildCgiEnvironment(request);
    
    // Execute CGI
    CgiResult result = cgiHandler.execute(
        scriptPath,
        request.getBody(),
        env,
        timeout
    );
    
    // Parse CGI output
    parseHeaders(result.output);
    
    // Send response
    sendResponse(result.statusCode, result.body);
}
```

### Request Parsing

WebServ implements a **state machine** for incremental HTTP parsing:

```
States:
1. PARSE_REQUEST_LINE → GET /path HTTP/1.1
2. PARSE_HEADERS      → Header: Value\r\n
3. PARSE_BODY         → [body content]
4. PARSE_COMPLETE     → Ready to process

Transitions:
- Incomplete data → Return PARSE_INCOMPLETE (wait for more)
- Valid data      → Advance to next state
- Invalid data    → Return PARSE_FAILED (400 Bad Request)
```

#### Chunked Transfer Encoding

For requests with `Transfer-Encoding: chunked`:

```
5\r\n              ← Chunk size (hex)
Hello\r\n          ← Chunk data
6\r\n
World!\r\n
0\r\n              ← Last chunk (size 0)
\r\n               ← Trailer
```

### Response Building

```cpp
Response response;

// Set status
response.setStatusCode(200);
response.setStatusText("OK");

// Set headers
response.setContentType("text/html");
response.setHeader("Content-Length", "1234");
response.setHeader("Connection", "keep-alive");

// Set body
response.setBody("<html>...</html>");

// Build HTTP response
string http_response = response.build();
// → "HTTP/1.1 200 OK\r\n"
//    "Content-Type: text/html\r\n"
//    "Content-Length: 1234\r\n"
//    "\r\n"
//    "<html>...</html>"
```

### File Upload Handling

#### Multipart Form Data

File uploads use `multipart/form-data` encoding:

```
POST /upload HTTP/1.1
Content-Type: multipart/form-data; boundary=----Boundary1234

------Boundary1234
Content-Disposition: form-data; name="description"

File description here
------Boundary1234
Content-Disposition: form-data; name="file"; filename="document.pdf"
Content-Type: application/pdf

[Binary file data]
------Boundary1234--
```

#### Parsing Flow

```cpp
// 1. Extract boundary from Content-Type
string boundary = extractBoundary(contentType);

// 2. Split body by boundary
vector<Part> parts = parseMultipart(body, boundary);

// 3. Process each part
for (Part part : parts) {
    if (part.isFile) {
        // Save file to disk
        saveFile(part.filename, part.data);
    } else {
        // Regular form field
        formData[part.name] = part.value;
    }
}
```

### Virtual Hosts

Multiple servers can share the same port but serve different content:

```cpp
// Request arrives on port 8080
// Host header: "example.com"

// 1. Find servers listening on port 8080
vector<Server*> candidates = findByPort(8080);

// 2. Match Host header
Server* server = matchByServerName(candidates, "example.com");

// 3. Route request using matched server's configuration
```

---

## 📚 Resources

### CGI Documentation

- [CGI: Getting Started](https://www.w3.org/CGI/)
- [CGI 1.1 Specification](https://datatracker.ietf.org/doc/html/rfc3875)

### Socket & Network Programming

**Unix Network Programming, Volume 1: The Sockets Networking API** by W. Richard Stevens

Essential chapters:
- Chapter 4: Elementary TCP Sockets
- Chapter 5: TCP Client/Server Example
- Chapter 6: I/O Multiplexing (`select`, `poll`, `epoll`)
- Chapter 16: Nonblocking I/O
- Chapter 8: Elementary UDP Sockets (optional)

### HTTP Protocol

**HTTP: The Definitive Guide** by David Gourley & Brian Totty

Key chapters:
- Chapter 1: Overview of HTTP
- Chapter 3: HTTP Messages
- Chapter 4: Connection Management
- Chapter 9: Web Robots (proper server behavior)
- Chapter 17: Content Negotiation and Transcoding
- Chapter 20: Redirection and Load Balancing

### Additional Resources

- [RFC 2616 - HTTP/1.1](https://www.rfc-editor.org/rfc/rfc2616)
- [RFC 7230-7235 - HTTP/1.1 (Updated)](https://www.rfc-editor.org/rfc/rfc7230)
- [Epoll Tutorial](https://man7.org/linux/man-pages/man7/epoll.7.html)
- [Nginx Documentation](https://nginx.org/en/docs/) (configuration reference)

---

## 📸 Screenshots

### Test Dashboard

*[Screenshot placeholder: Main test dashboard showing all test categories]*

![Test Dashboard](docs/images/dashboard.png)

### CGI Execution

*[Screenshot placeholder: CGI script output examples]*

![CGI Examples](docs/images/cgi-examples.png)

### File Upload Interface

*[Screenshot placeholder: File upload form and success page]*

![File Upload](docs/images/upload.png)

### Server Console Output

*[Screenshot placeholder: Terminal showing server startup and request logs]*

![Console Output](docs/images/console.png)

---

## 🤝 Contributing

This project is part of the 42 school curriculum. While it's primarily an educational project, suggestions and feedback are welcome.

---

## 📄 License

This project is part of 42 school curriculum and follows their academic policies.

---

## 👥 Authors

Developed as a team project at 42 School.

---

## 🙏 Acknowledgments

- 42 School for the project specifications
- The authors of the referenced technical books
- The open-source community for HTTP and networking standards

---

**Happy coding! 🚀**