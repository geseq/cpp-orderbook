# cpp-orderbook

A high-performance, low-latency C++20 order book for matching engines.

[![CI](https://github.com/geseq/cpp-orderbook/actions/workflows/cmake.yml/badge.svg)](https://github.com/geseq/cpp-orderbook/actions)

## Overview

`cpp-orderbook` implements a price-time priority order book suitable for use as the core matching component of an exchange or trading simulator.

The library is header-centric (`include/orderbook.hpp`) and exposes a clean, templated API. Callers supply a *notification handler* that receives order lifecycle and trade events; the book itself stays allocation-light thanks to intrusive Boost data structures and an adaptive object pool.

## Features

- [x] Simple, templated API — swap in any notification handler with zero virtual-dispatch overhead (CRTP)
- [x] Standard price-time priority (FIFO within a price level)
- [x] Market and limit orders
- [x] Order cancellation (no in-book updates; use Cancel + Create for amendments)
- [x] `IoC` (Immediate-or-Cancel), `AoN` (All-or-None), `FoK` (Fill-or-Kill) flags
- [x] No-matching mode — accept resting limit orders without crossing the spread
- [x] Intrusive Boost red-black trees — zero heap allocation per order in the hot path
- [x] Adaptive object pools for `Order` and `OrderQueue` objects
- [x] Fixed-precision decimal arithmetic via [geseq/cpp-decimal](https://github.com/geseq/cpp-decimal) (8 decimal places)
- [ ] Stop-loss / take-profit orders
- [ ] Order book snapshot / recovery
- [ ] Trailing stops

## Architecture

```
OrderBook<Notification>
├── PriceLevel<Bid>          (red-black tree sorted descending by price)
│   └── OrderQueue           (FIFO list of Orders at one price point)
│       └── Order
└── PriceLevel<Ask>          (red-black tree sorted ascending by price)
    └── OrderQueue
        └── Order
```

| Component | File | Role |
|---|---|---|
| `OrderBook<N>` | `include/orderbook.hpp` | Top-level matching engine; owns bid/ask price levels and order map |
| `PriceLevel<P>` | `include/pricelevel.hpp` | One side of the book; manages a tree of `OrderQueue`s |
| `OrderQueue` | `include/orderqueue.hpp` | All resting orders at a single price; handles partial/complete fills |
| `Order` | `include/order.hpp` | Intrusive node stored simultaneously in the price-level list and the global order map |
| `NotificationInterface<N>` | `include/types.hpp` | CRTP base — implement `putOrder` and `putTrade` for callbacks |
| `Decimal` (`decimal::U8`) | `include/types.hpp` | Fixed-point type with 8 decimal places |

## Prerequisites

- C++20-capable compiler (GCC ≥ 12 or Clang ≥ 15 recommended)
- CMake ≥ 3.20
- Internet access on first build (dependencies are fetched by [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake))

Dependencies fetched automatically:

| Library | Version | Purpose |
|---|---|---|
| [Boost](https://www.boost.org/) | 1.80.0 | Intrusive containers (`rbtree`, `list`) |
| [geseq/cpp-decimal](https://github.com/geseq/cpp-decimal) | 2.1.0 | Fixed-precision decimal arithmetic |
| [geseq/cpp-pool](https://github.com/geseq/cpp-pool) | 0.5.0 | Adaptive object pool |
| [GoogleTest](https://github.com/google/googletest) | 1.14.0 | Unit tests (test builds only) |

## Building

### Release

```bash
cmake --preset release
cmake --build --preset release
```

The compiled static library is placed at `build/release/liborderbook.a` and the benchmark binary at `build/release/main`.

### Debug + Tests

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

### Docker (benchmark)

```bash
docker compose up
```

This builds the image and runs the throughput benchmark pinned to a single CPU core for 30 seconds (see `docker-compose.yml`).

## Usage

### 1. Implement a notification handler

```cpp
#include "orderbook.hpp"

class MyNotification : public orderbook::NotificationInterface<MyNotification> {
public:
    // Called for CreateOrder / CancelOrder lifecycle events
    void putOrder(orderbook::MsgType msgType,
                  orderbook::OrderStatus status,
                  orderbook::OrderID id,
                  orderbook::Decimal qty) {
        // handle accepted / canceled / partially-filled events
    }

    // Called when a rejection or error occurs
    void putOrder(orderbook::MsgType msgType,
                  orderbook::OrderStatus status,
                  orderbook::OrderID id,
                  orderbook::Decimal qty,
                  orderbook::Error err) {
        // handle rejection events
    }

    // Called for each individual trade that results from matching
    void putTrade(orderbook::OrderID makerOrderID,
                  orderbook::OrderID takerOrderID,
                  orderbook::OrderStatus makerStatus,
                  orderbook::OrderStatus takerStatus,
                  orderbook::Decimal qty,
                  orderbook::Decimal price) {
        // record the trade
    }
};
```

`EmptyNotification` (defined in `types.hpp`) is a no-op implementation useful for benchmarking.

### 2. Create an order book and submit orders

```cpp
MyNotification n;
orderbook::OrderBook<MyNotification> ob(n);

using namespace orderbook;

// Limit buy at 100.00 for qty 5
ob.addOrder(1, Type::Limit, Side::Buy,  Decimal("5"),   Decimal("100.00"), Flag::None);

// Limit sell at 101.00 for qty 3
ob.addOrder(2, Type::Limit, Side::Sell, Decimal("3"),   Decimal("101.00"), Flag::None);

// Market buy for qty 2 (matches immediately)
ob.addOrder(3, Type::Market, Side::Buy, Decimal("2"),   Decimal("0"),      Flag::None);

// IoC limit sell — fills what it can, cancels the rest
ob.addOrder(4, Type::Limit, Side::Sell, Decimal("10"),  Decimal("100.00"), Flag::IoC);

// Cancel a resting order
ob.cancelOrder(1);

// Print the current book state (bids | asks)
std::cout << ob.toString();
```

### 3. Enums reference

**`Type`** — order type
| Value | Meaning |
|---|---|
| `Limit` | Rests in the book if not immediately matched |
| `Market` | Matches against the best available price; never rests |

**`Side`**
| Value | Meaning |
|---|---|
| `Buy` | Aggressive against asks; rests on the bid side |
| `Sell` | Aggressive against bids; rests on the ask side |

**`Flag`** — execution flags (bit-maskable)
| Value | Meaning |
|---|---|
| `None` | Standard order |
| `IoC` | Immediate-or-Cancel — unmatched quantity is discarded |
| `AoN` | All-or-None — only fill if the full quantity can be matched |
| `FoK` | Fill-or-Kill — `AoN` + `IoC` combined |

**`OrderStatus`** — reported via `putOrder` / `putTrade`
| Value | Meaning |
|---|---|
| `Accepted` | Order entered the book |
| `Rejected` | Order was rejected (see `Error`) |
| `Canceled` | Order was canceled |
| `FilledPartial` | Order partially filled |
| `FilledComplete` | Order fully filled |

**`Error`**
| Value | Meaning |
|---|---|
| `InvalidQty` | Zero quantity |
| `InvalidPrice` | Zero price on a limit order |
| `OrderExists` | Duplicate order ID |
| `OrderNotExists` | Cancel for unknown order ID |
| `InsufficientQty` | AoN/FoK could not be fully filled |
| `NoMatching` | Market order or crossing limit in no-matching mode |

### 4. No-matching mode

When `setMatching(false)` is called, the book accepts non-crossing resting limit orders but rejects market orders and any limit order that would immediately match:

```cpp
ob.setMatching(false);
ob.addOrder(5, Type::Market, Side::Buy, Decimal("1"), Decimal("0"), Flag::None);
// → putOrder(CreateOrder, Rejected, 5, 1, Error::NoMatching)
```

### 5. Pool sizing

The constructor accepts optional pool pre-allocation hints:

```cpp
orderbook::OrderBook<MyNotification> ob(
    n,
    /*price_level_pool_size=*/ 65536,   // expected unique price levels
    /*order_pool_size=*/       65536    // expected live orders
);
```

Larger pools reduce runtime allocation at the cost of upfront memory.

## Decimal type

Prices and quantities are represented by `orderbook::Decimal` (an alias for `decimal::U8` from [geseq/cpp-decimal](https://github.com/geseq/cpp-decimal)), a fixed-point type with **8 decimal places**. Construct values from strings or from an integer mantissa + exponent pair:

```cpp
orderbook::Decimal price("123.45");
orderbook::Decimal qty(10, 0);   // 10 × 10^0 = 10
```

## Limitations

- **8 decimal places** maximum precision — sufficient for most financial instruments.
- **No thread safety** — the order book is single-threaded. Serialize access externally or wrap with a disruptor/ring-buffer pattern.
- **No in-book order updates** — price or quantity changes must be implemented as `cancelOrder` + `addOrder`.
- **AoN volume accounting** is noted as a known TODO in the source.

## Contributing

Bug reports and pull requests are welcome. Please open an issue first for any significant change.
