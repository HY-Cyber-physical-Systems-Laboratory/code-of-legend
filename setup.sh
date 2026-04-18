#!/bin/bash
set -eo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

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

# ── 2. Install ALL dependencies ──
step "Installing dependencies..."
apt-get update -qq 2>/dev/null || sudo apt-get update -qq
apt-get install -y -qq \
    cmake g++ make git \
    postgresql postgresql-client \
    python3 \
    libjsoncpp-dev libssl-dev libpq-dev libargon2-dev uuid-dev zlib1g-dev \
    2>/dev/null \
|| sudo apt-get install -y -qq \
    cmake g++ make git \
    postgresql postgresql-client \
    python3 \
    libjsoncpp-dev libssl-dev libpq-dev libargon2-dev uuid-dev zlib1g-dev

# ── 3. PostgreSQL ──
step "Starting PostgreSQL..."
pg_ctlcluster $(pg_lsclusters -h 2>/dev/null | head -1 | awk '{print $1, $2}') start 2>/dev/null \
    || systemctl start postgresql 2>/dev/null \
    || service postgresql start 2>/dev/null \
    || true

for i in 1 2 3 4 5; do
    pg_isready -q 2>/dev/null && break
    sleep 1
done
pg_isready -q 2>/dev/null || fail "PostgreSQL failed to start"

step "PostgreSQL is running. Setting up database..."

# Detect how to run psql as postgres admin
if sudo -n -u postgres psql -c "SELECT 1" >/dev/null 2>&1; then
    PG() { sudo -u postgres psql "$@"; }
elif su - postgres -c "psql -c 'SELECT 1'" </dev/null >/dev/null 2>&1; then
    PG() { su - postgres -c "psql $*"; }
elif psql -U postgres -c "SELECT 1" >/dev/null 2>&1; then
    PG() { psql -U postgres "$@"; }
else
    warn "Cannot run psql as postgres admin. Skipping user/db creation."
    warn "Make sure DB user '$DB_USER' and database '$DB_NAME' exist."
    PG() { return 1; }
fi

PG -tc "SELECT 1 FROM pg_roles WHERE rolname='$DB_USER'" 2>/dev/null | grep -q 1 \
    || PG -c "CREATE ROLE \"$DB_USER\" WITH LOGIN PASSWORD '$DB_PASS' CREATEDB;" 2>/dev/null \
    || warn "User $DB_USER may already exist"

PG -c "ALTER ROLE \"$DB_USER\" WITH PASSWORD '$DB_PASS';" 2>/dev/null || true

PG -tc "SELECT 1 FROM pg_database WHERE datname='$DB_NAME'" 2>/dev/null | grep -q 1 \
    || PG -c "CREATE DATABASE $DB_NAME OWNER \"$DB_USER\";" 2>/dev/null \
    || warn "Database $DB_NAME may already exist"

PG -c "GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO \"$DB_USER\";" 2>/dev/null || true

step "Running schema migration..."
export PGPASSWORD="$DB_PASS"
psql -h 127.0.0.1 -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" -f sql/init.sql 2>&1 \
    || warn "Some tables may already exist (OK)"

psql -h 127.0.0.1 -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" \
    -c "GRANT ALL ON ALL TABLES IN SCHEMA public TO \"$DB_USER\";" 2>/dev/null || true
psql -h 127.0.0.1 -p "$DB_PORT" -U "$DB_USER" -d "$DB_NAME" \
    -c "GRANT ALL ON ALL SEQUENCES IN SCHEMA public TO \"$DB_USER\";" 2>/dev/null || true
unset PGPASSWORD

# ── 4. Build ──
step "Building project..."
mkdir -p build
cd build

step "Running cmake..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_DISABLE_FIND_PACKAGE_c-ares=ON || fail "CMake configuration failed"

step "Compiling..."
cmake --build . -j"$(nproc)" || fail "Build failed"

echo ""
echo -e "${G}============================================${NC}"
echo -e "${G}  Build complete!${NC}"
echo -e "${G}  Run:  cd build && ./code-of-legend${NC}"
echo -e "${G}  Open: http://localhost:8080${NC}"
echo -e "${G}============================================${NC}"

if [ "$1" = "--run" ]; then
    step "Starting server on http://localhost:8080 ..."
    exec ./code-of-legend
fi
