#!/bin/bash

# GStreamer Camera Manager - Single Command Startup
# FastAPI manages everything: PostgreSQL, Worker, Database Sync

echo "========================================="
echo "  GStreamer Camera Manager"
echo "  Production Startup Script"
echo "========================================="
echo ""

# Check if PostgreSQL is running
echo "Checking PostgreSQL..."
if ! docker ps | grep -q camera-postgres; then
    echo "Starting PostgreSQL..."
    cd backend
    docker-compose up -d
    cd ..
    echo "✓ PostgreSQL started"
else
    echo "✓ PostgreSQL already running"
fi

echo ""
echo "Starting FastAPI (will auto-start C++ Worker)..."
echo ""

cd backend
source venv/bin/activate
uvicorn app.main:app --host 0.0.0.0 --port 8002

# When you Ctrl+C, FastAPI will automatically stop the worker
