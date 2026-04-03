# Project Tracker API v2

A secure, self-hosted project and file management API with a modern WebUI. Built in C++ with Crow HTTP framework and SQLite.

## Features

- **Full REST API** for projects, files, and search
- **Session-based WebUI** with modern dark theme (pink/black/red)
- **Security-first design**: rate limiting, CSP, CSRF protection, CORS lockdown, parameterized SQL
- **API key authentication** for scripts/agents + session cookies for browsers
- **Thread-safe concurrency** — `std::shared_mutex` for read-many/write-one operations
- **Project archiving** — archive/restore projects with themed confirmation UI
- **File management** — create, edit, patch (line ops, find/replace, append), import, delete
- **Zero external dependencies** beyond Crow (header-only) and SQLiteCpp

## Quick Start

### 1. Clone & Build

```bash
git clone https://github.com/ConTronTech/project-tracker.git
cd project-tracker
make
```

Requires: `g++` (C++17), `make`, `libasio-dev`, `pthread`, SQLite3 development headers.

### 2. Configure

Copy the example config:

```bash
cp config.env project_api.env
```

Edit `project_api.env`:

```ini
# Required
PROJECT_API_KEY=your-secret-key-here
PROJECT_DB_PATH=./data/projects.db

# Server
PORT=8080
LOG_LEVEL=info

# CORS (comma-separated origins, or leave empty for same-origin only)
CORS_ORIGINS=http://localhost:3000,http://example.com

# Rate Limiting
RATE_LIMIT_READ=120
RATE_LIMIT_WRITE=30
RATE_LIMIT_BURST=15

# Sessions (for WebUI)
SESSION_MAX=50
SESSION_TIMEOUT=86400
SESSION_IP_BIND=true

# Trusted Proxies (for X-Forwarded-For, comma-separated)
TRUSTED_PROXIES=127.0.0.1,::1
```

### 3. Run

```bash
export PROJECT_API_KEY="your-secret-key-here"
./project_api project_api.env
```

The API will be available at `http://localhost:8080`.
The WebUI will be at `http://localhost:8080/login`.

## Configuration Reference

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `PROJECT_API_KEY` | **Yes** | — | API key for authentication |
| `PROJECT_DB_PATH` | No | `./data/projects.db` | Path to SQLite database |
| `PORT` | No | `8080` | Server port |
| `LOG_LEVEL` | No | `info` | Log verbosity: `debug`, `info`, `warn`, `error` |
| `CORS_ORIGINS` | No | (empty) | Comma-separated allowed origins for CORS |
| `RATE_LIMIT_READ` | No | `120` | Requests per minute for read operations |
| `RATE_LIMIT_WRITE` | No | `30` | Requests per minute for write operations |
| `RATE_LIMIT_BURST` | No | `15` | Burst allowance |
| `RATE_LIMIT_ENABLED` | No | `true` | Enable/disable rate limiting |
| `SESSION_MAX` | No | `50` | Maximum concurrent sessions |
| `SESSION_TIMEOUT` | No | `86400` | Session timeout in seconds (24h) |
| `SESSION_IP_BIND` | No | `true` | Bind sessions to client IP |
| `TRUSTED_PROXIES` | No | (empty) | Comma-separated trusted proxy IPs for X-Forwarded-For |

## API Endpoints

### Authentication

Write endpoints require the `X-API-Key` header. Read endpoints are public.
WebUI uses session cookies (HttpOnly, SameSite=Strict).

### Projects

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/projects` | List projects (excludes archived by default) |
| `GET` | `/api/projects?status=all` | List all projects including archived |
| `GET` | `/api/projects?status=archived` | List only archived projects |
| `GET` | `/api/projects?search=query` | Search projects by name/tags/description |
| `GET` | `/api/projects/{slug}` | Get project details + file list |
| `POST` | `/api/projects` | Create project (auth required) |
| `PUT` | `/api/projects/{slug}` | Update project (auth required) |
| `DELETE` | `/api/projects/{slug}` | Delete project + all files (auth required) |

**Create/Update JSON:**
```json
{
  "slug": "my-project",
  "name": "My Project",
  "status": "active",
  "priority": "high",
  "tags": "c++,web,api",
  "description": "A cool project",
  "repo_path": "/path/to/repo"
}
```

Valid statuses: `active`, `paused`, `completed`, `abandoned`, `archived`
Valid priorities: `high`, `medium`, `low`

### Files

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/projects/{slug}/files/{filename}` | Read file content |
| `GET` | `/api/projects/{slug}/files/{filename}?from=1&to=10` | Read specific lines |
| `PUT` | `/api/projects/{slug}/files/{filename}` | Create/overwrite file (auth) |
| `PATCH` | `/api/projects/{slug}/files/{filename}` | Patch file (auth) |
| `DELETE` | `/api/projects/{slug}/files/{filename}` | Delete file (auth) |

**Patch Operations:**

| Operation | Body | Description |
|-----------|------|-------------|
| `replace_line` | `{"op":"replace_line","line":5,"content":"new text"}` | Replace line N |
| `insert_line` | `{"op":"insert_line","line":5,"content":"new text"}` | Insert after line N |
| `delete_line` | `{"op":"delete_line","line":5}` | Delete line N |
| `append` | `{"op":"append","content":"text"}` | Append to end |
| `find_replace` | `{"op":"find_replace","find":"old","with":"new"}` | Find and replace |

### WebUI Routes

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/login` | Login page |
| `POST` | `/webui/login` | Authenticate (returns session cookie) |
| `POST` | `/webui/logout` | End session |
| `GET` | `/` | Main app (redirects to `/login` if not authenticated) |
| `GET` | `/static/css/{file}` | Stylesheets |
| `GET` | `/static/js/{file}` | JavaScript |

## WebUI Features

- **Dark theme** with pink/red accents
- **Project sidebar** with search and status badges
- **File editor** with syntax-aware editing
- **Archive toggle** — show/hide archived projects
- **Archive/Restore** — themed confirmation modal with context-aware UI
- **New Project/File modals** — inline creation
- **Edit project** — status, priority, tags, description, repo path
- **File import** — drag and drop or file picker
- **Conflict detection** — warns on overwrite, offers numbered copy
- **Keyboard shortcuts** — Escape closes modals

## Docker

### Build and Run

```bash
docker build -t project-tracker .
docker run -d \
  --name project_tracker \
  --restart unless-stopped \
  -p 8080:8080 \
  -e PROJECT_API_KEY=your-secret-key \
  -v pt_data:/app/data \
  -v pt_backups:/app/backups \
  project-tracker
```

### Docker Compose

```yaml
version: '3'
services:
  project-tracker:
    build: .
    ports:
      - "8080:8080"
    environment:
      - PROJECT_API_KEY=your-secret-key
    volumes:
      - pt_data:/app/data
      - pt_backups:/app/backups
    restart: unless-stopped

volumes:
  pt_data:
  pt_backups:
```

### Backups

The container automatically creates a timestamped SQLite backup on every startup with 7-day retention:
```
/app/backups/projects_20260403_153000.db
```

## Security

### Authentication & Sessions
- **API key auth** — `X-API-Key` header for programmatic access
- **Session cookies** — `HttpOnly; SameSite=Strict` for WebUI
- **Constant-time comparison** — prevents timing attacks on API key validation
- **IP-bound sessions** — sessions tied to client IP (configurable)
- **Periodic session cleanup** — background timer purges expired sessions every 5 minutes
- **Max concurrent sessions** — configurable limit (default 50)

### Concurrency & Thread Safety
- **`std::shared_mutex`** on database — read ops use `shared_lock`, write ops use `unique_lock`
- **Atomic patch operations** — entire read-modify-write under exclusive lock (TOCTOU fix)
- **SQLite transactions** — atomic cascading deletes
- **Move-only session semantics** — no accidental copies

### Input Validation
- **Parameterized SQL** — all queries use bound parameters, zero SQL injection surface
- **Slug validation** — alphanumeric + hyphens/underscores only
- **Filename validation** — no path traversal, no null bytes, no control chars
- **Body size limits** — configurable max request size
- **Status/priority sanitization** — whitelist-only values accepted

### HTTP Security
- **Content-Security-Policy** — `script-src 'self'` (no inline scripts), locked down sources
- **X-Frame-Options: DENY** — no iframe embedding
- **X-Content-Type-Options: nosniff** — no MIME sniffing
- **X-XSS-Protection: 1; mode=block** — XSS filter enabled
- **CSRF protection** — SameSite cookies + Origin/Referer verification
- **CORS lockdown** — whitelist-based allowed origins
- **Rate limiting** — token bucket per IP with configurable burst and auto-ban
- **Header injection prevention** — headers with CRLF are stripped
- **JSON escaping** — `nlohmann::json::dump()` for safe output
- **Path traversal blocked** — `std::filesystem::canonical()` on all static file paths

## Architecture

```
project-tracker/
├── include/
│   ├── core/
│   │   ├── config.h              # Singleton .env parser
│   │   └── logging.h             # Structured logger with levels
│   ├── db/
│   │   └── database.h            # SQLite wrapper with shared_mutex
│   ├── http/
│   │   └── middleware.h           # Security headers, CORS, CSRF, rate limit hooks
│   ├── routes/
│   │   ├── api.h                 # REST API routes (projects, files, search)
│   │   └── webui.h               # WebUI routes (login, static files, sessions)
│   └── security/
│       ├── auth.h                # API key + session management
│       ├── rate_limiter.h        # Token bucket rate limiter with auto-ban
│       └── sanitizer.h           # Input validation (slugs, filenames, statuses)
├── src/                          # Matching .cpp implementations
│   ├── main.cpp                  # Entry point, signal handling, graceful shutdown
│   ├── core/
│   ├── db/
│   ├── http/
│   ├── routes/
│   └── security/
├── webui/
│   ├── html/
│   │   ├── index.html            # Main app shell
│   │   └── login.html            # Login page
│   ├── css/
│   │   └── styles.css            # Dark theme (pink/black/red)
│   └── js/
│       ├── app.js                # Core: API wrapper, modals, utilities
│       ├── projects.js           # Project CRUD, archive, listing
│       └── editor.js             # File editor, tabs, syntax
├── libs/
│   ├── crow.h                    # Crow HTTP framework (header-only)
│   ├── SQLiteCpp/                # SQLite C++ wrapper
│   └── nlohmann/json.hpp         # JSON library
├── Makefile
├── Dockerfile
├── docker-entrypoint.sh          # Init DB, backup, start server
├── schema.sql                    # Database schema + seed data
└── config.env                    # Example configuration
```

## Development

### Building

```bash
make              # Build
make clean        # Clean build artifacts
./project_api config.env  # Run with config
```

### Dependencies

- **g++** with C++17 support
- **make**
- **libasio-dev** (async I/O for Crow)
- **pthread**
- SQLite3 headers (bundled via SQLiteCpp)

### Adding New Routes

1. Add handler declaration in `include/routes/api.h` or `webui.h`
2. Implement in `src/routes/`
3. Register in the appropriate `registerXRoutes()` function
4. Add input validation in `security/sanitizer.cpp` if needed

## License

MIT

## Credits

- [Crow](https://github.com/CrowCpp/Crow) — HTTP framework
- [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) — SQLite C++ wrapper
- [nlohmann/json](https://github.com/nlohmann/json) — JSON library
