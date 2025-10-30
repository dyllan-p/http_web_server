# Basic HTTP Web Server

## Description

A lightweight HTTP/1.1 web server implementation in C using Linux's epoll for event-driven I/O. Serves static HTML files from a local directory.

## Features

- Event-driven architecture using epoll
- Non-blocking I/O with edge-triggered notifications
- Concurrent connection handling
- Static file serving with chunked reads
- Basic HTTP/1.1 response generation
- Graceful handling of client disconnects

## Technical Implementation

The server uses Linux's epoll interface for I/O multiplexing, allowing it to handle multiple concurrent connections efficiently in a single thread. Sockets are configured as non-blocking with edge-triggered notifications (EPOLLET) to minimize system call overhead. File operations use chunked reads to support arbitrarily large files without memory constraints.

Key components:
- `setup_addrinfo()` - Configures address info hints for getaddrinfo
- `setup_server_socket()` - Socket initialization with SO_REUSEADDR
- `setup_epoll()` - Event loop managing client connections
- `do_use_fd()` - HTTP request parsing and response generation
- `setnonblocking()` - Configures file descriptors for non-blocking operation

## Requirements

- Linux kernel 2.6.27+ (for epoll_create1)
- GCC or compatible C compiler
- Root privileges (for binding to ports below 1024)

## Building

```bash
make
```

Or manually:

```bash
gcc -o httpd server.c -Wall -Wextra -O2
```

## Usage

### Running the Server

```bash
./httpd [PORT]
```

Example:

```bash
sudo ./httpd 80      # Requires root for port 80
./httpd 8080         # Non-privileged port
```

The server binds to the specified port on all network interfaces (0.0.0.0) and displays connection information:

```
Server starting...
Listening on address: 0.0.0.0, PORT: 8080

Connection from: 127.0.0.1, uri: public_html/index.html
```

### File Structure

Create a `public_html` directory in the same location as the executable:

```
.
├── httpd
└── public_html/
    ├── index.html
    └── [other html files]
```

The server serves files from `public_html/`. Request paths map directly to filesystem paths (e.g., `GET /index.html` serves `public_html/index.html`).

## Performance

Tested with ApacheBench on localhost:

```
ab -n 10000000 -c 1000 http://127.0.0.1/index.html
```

Results:
- **Requests per second:** 96,164.97 [#/sec]
- **Concurrency level:** 1000
- **Total requests:** 10,000,000
- **Failed requests:** 0
- **Median response time:** 1ms
- **95th percentile:** 2ms

## Implementation Notes

- The server handles basic GET requests only
- Configurable port via command-line argument
- Logs server bind address and client connections with request URIs
- Response always uses `Content-Type: text/html`
- No support for Content-Length calculation (HTTP/1.0 style connection close)
- Basic path validation (rejects requests containing "..")
- EAGAIN/EWOULDBLOCK handled for non-blocking socket operations
- Client sockets removed from epoll before closing to prevent use-after-free

## License

MIT
This is a learning project built from system call man pages and HTTP specifications.
