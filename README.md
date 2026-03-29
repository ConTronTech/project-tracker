# Project Tracker API v2

A secure, self-hosted project and file management API with a modern WebUI. Built in C++ with Crow HTTP framework and SQLite.

## Features

- **Full REST API** for projects, files, and search
- **Session-based WebUI** with modern dark theme (pink/black/red)
- **Security-first design**: rate limiting, CSRF protection, CORS lockdown, parameterized SQL
- **API key authentication** for scripts/agents + session cookies for browsers
- **Zero external dependencies** beyond Crow (header-only) and SQLiteCpp

## Quick Start

### 1. Clone & Build

```bash
git clone https://github.com/ConTronTech/project-tracker.git
cd project-tracker
make
```

Requires: `g++` (C++17), `make`, `pthread`, SQLite3 development headers.

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

All API endpoints require the `X-API-Key` header, except:
- `GET /api/files/{slug}/{filename}` (reads are public)
- WebUI routes (use session cookies)

### Projects

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/projects` | List all projects |
| `GET` | `/api/projects/{slug}` | Get project details |
| `POST` | `/api/projects` | Create project |
| `PUT` | `/api/projects/{slug}` | Update project |
| `DELETE` | `/api/projects/{slug}` | Delete project |

**Project JSON:**
```json
{
  "slug": "my-project",
  "name": "My Project",
  "status": "active",
  "tags": "c++,web,api",
  "description": "A cool project"
}
```

Valid statuses: `active`, `paused`, `completed`, `abandoned`

### Files

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/files/{slug}` | List files in project |
| `GET` | `/api/files/{slug}/{filename}` | Read file content (public) |
| `POST` | `/api/files/{slug}/{filename}` | Create/update file |
| `DELETE` | `/api/files/{slug}/{filename}` | Delete file |
| `GET` | `/api/files/{slug}/{filename}/download` | Download file as attachment |

### Search

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/api/search?q={query}` | Search projects and files |

### WebUI

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/login` | Login page |
| `POST` | `/webui/login` | Authenticate (returns session cookie) |
| `POST` | `/webui/logout` | End session |
| `GET` | `/` | Main app (redirects to `/login` if not authenticated) |
| `GET` | `/css/styles.css` | Stylesheet |
| `GET` | `/js/app.js` | Main JS |

## Docker

Build and run with Docker:

```bash
docker build -t project-tracker .
docker run -d \
  -p 8080:8080 \
  -e PROJECT_API_KEY=your-secret-key \
  -v /path/to/data:/app/data \
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
      - ./data:/app/data
    restart: unless-stopped
```

## Security Features

- **Parameterized SQL queries** — All database queries use bound parameters, preventing SQL injection
- **Rate limiting** — Configurable per-IP limits with burst allowance
- **CSRF protection** — Browser requests verified against Origin/Referer headers
- **CORS lockdown** — Whitelist-based allowed origins
- **Constant-time API key comparison** — Prevents timing attacks
- **Input validation** — Slugs, filenames, and body sizes validated
- **Security headers** — CSP, HSTS, X-Frame-Options, X-Content-Type-Options
- **Session binding** — Sessions can be bound to client IP

## Development

### Project Structure

```
project_api_v2/
├── include/           # Header files
│   ├── core/         # Config, logging
│   ├── db/           # Database, models
│   ├── http/         # Middleware
│   ├── routes/       # API endpoints
│   ├── security/     # Auth, rate limiting, validation
│   └── utils/        # String utilities
├── src/              # Implementation
├── webui/            # Frontend
│   ├── html/
│   ├── css/
│   └── js/
├── libs/             # Dependencies (Crow, SQLiteCpp, nlohmann/json)
├── Makefile
├── Dockerfile
└── schema.sql
```

### Building

```bash
make          # Build
make clean    # Clean build artifacts
./project_api config.env  # Run
```

## License

MIT

## Credits

- Crow HTTP framework: https://github.com/CrowCpp/Crow
- SQLiteCpp: https://github.com/SRombauts/SQLiteCpp
- nlohmann/json: https://github.com/nlohmann/json
