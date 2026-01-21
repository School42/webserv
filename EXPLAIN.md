# Web Server Technical Architecture

## Overview

A high-performance, event-driven HTTP/1.1 web server written in C++98 that handles multiple concurrent connections using epoll-based I/O multiplexing. Supports static file serving, CGI execution, file uploads, and DELETE operations.

**Key Features:**
- Non-blocking I/O with epoll
- Virtual host routing
- CGI with non-blocking pipe I/O
- Configurable via nginx-like syntax
- Keep-alive connections
- Multipart file uploads

---

## Architecture Flow

```
Config File → Lexer → Parser → ServerConfig
                                     ↓
                              Server Setup
                                     ↓
                    ┌────────────────┴────────────────┐
                    ↓                                  ↓
            Listen Sockets                        Epoll Loop
                    ↓                                  ↓
            Accept Connection                    Event Dispatch
                    ↓                                  ↓
              Create Client                      ┌────┴────┐
                    ↓                            ↓         ↓
            Add to ClientManager          Listen Socket  Client
                                                 ↓         ↓
                                          Accept New   Read/Write
                                                       ↓
                                              Parse HTTP Request
                                                       ↓
                                                   Router
                                                       ↓
                                    ┌──────────────────┼──────────────────┐
                                    ↓                  ↓                  ↓
                              Static File            CGI            File Upload
                                    ↓                  ↓                  ↓
                              FileServer         CgiHandler       UploadHandler
                                    ↓                  ↓                  ↓
                                Response          Response           Response
                                    └──────────────────┴──────────────────┘
                                                       ↓
                                              Send to Client
```

---

## 1. Configuration Parsing

### 1.1 Lexer (Tokenization)

**Purpose:** Convert configuration text into tokens.

**Implementation:** `Lexer.cpp`

**Process:**
```cpp
Input: "server { listen 8080; }"

Tokens:
- TOK_IDENT("server")
- TOK_LBRACE("{")
- TOK_IDENT("listen")
- TOK_IDENT("8080")
- TOK_SEMICOLON(";")
- TOK_RBRACE("}")
- TOK_EOF
```

**Features:**
- Handles quoted strings with escape sequences
- Supports comments (`#`)
- Tracks line/column for error reporting
- Distinguishes identifiers, braces, semicolons

**Key Methods:**
- `nextToken()` - Returns next token
- `peekToken()` - Looks ahead without consuming
- `lexQuotedString()` - Handles `"..."` and `'...'`

### 1.2 Parser (Syntax Analysis)

**Purpose:** Convert tokens into configuration objects with validation.

**Implementation:** `Parser.cpp`

**Grammar:**
```
config     := server_block+
server     := "server" "{" directive* location* "}"
location   := "location" path "{" directive* "}"
directive  := name value+ ";"
```

**Key Classes:**
- `ServerConfig` - Top-level server configuration
- `LocationConfig` - Location block configuration
- `DirectiveSpec` - Directive metadata (scope, arity)

**Validation:**
- Duplicate directive detection
- Type checking (int, size, IP address)
- Scope enforcement (server-only vs location-only)
- Inheritance resolution (location inherits from server)

**Example:**
```nginx
server {
    listen 8080;
    server_name localhost;
    root /var/www;
    
    location /api {
        allowed_methods GET POST;
        cgi_extension .py;
    }
}
```

Parsed into:
```cpp
ServerConfig {
    listen: [8080]
    server_names: ["localhost"]
    root: "/var/www"
    locations: [
        LocationConfig {
            path: "/api"
            allowed_methods: ["GET", "POST"]
            cgi_extension: [".py"]
            root: "/var/www" (inherited)
        }
    ]
}
```

---

## 2. Socket & Epoll Setup

### 2.1 Socket Class

**Purpose:** Abstraction over sockets.

**Implementation:** `Socket.cpp`

**Key Operations:**
```cpp
Socket sock;
sock.setReuseAddr(true);      // SO_REUSEADDR
sock.setNonBlocking(true);     // O_NONBLOCK
sock.bind("0.0.0.0", 8080);   // Bind to address
sock.listen(128);              // Start listening
int clientFd = sock.accept(addr, port);
```

**Features:**
- Non-blocking mode
- Address reuse
- IPv4 support

### 2.2 Epoll (Event Multiplexing)

**Purpose:** Monitor multiple file descriptors efficiently.

**Implementation:** `Epoll.cpp`

**API:**
```cpp
Epoll epoll;
epoll.add(fd, EVENT_READ | EVENT_WRITE);
epoll.modify(fd, EVENT_READ);
epoll.remove(fd);

std::vector<Event> events;
int count = epoll.wait(events, timeout_ms);
for (int i = 0; i < count; i++) {
    if (events[i].isReadable()) { /* read */ }
    if (events[i].isWritable()) { /* write */ }
    if (events[i].isError()) { /* error */ }
}
```

**Event Types:**
- `EPOLLIN` - Readable
- `EPOLLOUT` - Writable
- `EPOLLERR` - Error
- `EPOLLHUP` - Hangup
- `EPOLLRDHUP` - Peer closed

**Advantages over select/poll:**
- O(1) performance (not O(n))
- No FD_SETSIZE limit
- Edge-triggered option available

---

## 3. Server Main Loop

### 3.1 Server Class

**Purpose:** Orchestrate all components.

**Implementation:** `Server.cpp`

**Initialization:**
```cpp
Server::Server(configs) {
    // Setup listen sockets for all server blocks
    for (each config) {
        for (each listen address) {
            Socket* sock = new Socket();
            sock->bind(interface, port);
            sock->listen();
            epoll.add(sock->getFd(), EVENT_READ);
        }
    }
}
```

**Event Loop:**
```cpp
void Server::eventLoop() {
    while (running) {
        epoll.wait(events, timeout);
        
        for (each event) {
            if (isListenSocket(fd)) {
                handleNewConnection();
            } else if (isCgiPipe(fd)) {
                handleCgiEvent();  // Non-blocking CGI I/O
            } else if (isClient(fd)) {
                handleClientEvent();
            }
        }
        
        checkTimeouts();
        checkCgiTimeouts();
    }
}
```

### 3.2 Client Management

**Purpose:** Track client connections and their state.

**Implementation:** `Client.cpp`, `ClientManager.cpp`

**Client States:**
```cpp
enum ClientState {
    STATE_READING_REQUEST,    // Reading HTTP headers/body
    STATE_PROCESSING,         // Processing (CGI running)
    STATE_WRITING_RESPONSE,   // Sending response
    STATE_DONE               // Complete (close or keep-alive)
};
```

**Client Lifecycle:**
```
Accept → STATE_READING_REQUEST → Parse HTTP
                ↓
        STATE_PROCESSING → Route & Handle
                ↓
        STATE_WRITING_RESPONSE → Send Response
                ↓
        STATE_DONE → Close or Keep-Alive
```

**Keep-Alive Support:**
```cpp
// After response sent:
if (client->isKeepAlive() && requestCount < MAX) {
    client->reset();  // Reuse connection
    epoll.modify(fd, EVENT_READ);
} else {
    clientManager.removeClient(fd);
}
```

---

## 4. HTTP Request Parsing

### 4.1 HttpRequest Class

**Purpose:** Incremental HTTP/1.1 parser (handles chunked encoding).

**Implementation:** `HttpRequest.cpp`

**Parsing States:**
```cpp
enum HttpParseState {
    PARSE_REQUEST_LINE,     // "GET /path HTTP/1.1"
    PARSE_HEADERS,          // "Host: localhost"
    PARSE_BODY,             // Fixed Content-Length body
    PARSE_CHUNKED_SIZE,     // Chunk size line
    PARSE_CHUNKED_DATA,     // Chunk data
    PARSE_COMPLETE,         // Done
    PARSE_ERROR            // Invalid
};
```

**Incremental Parsing:**
```cpp
HttpRequest req;
while (receiving) {
    size_t consumed;
    HttpParseResult result = req.parse(buffer, consumed);
    buffer.erase(0, consumed);
    
    if (result == PARSE_SUCCESS) break;
    if (result == PARSE_FAILED) return error;
    // PARSE_INCOMPLETE - need more data
}
```

**Features:**
- Handles chunked transfer encoding
- Content-Length validation
- Header size limits
- Case-insensitive header lookup
- Keep-alive detection

**Example:**
```
Request:
GET /api/users?limit=10 HTTP/1.1
Host: example.com
Content-Length: 13

{"key":"val"}

Parsed:
method: "GET"
uri: "/api/users?limit=10"
path: "/api/users"
queryString: "limit=10"
headers: {"host": "example.com", "content-length": "13"}
body: "{\"key\":\"val\"}"
```

---

## 5. Routing

### 5.1 Router Class

**Purpose:** Match request to server and location configuration.

**Implementation:** `Router.cpp`

**Process:**
```cpp
RouteResult Router::route(request, listenPort) {
    // 1. Find server by Host header + port
    server = findServer(request.getHost(), listenPort);
    
    // 2. Normalize and decode path
    path = normalizePath(urlDecode(request.getPath()));
    
    // 3. Find location (longest prefix match)
    location = findLocation(server, path);
    
    // 4. Check method allowed
    if (!isMethodAllowed(location, request.getMethod()))
        return 405;
    
    // 5. Resolve filesystem path
    resolvedPath = location.root + (path - location.path);
    
    return RouteResult{server, location, resolvedPath};
}
```

**Server Selection:**
```cpp
1. Listen on port? → candidate
2. server_name matches Host? → exact match
3. No match → use first server (default)
```

**Location Matching (Longest Prefix):**
```
Locations: ["/", "/api", "/api/users"]
Request: "/api/users/123"
Match: "/api/users" (longest prefix)
```

**Path Resolution:**
```
location /api/ {
    root /var/www;
}

Request: /api/users/1
Resolved: /var/www/users/1
```

---

## 6. Request Handlers

### 6.1 FileServer

**Purpose:** Serve static files and directory listings.

**Implementation:** `FileServer.cpp`

**Flow:**
```cpp
FileResult FileServer::serveFile(request, route) {
    path = route.resolvedPath;
    
    if (!exists(path))
        return 404;
    
    if (isDirectory(path)) {
        if (!path.endsWith("/"))
            return redirect(path + "/");
        
        // Try index files
        for (each indexFile) {
            if (exists(path + indexFile))
                return serve(path + indexFile);
        }
        
        // Autoindex enabled?
        if (location.autoindex)
            return generateDirectoryListing(path);
        else
            return 403;
    }
    
    // Serve file
    content = readFile(path);
    mimeType = getMimeType(path);
    return FileResult{200, mimeType, content};
}
```

**MIME Type Detection:**
```cpp
map[".html"] = "text/html"
map[".css"]  = "text/css"
map[".js"]   = "text/javascript"
map[".png"]  = "image/png"
// ... 50+ types
```

**Directory Listing:**
```html
<!DOCTYPE html>
<html>
<head><title>Index of /path</title></head>
<body>
  <h1>Index of /path</h1>
  <table>
    <tr><th>Name</th><th>Size</th><th>Modified</th></tr>
    <tr><td><a href="file.txt">file.txt</a></td><td>1.2KB</td><td>2024-01-15</td></tr>
  </table>
</body>
</html>
```

### 6.2 CGI Handler (Non-Blocking)

**Purpose:** Execute CGI scripts without blocking server.

**Implementation:** `CgiHandler.cpp`, integrated into `Server.cpp`

**Architecture:**
```
Client Request
     ↓
startCgiSession()
     ↓
fork() → CGI Process
     ↓
Create pipes (stdin/stdout)
     ↓
Add pipes to epoll
     ↓
[Server continues serving other requests]
     ↓
Epoll events on pipes:
  - stdout readable → read and accumulate
  - stdin writable → write POST data
  - stdout EOF → parse output, send response
     ↓
finalizeCgiSession()
     ↓
Response to client
```

**Non-Blocking Flow:**
```cpp
// 1. Start CGI (non-blocking)
CgiStartResult result = cgiHandler.startNonBlocking(request, route, ...);
if (!result.success) return error;

// 2. Create session
CgiSession session;
session.pid = result.pid;
session.stdoutFd = result.stdoutFd;
session.stdinFd = result.stdinFd;
session.inputBuffer = request.getBody();

// 3. Add pipes to epoll
epoll.add(session.stdoutFd, EVENT_READ);
if (hasInput)
    epoll.add(session.stdinFd, EVENT_WRITE);

// 4. Store session
_cgiSessions[session.stdoutFd] = session;

// 5. Continue event loop
return;  // Don't block!

// ... Later, in event loop ...

// 6. Handle stdout readable
if (event.fd == session.stdoutFd && event.isReadable()) {
    read(fd, buffer, size);
    session.outputBuffer += buffer;
    
    if (read() == 0) {  // EOF
        finalizeCgiSession(fd);
    }
}

// 7. Finalize: parse output, send response
void finalizeCgiSession(fd) {
    parseCgiOutput(session.outputBuffer, headers, body, status);
    buildResponse(status, headers, body);
    sendToClient(response);
    cleanupCgiSession(fd);
}
```

**Environment Variables:**
```cpp
GATEWAY_INTERFACE=CGI/1.1
SERVER_PROTOCOL=HTTP/1.1
REQUEST_METHOD=POST
SERVER_PORT=8080
SERVER_NAME=localhost
SCRIPT_NAME=/cgi-bin/script.py
SCRIPT_FILENAME=/var/www/cgi-bin/script.py
QUERY_STRING=foo=bar
REQUEST_URI=/cgi-bin/script.py?foo=bar
CONTENT_LENGTH=13
CONTENT_TYPE=application/json
HTTP_HOST=localhost:8080
HTTP_USER_AGENT=curl/7.68.0
PATH=/usr/local/bin:/usr/bin:/bin
```

**CGI Output Parsing:**
```
CGI Output:
Content-Type: text/html
Status: 200 OK

<html>...</html>

Parsed:
statusCode: 200
statusText: "OK"
headers: {"Content-Type": "text/html"}
body: "<html>...</html>"
```

**Benefits of Non-Blocking CGI:**
- Server handles other requests during CGI execution
- Multiple CGI scripts run concurrently
- 30-second timeout protection
- 10MB output size limit

### 6.3 Upload Handler

**Purpose:** Handle multipart/form-data file uploads.

**Implementation:** `UploadHandler.cpp`

**Multipart Parsing:**
```
Input:
------WebKitFormBoundary
Content-Disposition: form-data; name="file"; filename="test.txt"
Content-Type: text/plain

File content here
------WebKitFormBoundary--

Parsed:
MultipartPart {
    name: "file"
    filename: "test.txt"
    contentType: "text/plain"
    data: "File content here"
}
```

**Upload Flow:**
```cpp
1. Detect multipart: Content-Type contains "multipart/form-data"
2. Extract boundary from Content-Type
3. Parse body into parts
4. For each file part:
   - Sanitize filename (remove path, dangerous chars)
   - Generate unique filename if exists
   - Save to upload_store directory
5. Return list of uploaded files
```

**Security:**
- Filename sanitization (no `../`, no leading `.`)
- Size limits (client_max_body_size)
- Type validation
- Upload directory validation

---

## 7. Response Building

### 7.1 Response Class

**Purpose:** Build HTTP responses.

**Implementation:** `Response.cpp`

**Structure:**
```cpp
Response response;
response.setStatusCode(200);
response.setStatusText("OK");
response.setContentType("text/html");
response.setBody("<html>...</html>");
response.setHeader("Server", "webserv/1.0");
response.setKeepAlive(true);

string httpResponse = response.build();
```

**Output:**
```
HTTP/1.1 200 OK
Content-Type: text/html
Content-Length: 27
Connection: keep-alive
Server: webserv/1.0

<html>...</html>
```

**Factory Methods:**
```cpp
Response::ok(body, contentType);
Response::created(body, contentType);
Response::redirect(code, location);
Response::error(code, message);
```

**Custom Error Pages:**
```nginx
server {
    error_page 404 /errors/404.html;
    error_page 500 /errors/50x.html;
}
```

---

## 8. Key Design Patterns

### 8.1 Non-Blocking I/O

**Problem:** Blocking I/O halts entire server.

**Solution:** 
1. Set sockets to O_NONBLOCK
2. Use epoll to wait for events
3. Handle EAGAIN/EWOULDBLOCK gracefully
4. Process incrementally (partial reads/writes)

**Example:**
```cpp
// Bad (blocking)
read(fd, buffer, size);  // Blocks until data available

// Good (non-blocking)
epoll.wait(events);
if (event.isReadable()) {
    ssize_t n = read(fd, buffer, size);
    if (n > 0) {
        // Process n bytes
    } else if (n == 0) {
        // EOF
    } else {
        // Would block - wait for next event
    }
}
```

### 8.2 State Machines

**Client State Machine:**
```
STATE_READING_REQUEST:
    if (can read)
        read data
        parse incrementally
        if (complete) → STATE_PROCESSING
    
STATE_PROCESSING:
    route request
    generate response
    → STATE_WRITING_RESPONSE

STATE_WRITING_RESPONSE:
    if (can write)
        write data
        if (complete) → STATE_DONE

STATE_DONE:
    if (keep-alive)
        reset() → STATE_READING_REQUEST
    else
        close connection
```

### 8.3 Resource Management (RAII)

**Socket:**
```cpp
class Socket {
    int _fd;
public:
    Socket() : _fd(socket(...)) {}
    ~Socket() { if (_fd >= 0) close(_fd); }
};
```

**Epoll:**
```cpp
class Epoll {
    int _epollFd;
public:
    Epoll() : _epollFd(epoll_create1(0)) {}
    ~Epoll() { if (_epollFd >= 0) close(_epollFd); }
};
```

---

## 9. Performance Characteristics

### 9.1 Latency

- Connection accept: <1ms
- Static file: <5ms (for small files)
- CGI startup: ~10ms (fork overhead)
- CGI execution: depends on script

---

## 10. Error Handling

### 10.1 Configuration Errors

```cpp
try {
    Lexer lexer(config);
    Parser parser(lexer);
    servers = parser.parse();
} catch (ConfigError& e) {
    cerr << e.formatMessage() << endl;
    // "Config error at line 5, column 12: Invalid directive"
    exit(1);
}
```

### 10.2 Runtime Errors

**Socket Errors:**
```cpp
try {
    socket.bind(address, port);
} catch (SocketError& e) {
    cerr << "Failed to bind: " << e.what() << endl;
}
```

**Epoll Errors:**
```cpp
try {
    epoll.add(fd, events);
} catch (EpollError& e) {
    cerr << "Epoll error: " << e.what() << endl;
}
```

**HTTP Errors:**
- 400 Bad Request - Malformed HTTP
- 404 Not Found - File doesn't exist
- 405 Method Not Allowed - Method not in allowed_methods
- 413 Payload Too Large - Exceeds client_max_body_size
- 500 Internal Server Error - Server fault
- 502 Bad Gateway - CGI failed
- 504 Gateway Timeout - CGI timed out

### 10.3 Timeout Handling

**Client Timeout:**
```cpp
// 60 seconds of inactivity
if (time(NULL) - client.lastActivity > 60) {
    clientManager.removeClient(client.fd);
}
```

**CGI Timeout:**
```cpp
// 30 seconds of execution
if (time(NULL) - session.startTime > 30) {
    kill(session.pid, SIGKILL);
    sendError(502, "Gateway Timeout");
}
```

---

## 11. Configuration Examples

### 11.1 Basic Static Server

```nginx
server {
    listen 8080;
    server_name localhost;
    root /var/www/html;
    index index.html index.htm;
    autoindex on;
}
```

### 11.2 Virtual Hosts

```nginx
server {
    listen 80;
    server_name site1.com;
    root /var/www/site1;
}

server {
    listen 80;
    server_name site2.com;
    root /var/www/site2;
}
```

### 11.3 CGI + Upload

```nginx
server {
    listen 8080;
    root /var/www;
    client_max_body_size 10M;
    
    location /cgi-bin/ {
        cgi_extension .py .pl .php;
        cgi_pass /usr/bin/python3;
        allowed_methods GET POST;
    }
    
    location /upload {
        allowed_methods POST DELETE;
        upload_store /var/www/uploads;
    }
}
```

### 11.4 Redirects

```nginx
server {
    listen 80;
    
    location /old-path {
        return 301 /new-path;
    }
    
    location /external {
        return 302 https://example.com;
    }
}
```

---

## 12. Testing Strategy

### 12.1 Unit Tests

- **Lexer:** Tokenization correctness
- **Parser:** Config validation
- **Router:** Path matching
- **HttpRequest:** Parsing edge cases

### 12.2 Integration Tests

- **Static files:** Various content types
- **CGI:** Different interpreters (Python, Perl, PHP)
- **Upload:** Multipart parsing
- **Keep-alive:** Multiple requests per connection

### 12.3 Load Tests

```bash
# Siege
siege -c 100 -r 100 http://localhost:8080/
```

### 12.4 Stress Tests

```bash
# Test concurrent CGI
for i in {1..50}; do
    curl http://localhost:8080/cgi-bin/slow.py &
done
wait

# Test file upload
curl -F "file=@bigfile.zip" http://localhost:8080/upload
```

---

## 13. Limitations & Future Work

### Current Limitations

1. **No HTTPS** - TLS not implemented
2. **IPv4 only** - No IPv6 support
3. **No HTTP/2** - Only HTTP/1.1
4. **No compression** - gzip/brotli not supported
5. **No caching** - ETag/If-Modified-Since not implemented
6. **No FastCGI** - Only traditional CGI

### Potential Improvements

1. **TLS Support:** Integrate OpenSSL
2. **HTTP/2:** Implement binary framing
3. **Compression:** Add gzip/deflate
4. **Caching:** Implement cache headers
5. **FastCGI:** Persistent process pooling
6. **WebSocket:** Upgrade connections
7. **Access Logs:** Apache-style logging
8. **Rate Limiting:** Per-IP connection limits
9. **Load Balancing:** Upstream proxy support
10. **Health Checks:** Monitor upstream servers

---

## 14. Summary

This web server demonstrates several key systems programming concepts:

- **Event-driven architecture** using epoll
- **Non-blocking I/O** for high concurrency
- **State machines** for connection management
- **Incremental parsing** for HTTP protocol
- **Process management** for CGI execution
- **Configuration DSL** with lexer/parser
- **Resource management** with RAII

The design prioritizes:
- **Performance:** O(1) event handling via epoll
- **Scalability:** Thousands of concurrent connections
- **Robustness:** Timeouts, limits, error handling
- **Maintainability:** Clean separation of concerns

**Core Technologies:**
- C++98 (maximum compatibility)
- Linux epoll (I/O multiplexing)
- BSD sockets (networking)
- POSIX (fork, pipe, signals)

**Total Lines of Code:** ~15,000 LOC
- Configuration: ~2,000 LOC
- Networking: ~3,000 LOC  
- HTTP: ~2,500 LOC
- Handlers: ~5,000 LOC
- Utils: ~2,500 LOC