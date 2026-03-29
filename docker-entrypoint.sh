#!/bin/bash
set -e

# Init DB if not exists
if [ ! -f /app/data/projects.db ]; then
    echo "[init] Creating database..."
    sqlite3 /app/data/projects.db < /app/schema.sql
    echo "[init] Database created with seed data"
fi

# Backup on startup (7-day retention)
if [ -f /app/data/projects.db ]; then
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    cp /app/data/projects.db /app/backups/projects_${TIMESTAMP}.db
    echo "[backup] Created: projects_${TIMESTAMP}.db"
    cd /app/backups
    find . -name "projects_*.db" -mtime +7 -delete 2>/dev/null
    BACKUP_COUNT=$(ls projects_*.db 2>/dev/null | wc -l)
    echo "[backup] ${BACKUP_COUNT} backups retained (7-day retention)"
fi

# API key from environment
if [ -n "$PROJECT_API_KEY" ]; then
    echo "[auth] API key set"
else
    echo "[auth] WARNING: No API key - running without auth"
fi

# Start API (serves both API on 8080 and WebUI on 8081)
cd /app
echo "[start] Launching..."
exec ./project_api
