#!/bin/bash
set -e

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

# ── 2. Install all dependencies at once ──
step "Checking dependencies..."
DEPS=""
command -v cmake   >/dev/null 2>&1 || DEPS="$DEPS cmake"
command -v g++     >/dev/null 2>&1 || DEPS="$DEPS g++"
command -v python3 >/dev/null 2>&1 || DEPS="$DEPS python3"
command -v psql    >/dev/null 2>&1 || DEPS="$DEPS postgresql postgresql-client"

pkg-config --exists jsoncpp   2>/dev/null || DEPS="$DEPS libjsoncpp-dev"
pkg-config --exists openssl   2>/dev/null || DEPS="$DEPS libssl-dev"
pkg-config --exists libpq     2>/dev/null || DEPS="$DEPS libpq-dev"
pkg-config --exists libargon2 2>/dev/null || DEPS="$DEPS libargon2-dev"
[ -f /usr/include/uuid/uuid.h ]           || DEPS="$DEPS uuid-dev"
dpkg -s zlib1g-dev >/dev/null 2>&1        || DEPS="$DEPS zlib1g-dev"

if [ -n "$DEPS" ]; then
    step "Installing:$DEPS"
    apt-get update -qq 2>/dev/null || sudo apt-get update -qq
    apt-get install -y -qq $DEPS 2>/dev/null || sudo apt-get install -y -qq $DEPS
fi

# ── 3. PostgreSQL: start + create user/db ──
step "Setting up PostgreSQL..."

# Try every known way to start PostgreSQL
pg_ctlcluster $(pg_lsclusters -h 2>/dev/null | head -1 | awk '{print $1, $2}') start 2>/dev/null \
    || systemctl start postgresql 2>/dev/null \
    || service postgresql start 2>/dev/null \
    || pg_isready -q 2>/dev/null \
    || true

# Wait for it
for i in 1 2 3 4 5; do
    pg_isready -q 2>/dev/null && break
    sleep 1
done

if ! pg_isready -q 2>/dev/null; then
    fail "PostgreSQL failed to start. Try: sudo systemctl start postgresql"
fi

step "PostgreSQL is running."

# All admin ops go through the postgres system user (peer auth, no password needed)
PG="sudo -u postgres psql"

$PG -tc "SELECT 1 FROM pg_roles WHERE rolname='$DB_USER'" 2>/dev/null | grep -q 1 \
    || $PG -c "CREATE USER \"$DB_USER\" WITH PASSWORD '$DB_PASS' CREATEDB LOGIN;" \
    || warn "User $DB_USER may already exist"

$PG -c "ALTER USER \"$DB_USER\" WITH PASSWORD '$DB_PASS';" 2>/dev/null || true

$PG -tc "SELECT 1 FROM pg_database WHERE datname='$DB_NAME'" 2>/dev/null | grep -q 1 \
    || $PG -c "CREATE DATABASE $DB_NAME OWNER \"$DB_USER\";" \
    || warn "Database $DB_NAME may already exist"

$PG -c "GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO \"$DB_USER\";" 2>/dev/null || true

step "Running schema migration..."
$PG -d "$DB_NAME" -f sql/init.sql 2>&1 \
    || warn "Some tables may already exist (OK)"

$PG -d "$DB_NAME" -c "GRANT ALL ON ALL TABLES IN SCHEMA public TO \"$DB_USER\";" 2>/dev/null || true
$PG -d "$DB_NAME" -c "GRANT ALL ON ALL SEQUENCES IN SCHEMA public TO \"$DB_USER\";" 2>/dev/null || true

# ── 4. Build ──
step "Building project..."
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
