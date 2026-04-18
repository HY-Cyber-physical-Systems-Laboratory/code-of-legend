#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

# ── colors ──
G='\033[0;32m'; Y='\033[1;33m'; R='\033[0;31m'; NC='\033[0m'
step() { echo -e "\n${G}[*]${NC} $1"; }
warn() { echo -e "${Y}[!]${NC} $1"; }
fail() { echo -e "${R}[✗]${NC} $1"; exit 1; }

# ── 1. config.json ──
step "Checking config..."
if [ ! -f config/config.json ]; then
    cp config/config.example.json config/config.json
    warn "Created config/config.json from template."
    warn "Edit config/config.json with your DB credentials, then re-run this script."
    exit 1
fi

DB_USER=$(python3 -c "import json; print(json.load(open('config/config.json'))['db_clients'][0]['user'])")
DB_PASS=$(python3 -c "import json; print(json.load(open('config/config.json'))['db_clients'][0]['passwd'])")
DB_NAME=$(python3 -c "import json; print(json.load(open('config/config.json'))['db_clients'][0]['dbname'])")
DB_PORT=$(python3 -c "import json; print(json.load(open('config/config.json'))['db_clients'][0]['port'])")

if [ "$DB_USER" = "your_username" ]; then
    fail "Edit config/config.json with your DB credentials first."
fi

# ── 2. dependencies ──
step "Checking dependencies..."
DEPS=""
command -v cmake  >/dev/null 2>&1 || DEPS="$DEPS cmake"
command -v g++    >/dev/null 2>&1 || DEPS="$DEPS g++"
command -v psql   >/dev/null 2>&1 || DEPS="$DEPS postgresql postgresql-client"
command -v python3 >/dev/null 2>&1 || DEPS="$DEPS python3"

pkg-config --exists jsoncpp   2>/dev/null || DEPS="$DEPS libjsoncpp-dev"
pkg-config --exists openssl   2>/dev/null || DEPS="$DEPS libssl-dev"
pkg-config --exists libpq     2>/dev/null || DEPS="$DEPS libpq-dev"
pkg-config --exists libargon2 2>/dev/null || DEPS="$DEPS libargon2-dev"
[ -f /usr/include/uuid/uuid.h ]           || DEPS="$DEPS uuid-dev"
dpkg -s zlib1g-dev >/dev/null 2>&1        || DEPS="$DEPS zlib1g-dev"

if [ -n "$DEPS" ]; then
    step "Installing: $DEPS"
    sudo apt-get update -qq
    sudo apt-get install -y -qq $DEPS
fi

# ── 3. PostgreSQL ──
step "Setting up PostgreSQL..."
sudo systemctl start postgresql 2>/dev/null || sudo service postgresql start 2>/dev/null || true

if ! sudo -u postgres psql -c "SELECT 1" >/dev/null 2>&1; then
    fail "PostgreSQL is not running."
fi

sudo -u postgres psql -tc "SELECT 1 FROM pg_roles WHERE rolname='$DB_USER'" | grep -q 1 || \
    sudo -u postgres psql -c "CREATE USER $DB_USER WITH PASSWORD '$DB_PASS';"

sudo -u postgres psql -tc "SELECT 1 FROM pg_database WHERE datname='$DB_NAME'" | grep -q 1 || \
    sudo -u postgres psql -c "CREATE DATABASE $DB_NAME OWNER $DB_USER;"

sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO $DB_USER;"

step "Running schema migration..."
PGPASSWORD="$DB_PASS" psql -h 127.0.0.1 -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -f sql/init.sql 2>&1 | tail -5

# ── 4. Build ──
step "Building..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | tail -3
cmake --build . -j"$(nproc)" 2>&1 | tail -5

echo ""
echo -e "${G}============================================${NC}"
echo -e "${G}  Build complete!${NC}"
echo -e "${G}  Run:  cd build && ./code-of-legend${NC}"
echo -e "${G}  Open: http://localhost:8080${NC}"
echo -e "${G}============================================${NC}"

# ── 5. Run (optional: pass --run) ──
if [ "$1" = "--run" ]; then
    step "Starting server on http://localhost:8080 ..."
    exec ./code-of-legend
fi
