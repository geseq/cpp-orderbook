// cpp_orderbook_adapter.cpp — geseq/cpp-orderbook behind the harness
// matching_engine_api.h ABI.
//
// cpp-orderbook is a header-template C++ limit-order book whose
// OrderBook<Notification> fires a single onExecutionReport(ExecutionReport)
// callback per event (New/Rejected/Canceled/Trade). This adapter:
//   - implements a NotificationInterface whose onExecutionReport converts
//     each Trade into a harness ME_TRADE report and records the engine's
//     cancel verdict (Canceled vs Rejected(OrderNotExists))
//   - synthesises ME_ORDER_ACK / ME_CANCEL_ACK / ME_MODIFY_ACK /
//     ME_CANCEL_REJECT / ME_MODIFY_REJECT above the engine — the Trade/cancel
//     callbacks lack the side/price the harness wire format requires
//   - shadow-tracks {oid -> price,side,remaining,alive} for the side/price
//     echo and as the source of truth for the audit queries
//
// Modify is cancel + reinsert (the engine has no native modify; this matches
// the harness contract: fills cross with the modify's seq). This mirrors the
// geseq Go adapter (additional_references/geseq_adapter) one-for-one, except
// cpp-orderbook's addOrder/cancelOrder take no monotonic token (the Go engine
// does), so there is no token counter here.

#include <cstdint>
#include <vector>

#include "matching_engine_api.h"
#include "orderbook.hpp"
#include "types.hpp"

using orderbook::Decimal;
using orderbook::ExecType;
using orderbook::ExecutionReport;
using orderbook::Flag;
using orderbook::OrderBook;
using orderbook::OrderID;
using orderbook::Side;
using orderbook::Type;

// ---------------------------------------------------------------------------
// Decimal conversions.
//
// Workload ticks are positive signed integers. Decimal = decimal::U8 carries
// each tick as Decimal(tick, 0) which produces internal fp = tick * 10^8. The
// engine compares fp directly, so a strictly increasing tick mapping is
// preserved bit-for-bit, and d.to_int() recovers fp / 10^8 = tick exactly
// (round-trips). This mirrors the geseq Go adapter's udecimal.New(tick,0) /
// d.Int().
// ---------------------------------------------------------------------------

static inline Decimal toDecPrice(int64_t ticks) {
    if (ticks <= 0) {
        return Decimal(uint64_t(0));
    }
    return Decimal(uint64_t(ticks), 0);
}

static inline Decimal toDecQty(uint32_t q) { return Decimal(uint64_t(q), 0); }

static inline int64_t fromDecPrice(const Decimal& d) { return int64_t(d.to_int()); }

static inline uint32_t fromDecQty(const Decimal& d) {
    uint64_t v = d.to_int();
    if (v > UINT32_MAX) {
        return UINT32_MAX;
    }
    return uint32_t(v);
}

// ---------------------------------------------------------------------------
// Shadow state.
// ---------------------------------------------------------------------------

struct Shadow {
    int64_t price = 0;
    uint64_t remaining = 0;
    uint8_t side = 0;  // 0 = buy, 1 = sell
    bool alive = false;
};

namespace {

const me_transport_t* gTransport = nullptr;
void* gSink = nullptr;

// Per-call context: onExecutionReport(Trade) reads gCurSeq, accumulates into
// gTakerFill, and decrements the maker's shadow. onExecutionReport(Canceled /
// Rejected) records the engine's cancel verdict in gCancelOK/gCancelQty.
uint64_t gCurSeq = 0;
uint64_t gTakerFill = 0;
bool gCancelOK = false;
uint64_t gCancelQty = 0;

std::vector<Shadow> gShadow;

// Forward decls used by the notification handler.
void emitTrade(uint64_t seq, uint64_t makerID, uint64_t takerID, int64_t price, uint32_t qty);

inline Shadow& shadowRef(uint64_t oid) {
    if (oid >= gShadow.size()) {
        gShadow.resize(oid + 1);
    }
    return gShadow[oid];
}

// ---------------------------------------------------------------------------
// Notification handler (cpp-orderbook CRTP NotificationInterface).
//
// The engine fires onExecutionReport:
//  - ExecType::New (CreateOrder, Accepted) once per addOrder, before matching.
//    Ignored — engine_on_new_order eagerly emits ME_ORDER_ACK with the
//    side+price the report doesn't carry.
//  - ExecType::Trade per fill: maker/taker ids + last_qty + last_price.
//  - ExecType::Canceled (CancelOrder) on a successful cancelOrder: the engine's
//    cancel verdict. Recorded into gCancelOK/gCancelQty; engine_on_cancel
//    emits the ME_CANCEL_ACK itself with the side/price the report lacks.
//  - ExecType::Rejected on a cancel of a not-resting order (CancelOrder +
//    Error::OrderNotExists) or on duplicate-ID / zero-qty / zero-price create
//    errors. The cancel reject is the source of the gCancelOK = false verdict;
//    create rejects don't occur in the canonical workload and are ignored.
// ---------------------------------------------------------------------------

class HarnessNotification : public orderbook::NotificationInterface<HarnessNotification> {
   public:
    void onExecutionReport(const ExecutionReport& r) {
        switch (r.exec_type) {
            case ExecType::Trade: {
                uint32_t q = fromDecQty(r.last_qty);
                int64_t p = fromDecPrice(r.last_price);
                emitTrade(gCurSeq, r.maker_order_id, r.taker_order_id, p, q);
                gTakerFill += q;

                Shadow& e = shadowRef(r.maker_order_id);
                if (e.remaining >= q) {
                    e.remaining -= q;
                } else {
                    e.remaining = 0;
                }
                if (e.remaining == 0) {
                    e.alive = false;
                }
                break;
            }
            case ExecType::Canceled: {
                // Public cancelOrder verdict: removed.
                gCancelOK = true;
                gCancelQty = fromDecQty(r.qty);
                break;
            }
            case ExecType::Rejected: {
                // cancelOrder of a not-resting order reports
                // CancelOrder + OrderNotExists. gCancelOK stays false (set by
                // the caller before cancelOrder); nothing to record.
                break;
            }
            case ExecType::New:
            default:
                // Accept notification carries no payload the adapter needs;
                // the OrderAck is synthesised above the engine.
                break;
        }
    }
};

HarnessNotification gNotification;
OrderBook<HarnessNotification>* gBook = nullptr;

// ---------------------------------------------------------------------------
// Report emission.
// ---------------------------------------------------------------------------

inline void cpu_pause() {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

inline void emit(const me_report_t* r) {
    while (gTransport->push(gSink, r) == 0) {
        cpu_pause();  // spin until the transport accepts the report
    }
}

inline void emitAck(uint8_t rtype, uint64_t seq, uint64_t oid, uint8_t side, int64_t price, uint32_t qty) {
    me_report_t r{};
    r.type = rtype;
    r.side = side;
    r.sequence_number = seq;
    r.order_id = oid;
    r.price_ticks = price;
    r.quantity = qty;
    emit(&r);
}

void emitTrade(uint64_t seq, uint64_t makerID, uint64_t takerID, int64_t price, uint32_t qty) {
    me_report_t r{};
    r.type = ME_TRADE;
    r.side = 0;
    r.sequence_number = seq;
    r.order_id = makerID;
    r.price_ticks = price;
    r.quantity = qty;
    r.maker_order_id = makerID;
    r.taker_order_id = takerID;
    emit(&r);
}

// ---------------------------------------------------------------------------
// Per-message handlers (shared by the per-message ABI and engine_on_batch).
// ---------------------------------------------------------------------------

void onNewOrder(const new_order_t* o) {
    uint64_t seq = o->sequence_number;
    uint64_t oid = o->order_id;
    uint8_t side = o->side;
    int64_t price = o->price_ticks;
    uint32_t qty = o->quantity;

    // 1. OrderAck. (Engine fires Accepted too; we ignore that.)
    emitAck(ME_ORDER_ACK, seq, oid, side, price, qty);

    // 2. Drive the engine. onExecutionReport(Trade) reads gCurSeq and writes
    //    gTakerFill / decrements the maker shadow.
    gCurSeq = seq;
    gTakerFill = 0;

    Side sideT = (side == 0) ? Side::Buy : Side::Sell;
    Flag flag = (o->ioc != 0) ? Flag::IoC : Flag::None;

    gBook->addOrder(oid, Type::Limit, sideT, toDecQty(qty), toDecPrice(price), flag);

    uint64_t filled = gTakerFill;
    uint32_t residual = (filled < qty) ? uint32_t(qty - filled) : 0;

    if (o->ioc != 0) {
        // IoC residual cancellation: emit a CancelAck for the unfilled
        // remainder. The engine discards the residual without notifying.
        if (residual > 0) {
            emitAck(ME_CANCEL_ACK, seq, oid, side, price, residual);
        }
        return;
    }

    // GTC: shadow tracks the resting remainder.
    Shadow& e = shadowRef(oid);
    e.price = price;
    e.side = side;
    e.remaining = residual;
    e.alive = (residual > 0);
}

void onCancel(const cancel_t* c) {
    uint64_t seq = c->sequence_number;
    uint64_t oid = c->order_id;

    // The engine adjudicates: cancelOrder reports Canceled on removal, or
    // Rejected(OrderNotExists) for never-seen / already-terminal ids.
    gCancelOK = false;
    gCancelQty = 0;
    gBook->cancelOrder(oid);
    if (!gCancelOK) {
        emitAck(ME_CANCEL_REJECT, seq, oid, 0, 0, 0);
        return;
    }

    // Payload echo: side/price from the shadow (the engine's report carries
    // neither); the quantity is the engine's own reported remainder.
    Shadow& e = shadowRef(oid);
    emitAck(ME_CANCEL_ACK, seq, oid, e.side, e.price, uint32_t(gCancelQty));
    e.alive = false;  // audit queries scan alive/remaining
}

void onModify(const modify_t* m) {
    uint64_t seq = m->sequence_number;
    uint64_t oid = m->order_id;
    int64_t newPrice = m->new_price_ticks;
    uint32_t newQty = m->new_quantity;

    // The engine adjudicates the cancel half of cancel + reinsert: Rejected
    // (never seen / already terminal) maps to ModifyReject.
    gCancelOK = false;
    gCancelQty = 0;
    gBook->cancelOrder(oid);
    if (!gCancelOK) {
        emitAck(ME_MODIFY_REJECT, seq, oid, 0, 0, 0);
        return;
    }

    uint8_t side = shadowRef(oid).side;  // payload echo (report has no side)

    emitAck(ME_MODIFY_ACK, seq, oid, side, newPrice, newQty);

    // Reinsert at the new price/quantity; its crossing fills emit ME_TRADE.
    gCurSeq = seq;
    gTakerFill = 0;

    Side sideT = (side == 0) ? Side::Buy : Side::Sell;
    gBook->addOrder(oid, Type::Limit, sideT, toDecQty(newQty), toDecPrice(newPrice), Flag::None);

    uint64_t filled = gTakerFill;
    uint32_t residual = (filled < newQty) ? uint32_t(newQty - filled) : 0;

    Shadow& e = shadowRef(oid);
    e.price = newPrice;
    e.side = side;
    e.remaining = residual;
    e.alive = (residual > 0);
}

}  // namespace

// ---------------------------------------------------------------------------
// Exported ABI.
// ---------------------------------------------------------------------------

extern "C" {

void engine_init(uint64_t /*seed*/, const me_transport_t* transport, void* report_sink) {
    gTransport = transport;
    gSink = report_sink;
    gCurSeq = 0;
    gTakerFill = 0;
    gCancelOK = false;
    gCancelQty = 0;

    gShadow.assign(size_t(1) << 21, Shadow{});

    delete gBook;
    // Generous pools: the canonical workload has up to ~millions of orders;
    // the AdaptiveObjectPool grows on demand so these are just initial sizes.
    gBook = new OrderBook<HarnessNotification>(gNotification, 1 << 16, 1 << 21);
}

void engine_shutdown(void) {
    delete gBook;
    gBook = nullptr;
    gShadow.clear();
    gShadow.shrink_to_fit();
}

void engine_on_new_order(const new_order_t* order) { onNewOrder(order); }

void engine_on_cancel(const cancel_t* cancel) { onCancel(cancel); }

void engine_on_modify(const modify_t* modify) { onModify(modify); }

void engine_flush(void) {
    // Synchronous matcher: engine_on_* has already produced every report.
}

int64_t engine_query_best_bid(void) {
    int64_t best = INT64_MIN;
    for (const Shadow& e : gShadow) {
        if (e.alive && e.side == 0 && e.price > best) {
            best = e.price;
        }
    }
    return best;
}

int64_t engine_query_best_ask(void) {
    int64_t best = INT64_MAX;
    for (const Shadow& e : gShadow) {
        if (e.alive && e.side == 1 && e.price < best) {
            best = e.price;
        }
    }
    return best;
}

uint64_t engine_query_depth_at(int64_t price_ticks, uint8_t side) {
    uint64_t total = 0;
    for (const Shadow& e : gShadow) {
        if (e.alive && e.side == side && e.price == price_ticks) {
            total += e.remaining;
        }
    }
    return total;
}

void engine_on_batch(const me_msg_t* msgs, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        const me_msg_t& m = msgs[i];
        switch (m.type) {
            case 0:
                onNewOrder(&m.no);
                break;
            case 1:
                onCancel(&m.c);
                break;
            case 2:
                onModify(&m.md);
                break;
            default:
                break;
        }
    }
}

}  // extern "C"
