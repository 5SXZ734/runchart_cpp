#!/usr/bin/env bash
set -euo pipefail

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-8080}"
TRACK_ID="${TRACK_ID:-}"
TOKEN="${TOKEN:-}"
OUT_DIR="${OUT_DIR:-./tmp_stream_test}"
CURL_BIN="${CURL_BIN:-curl}"

usage() {
  cat <<EOF
Usage:
  HOST=127.0.0.1 PORT=8080 TRACK_ID=1 TOKEN=secret ./scripts/test_stream_endpoint.sh

Required env:
  TRACK_ID           A valid track id present in library.db
Optional env:
  HOST               Default: 127.0.0.1
  PORT               Default: 8080
  TOKEN              x-auth-token value; optional when auth secret is empty
  OUT_DIR            Default: ./tmp_stream_test
  CURL_BIN           Default: curl
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ -z "$TRACK_ID" ]]; then
  echo "ERROR: TRACK_ID is required." >&2
  usage
  exit 2
fi

mkdir -p "$OUT_DIR"

pass() { echo "[PASS] $*"; }
fail() { echo "[FAIL] $*"; exit 1; }
info() { echo "[INFO] $*"; }

request() {
  local method="$1"; shift
  local url="$1"; shift
  local headers_file="$1"; shift
  local body_file="$1"; shift
  local -a extra=("$@")

  "$CURL_BIN" -sS -X "$method" "$url" -D "$headers_file" -o "$body_file" "${extra[@]}"
}

status_code() {
  local headers_file="$1"
  awk 'toupper($1) ~ /^HTTP\// { code=$2 } END { print code }' "$headers_file"
}

assert_status() {
  local got="$1" expected="$2" name="$3"
  if [[ "$got" == "$expected" ]]; then
    pass "$name -> HTTP $got"
  else
    echo "---- headers ----" && cat "$OUT_DIR/$name.headers" || true
    echo "---- body ----" && cat "$OUT_DIR/$name.body" || true
    fail "$name expected HTTP $expected but got $got"
  fi
}

assert_header_contains() {
  local headers_file="$1" pattern="$2" name="$3"
  if grep -Eiq "$pattern" "$headers_file"; then
    pass "$name header matches /$pattern/"
  else
    echo "---- headers ----" && cat "$headers_file" || true
    fail "$name missing header pattern: $pattern"
  fi
}

BASE_URL="http://${HOST}:${PORT}"
AUTH_HEADER=()
if [[ -n "$TOKEN" ]]; then
  AUTH_HEADER=(-H "x-auth-token: ${TOKEN}")
fi

info "Testing against ${BASE_URL}, TRACK_ID=${TRACK_ID}"

# 1) Health
request GET "${BASE_URL}/health" "$OUT_DIR/health.headers" "$OUT_DIR/health.body"
assert_status "$(status_code "$OUT_DIR/health.headers")" "200" "health"

# 2) Metrics
request GET "${BASE_URL}/metrics" "$OUT_DIR/metrics.headers" "$OUT_DIR/metrics.body"
assert_status "$(status_code "$OUT_DIR/metrics.headers")" "200" "metrics"

# 3) Full stream
request GET "${BASE_URL}/stream/${TRACK_ID}" "$OUT_DIR/full.headers" "$OUT_DIR/full.body" "${AUTH_HEADER[@]}"
full_status="$(status_code "$OUT_DIR/full.headers")"
if [[ "$full_status" == "200" ]]; then
  pass "full stream -> HTTP 200"
else
  echo "---- full headers ----" && cat "$OUT_DIR/full.headers" || true
  echo "---- full body ----" && cat "$OUT_DIR/full.body" || true
  fail "full stream expected 200 (if auth enabled, set TOKEN correctly)"
fi
assert_header_contains "$OUT_DIR/full.headers" '^Content-Type:\s*audio/mpeg' "full"
assert_header_contains "$OUT_DIR/full.headers" '^Accept-Ranges:\s*bytes' "full"
full_size=$(wc -c < "$OUT_DIR/full.body" | tr -d ' ')
if [[ "$full_size" -gt 0 ]]; then
  pass "full stream returned ${full_size} bytes"
else
  fail "full stream returned empty body"
fi

# 4) Partial range 0-1023
request GET "${BASE_URL}/stream/${TRACK_ID}" "$OUT_DIR/range_0_1023.headers" "$OUT_DIR/range_0_1023.body" \
  -H "Range: bytes=0-1023" "${AUTH_HEADER[@]}"
assert_status "$(status_code "$OUT_DIR/range_0_1023.headers")" "206" "range_0_1023"
assert_header_contains "$OUT_DIR/range_0_1023.headers" '^Content-Range:\s*bytes\s+0-1023/' "range_0_1023"
range_size=$(wc -c < "$OUT_DIR/range_0_1023.body" | tr -d ' ')
if [[ "$range_size" -eq 1024 ]]; then
  pass "range 0-1023 returned 1024 bytes"
else
  fail "range 0-1023 expected 1024 bytes but got ${range_size}"
fi

# 5) Invalid range (should be 416)
request GET "${BASE_URL}/stream/${TRACK_ID}" "$OUT_DIR/range_invalid.headers" "$OUT_DIR/range_invalid.body" \
  -H "Range: bytes=999999999-1000000000" "${AUTH_HEADER[@]}"
assert_status "$(status_code "$OUT_DIR/range_invalid.headers")" "416" "range_invalid"
assert_header_contains "$OUT_DIR/range_invalid.headers" '^Content-Range:\s*bytes\s+\*/' "range_invalid"

# 6) Not found track
request GET "${BASE_URL}/stream/999999999999" "$OUT_DIR/not_found.headers" "$OUT_DIR/not_found.body" "${AUTH_HEADER[@]}"
assert_status "$(status_code "$OUT_DIR/not_found.headers")" "404" "not_found"

# 7) Unauthorized (only when TOKEN provided; use a bad token)
if [[ -n "$TOKEN" ]]; then
  request GET "${BASE_URL}/stream/${TRACK_ID}" "$OUT_DIR/unauthorized.headers" "$OUT_DIR/unauthorized.body" -H "x-auth-token: __invalid__"
  assert_status "$(status_code "$OUT_DIR/unauthorized.headers")" "401" "unauthorized"
else
  info "Skipping explicit unauthorized check because TOKEN is empty (auth may be disabled)."
fi

pass "All stream endpoint checks passed. Artifacts in: $OUT_DIR"
