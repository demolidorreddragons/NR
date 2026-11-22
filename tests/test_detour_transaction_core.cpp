#include "detour_transaction_core.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

enum class Failure {
    None,
    PrepareNoEffect,
    PrepareUnknown,
    PublishNoEffect,
    PublishUnknown,
    GateNoEffect,
    GateUnknown,
    WritableNoEffect,
    WritableUnknown,
    WriteNoEffect,
    WritePartial,
    VerifyPatch,
    RestoreProtection,
    ReleaseGate,
    RollbackWrite,
    VerifyRollback,
    ReleaseResources,
    PublishNoEffectReleaseResources
};

struct Fake {
    Failure failure = Failure::None;
    std::array<uint8_t, 8> target{{1,2,3,4,5,6,7,8}};
    bool prepared = false;
    bool published = false;
    bool gate = false;
    bool writable = false;
    bool resources_released = false;
    unsigned write_calls = 0;
};

static int preflight(void *, const uint8_t *original,
                     const uint8_t *patch, size_t length) {
    return original && patch && length == 8;
}
static nr_detour_tx_outcome prepare(void *p) {
    auto &f = *static_cast<Fake *>(p);
    if (f.failure == Failure::PrepareNoEffect) return NR_DETOUR_TX_NO_EFFECT;
    if (f.failure == Failure::PrepareUnknown) return NR_DETOUR_TX_UNKNOWN;
    f.prepared = true;
    return NR_DETOUR_TX_COMPLETE;
}
static nr_detour_tx_outcome publish(void *p) {
    auto &f = *static_cast<Fake *>(p);
    if (f.failure == Failure::PublishNoEffect || f.failure == Failure::PublishNoEffectReleaseResources) return NR_DETOUR_TX_NO_EFFECT;
    if (f.failure == Failure::PublishUnknown) return NR_DETOUR_TX_UNKNOWN;
    f.published = true;
    return NR_DETOUR_TX_COMPLETE;
}
static nr_detour_tx_outcome gate(void *p) {
    auto &f = *static_cast<Fake *>(p);
    if (f.failure == Failure::GateNoEffect) return NR_DETOUR_TX_NO_EFFECT;
    if (f.failure == Failure::GateUnknown) return NR_DETOUR_TX_UNKNOWN;
    f.gate = true;
    return NR_DETOUR_TX_COMPLETE;
}
static nr_detour_tx_outcome writable(void *p) {
    auto &f = *static_cast<Fake *>(p);
    if (f.failure == Failure::WritableNoEffect) return NR_DETOUR_TX_NO_EFFECT;
    if (f.failure == Failure::WritableUnknown) return NR_DETOUR_TX_UNKNOWN;
    f.writable = true;
    return NR_DETOUR_TX_COMPLETE;
}
static nr_detour_tx_outcome write_target(void *p, const uint8_t *bytes,
                                         size_t length) {
    auto &f = *static_cast<Fake *>(p);
    ++f.write_calls;
    if (f.write_calls == 1 && f.failure == Failure::WriteNoEffect)
        return NR_DETOUR_TX_NO_EFFECT;
    if (f.write_calls == 1 && f.failure == Failure::WritePartial) {
        std::memcpy(f.target.data(), bytes, length / 2);
        return NR_DETOUR_TX_UNKNOWN;
    }
    if (f.write_calls > 1 && f.failure == Failure::RollbackWrite)
        return NR_DETOUR_TX_UNKNOWN;
    std::memcpy(f.target.data(), bytes, length);
    return NR_DETOUR_TX_COMPLETE;
}
static int verify(void *p, const uint8_t *expected, size_t length) {
    auto &f = *static_cast<Fake *>(p);
    if (f.write_calls == 1 && f.failure == Failure::VerifyPatch) return 0;
    if (f.write_calls > 1 && f.failure == Failure::VerifyRollback) return 0;
    return std::memcmp(f.target.data(), expected, length) == 0;
}
static nr_detour_tx_outcome restore_protection(void *p) {
    auto &f = *static_cast<Fake *>(p);
    if (f.failure == Failure::RestoreProtection) return NR_DETOUR_TX_UNKNOWN;
    f.writable = false;
    return NR_DETOUR_TX_COMPLETE;
}
static nr_detour_tx_outcome release_gate(void *p) {
    auto &f = *static_cast<Fake *>(p);
    if (f.failure == Failure::ReleaseGate) return NR_DETOUR_TX_UNKNOWN;
    f.gate = false;
    return NR_DETOUR_TX_COMPLETE;
}
static nr_detour_tx_outcome release_resources(void *p) {
    auto &f = *static_cast<Fake *>(p);
    if (f.failure == Failure::ReleaseResources) return NR_DETOUR_TX_UNKNOWN;
    f.resources_released = true;
    return NR_DETOUR_TX_COMPLETE;
}

static nr_detour_tx_ops make_ops(Fake &f) {
    nr_detour_tx_ops ops{};
    ops.context = &f;
    ops.preflight = preflight;
    ops.prepare_resources = prepare;
    ops.publish_resources = publish;
    ops.acquire_patch_window = gate;
    ops.make_target_writable = writable;
    ops.write_target = write_target;
    ops.verify_target = verify;
    ops.restore_target_protection = restore_protection;
    ops.release_patch_window = release_gate;
    ops.release_resources = release_resources;
    return ops;
}

static void expect_aborted_without_target_change(Failure failure) {
    Fake f;
    f.failure = failure;
    const auto original = f.target;
    const std::array<uint8_t, 8> patch{{9,9,9,9,9,9,9,9}};
    auto ops = make_ops(f);
    auto result = nr_detour_tx_install(&ops, original.data(), patch.data(), patch.size());
    assert(result.state == NR_DETOUR_TX_ABORTED);
    assert(!result.target_may_be_modified);
    assert(f.target == original);
}

int main() {
    const std::array<uint8_t, 8> original{{1,2,3,4,5,6,7,8}};
    const std::array<uint8_t, 8> patch{{9,8,7,6,5,4,3,2}};

    {
        Fake f;
        auto ops = make_ops(f);
        auto result = nr_detour_tx_install(&ops, original.data(), patch.data(), patch.size());
        assert(result.state == NR_DETOUR_TX_ACTIVE);
        assert(f.target == patch);
        assert(!f.gate && !f.writable && f.prepared && f.published);
        assert(!f.resources_released);
    }

    expect_aborted_without_target_change(Failure::PrepareNoEffect);
    expect_aborted_without_target_change(Failure::PublishNoEffect);
    expect_aborted_without_target_change(Failure::GateNoEffect);
    expect_aborted_without_target_change(Failure::WritableNoEffect);
    expect_aborted_without_target_change(Failure::WriteNoEffect);

    for (Failure failure : {Failure::PrepareUnknown, Failure::PublishUnknown,
                            Failure::GateUnknown, Failure::WritableUnknown,
                            Failure::RestoreProtection, Failure::ReleaseGate}) {
        Fake f;
        f.failure = failure;
        auto ops = make_ops(f);
        auto result = nr_detour_tx_install(&ops, original.data(), patch.data(), patch.size());
        assert(result.state == NR_DETOUR_TX_POISONED);
        assert(result.resources_may_be_owned);
    }

    {\n        Fake f;\n        f.failure = Failure::PublishNoEffectReleaseResources;\n        auto ops = make_ops(f);\n        auto result = nr_detour_tx_install(&ops, original.data(), patch.data(), patch.size());\n        assert(result.state == NR_DETOUR_TX_POISONED);\n        assert(!result.target_may_be_modified && result.resources_may_be_owned);\n    }\n\n    for (Failure failure : {Failure::WritePartial, Failure::VerifyPatch}) {
        Fake f;
        f.failure = failure;
        auto ops = make_ops(f);
        auto result = nr_detour_tx_install(&ops, original.data(), patch.data(), patch.size());
        assert(result.state == NR_DETOUR_TX_ABORTED);
        assert(f.target == original);
        assert(!result.target_may_be_modified);
        assert(f.resources_released);
        assert(!f.gate && !f.writable);
    }

    for (Failure failure : {Failure::RollbackWrite, Failure::VerifyRollback}) {
        Fake f;
        f.failure = failure;
        auto ops = make_ops(f);
        auto result = nr_detour_tx_install(&ops, original.data(), patch.data(), patch.size());
        assert(result.state == NR_DETOUR_TX_POISONED);
        assert(result.target_may_be_modified);
        assert(result.patch_window_may_be_held);
        assert(result.resources_may_be_owned);
        assert(!f.resources_released);
    }

    std::puts("detour transaction core: success, pre-mutation failures, rollback, and poisoned retention passed");
    return 0;
}
