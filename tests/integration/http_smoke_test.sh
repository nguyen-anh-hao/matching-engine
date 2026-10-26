#!/usr/bin/env bash
# End-to-end smoke test for the HTTP adapter: starts the binary, exercises every
# route, and verifies graceful shutdown on SIGTERM.
#
# Usage: tests/integration/http_smoke_test.sh <path-to-matching-engine> [port]
set -euo pipefail

binary=${1:?usage: http_smoke_test.sh <matching-engine-binary> [port]}
port=${2:-19001}
base="http://127.0.0.1:${port}"
log_file=$(mktemp)
body_file=$(mktemp)

"$binary" "$port" >"$log_file" 2>&1 &
server_pid=$!

cleanup() {
    kill -TERM "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    rm -f "$log_file" "$body_file"
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    if curl -fsS "$base/health" >/dev/null 2>&1; then
        break
    fi
    sleep 0.1
done

fail() {
    echo "FAIL: $*"
    echo "--- server log ---"
    cat "$log_file"
    exit 1
}

request() {
    # request <method> <path> [body]
    local method=$1 path=$2 payload=${3:-}
    if [ -n "$payload" ]; then
        curl -s -o "$body_file" -w '%{http_code}' -X "$method" --data-binary "$payload" "$base$path"
    else
        curl -s -o "$body_file" -w '%{http_code}' -X "$method" "$base$path"
    fi
}

expect_status() {
    # expect_status <method> <path> <expected> [body]
    local actual
    actual=$(request "$1" "$2" "${4:-}")
    [ "$actual" = "$3" ] || fail "$1 $2 expected $3, got $actual: $(cat "$body_file")"
}

expect_body_contains() {
    grep -Fq -- "$1" "$body_file" || fail "body does not contain '$1': $(cat "$body_file")"
}

expect_status GET /health 200
expect_body_contains '"status":"ok"'

expect_status POST /orders 200 \
    '{"symbol":"AAPL","side":"BUY","type":"LIMIT","price":"10.50","quantity":"100","account_id":7}'
expect_body_contains '"status":"NEW"'

expect_status POST /orders 200 \
    '{"symbol":"AAPL","side":"SELL","type":"LIMIT","price":"10.50","quantity":"40","account_id":8}'
expect_body_contains '"status":"FILLED"'
expect_body_contains '"average_price":"10.5"'

expect_status GET '/book?symbol=AAPL&depth=5' 200
expect_body_contains '"bids":[["10.5","60"]]'

expect_status GET '/ticker?symbol=AAPL' 200
expect_body_contains '"last_price":"10.5"'

expect_status GET '/orders?symbol=AAPL' 200
expect_body_contains '"leaves":"60"'

expect_status POST /orders 400 \
    '{"symbol":"AAPL","side":"HOLD","price":"10","quantity":"1","account_id":7}'
expect_status POST /orders 400 '{"symbol":"AAPL",'
expect_status GET /missing 404
expect_status POST /book 405

expect_status DELETE '/orders?id=1&symbol=AAPL' 200
expect_body_contains '"success":true'
expect_status DELETE '/orders?id=1&symbol=AAPL' 404

expect_status GET /metrics 200
expect_body_contains 'trading_matching_orders_total'
expect_body_contains 'trading_matching_order_pool_capacity'

kill -TERM "$server_pid"
if ! wait "$server_pid"; then
    fail "server did not exit cleanly on SIGTERM"
fi
trap - EXIT
rm -f "$log_file" "$body_file"
echo "HTTP smoke test passed (port ${port})"
