#!/usr/bin/env bash
# Build cpp_orderbook_adapter.so. Clones the matching-engine-benchmark at the
# pinned commit (for api/matching_engine_api.h), configures cpp-orderbook with
# CMake so the CPM dependencies (Boost, cpp-decimal, cpp-pool) are fetched into
# build/<preset>/_deps, then compiles this adapter together with the engine's
# four translation units into a single .so.
#
# The geseq/cpp-orderbook engine is template-heavy: OrderBook<Notification> is
# header-only, but PriceLevel<P>'s matching members are explicitly instantiated
# (against the std::function-typed TradeNotification/PostOrderFill) in
# src/pricelevel.cpp, so that TU — plus orderqueue.cpp, order.cpp and the
# enum operator<< in types.cpp — must be compiled in.
#
# Overrides:
#   BENCH_SRC=/path/to/existing/matching-engine-benchmark   (skip the clone)
#   ME_ENGINE_SRC=/path/to/cpp-orderbook                    (default: this repo)
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# --- Resolve the engine source (cpp-orderbook root). ------------------------
if [ -n "${ME_ENGINE_SRC:-}" ]; then
    ENGINE="$ME_ENGINE_SRC"
else
    ENGINE="$(git -C "$DIR" rev-parse --show-toplevel 2>/dev/null || echo "$(cd "$DIR/.." && pwd)")"
fi
ENGINE="$(cd "$ENGINE" && pwd)"

# --- Resolve the benchmark source (for the C ABI header). -------------------
BENCH_URL="https://github.com/flash1-dev/matching-engine-benchmark"
BENCH_REF="77697a115e76a25a1e2aa886f555d55b87fcf052"
if [ -n "${BENCH_SRC:-}" ]; then
    BENCH="$BENCH_SRC"
else
    BENCH="$DIR/third_party/matching-engine-benchmark"
    if [ ! -d "$BENCH/.git" ]; then
        mkdir -p "$(dirname "$BENCH")"
        git clone --quiet "$BENCH_URL" "$BENCH"
    fi
    git -C "$BENCH" fetch --quiet origin "$BENCH_REF" 2>/dev/null || true
    git -C "$BENCH" checkout --quiet "$BENCH_REF"
fi
BENCH="$(cd "$BENCH" && pwd)"

# --- Locate cmake. ----------------------------------------------------------
CMAKE="${CMAKE:-cmake}"
if ! command -v "$CMAKE" >/dev/null 2>&1; then
    if [ -x "$HOME/.local/bin/cmake" ]; then
        CMAKE="$HOME/.local/bin/cmake"
    else
        echo "error: cmake not found (set CMAKE=/path/to/cmake)" >&2
        exit 1
    fi
fi

# --- Configure cpp-orderbook so CPM populates build/<preset>/_deps. ---------
# A "debug" preset configure is enough to fetch Boost / cpp-decimal / cpp-pool;
# we only need their headers, not the engine's own build artifacts.
BUILD_DIR="$ENGINE/build/debug"
if [ ! -d "$BUILD_DIR/_deps/decimal-src" ] || [ ! -d "$BUILD_DIR/_deps/pool-src" ] || [ ! -d "$BUILD_DIR/_deps/boost-src" ]; then
    if [ -f "$ENGINE/CMakePresets.json" ] && "$CMAKE" --preset debug -S "$ENGINE" >/dev/null 2>&1; then
        :
    else
        "$CMAKE" -S "$ENGINE" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug >/dev/null
    fi
fi

DECIMAL_INC="$BUILD_DIR/_deps/decimal-src/include"
POOL_INC="$BUILD_DIR/_deps/pool-src/include"
# Boost from CPM is laid out as a super-project; its component headers live in
# libs/<lib>/include. Collect every include dir so <boost/...> resolves.
BOOST_SRC="$BUILD_DIR/_deps/boost-src"
BOOST_INCS=()
if [ -d "$BOOST_SRC/libs" ]; then
    while IFS= read -r inc; do
        BOOST_INCS+=("-I$inc")
    done < <(find "$BOOST_SRC/libs" -maxdepth 2 -type d -name include)
fi
# Fallback: a flattened boost include root, if present.
[ -d "$BOOST_SRC/include" ] && BOOST_INCS+=("-I$BOOST_SRC/include")

for p in "$DECIMAL_INC" "$POOL_INC"; do
    if [ ! -d "$p" ]; then
        echo "error: dependency include dir missing: $p" >&2
        echo "       (CMake configure of $ENGINE did not populate _deps)" >&2
        exit 1
    fi
done

# --- Compile the adapter + engine TUs into the .so. -------------------------
OUT="$DIR/cpp_orderbook_adapter.so"
g++ -std=c++20 -O3 -march=native -fPIC -shared \
    -I"$BENCH/api" \
    -I"$ENGINE/include" \
    -I"$ENGINE/src" \
    -I"$DECIMAL_INC" \
    -I"$POOL_INC" \
    "${BOOST_INCS[@]}" \
    "$DIR/cpp_orderbook_adapter.cpp" \
    "$ENGINE/src/pricelevel.cpp" \
    "$ENGINE/src/orderqueue.cpp" \
    "$ENGINE/src/order.cpp" \
    "$ENGINE/src/types.cpp" \
    -pthread \
    -o "$OUT"

echo "built: $OUT"
