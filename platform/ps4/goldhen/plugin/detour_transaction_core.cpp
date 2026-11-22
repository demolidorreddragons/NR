#include "detour_transaction_core.h"

#include <cstring>

static nr_detour_tx_result result_for(nr_detour_tx_state state) {
    nr_detour_tx_result result{};
    result.state = state;
    return result;
}

static int ops_complete(const nr_detour_tx_ops *ops) {
    return ops && ops->preflight && ops->prepare_resources &&
           ops->publish_resources && ops->acquire_patch_window &&
           ops->acquire_remove_window_and_drain && ops->make_target_writable && ops->write_target &&
           ops->verify_target && ops->restore_target_protection &&
           ops->release_patch_window && ops->release_resources;
}

static nr_detour_tx_result poison(int modified, int window, int resources) {
    nr_detour_tx_result result = result_for(NR_DETOUR_TX_POISONED);
    result.target_may_be_modified = modified;
    result.patch_window_may_be_held = window;
    result.resources_may_be_owned = resources;
    return result;
}

static nr_detour_tx_result cleanup_before_mutation(const nr_detour_tx_ops *ops,
                                                    int window_held,
                                                    int resources_owned) {
    if (window_held &&
        ops->release_patch_window(ops->context) != NR_DETOUR_TX_COMPLETE)
        return poison(0, 1, resources_owned);

    if (resources_owned &&
        ops->release_resources(ops->context) != NR_DETOUR_TX_COMPLETE)
        return poison(0, 0, 1);

    return result_for(NR_DETOUR_TX_ABORTED);
}

static nr_detour_tx_result cleanup_after_writable(const nr_detour_tx_ops *ops,
                                                   int resources_owned) {
    if (ops->restore_target_protection(ops->context) !=
        NR_DETOUR_TX_COMPLETE)
        return poison(0, 1, resources_owned);
    return cleanup_before_mutation(ops, 1, resources_owned);
}

static nr_detour_tx_result rollback(const nr_detour_tx_ops *ops,
                                    const uint8_t *original, size_t length) {
    if (ops->write_target(ops->context, original, length) !=
        NR_DETOUR_TX_COMPLETE)
        return poison(1, 1, 1);
    if (!ops->verify_target(ops->context, original, length))
        return poison(1, 1, 1);

    if (ops->restore_target_protection(ops->context) !=
        NR_DETOUR_TX_COMPLETE)
        return poison(0, 1, 1);
    if (ops->release_patch_window(ops->context) !=
        NR_DETOUR_TX_COMPLETE)
        return poison(0, 1, 1);
    if (ops->release_resources(ops->context) != NR_DETOUR_TX_COMPLETE)
        return poison(0, 0, 1);

    return result_for(NR_DETOUR_TX_ABORTED);
}

extern "C" nr_detour_tx_result nr_detour_tx_install(
    const nr_detour_tx_ops *ops, const uint8_t *original,
    const uint8_t *patch, size_t length) {
    nr_detour_tx_result result;
    nr_detour_tx_outcome outcome;

    if (!ops_complete(ops) || !original || !patch || length == 0)
        return result_for(NR_DETOUR_TX_ABORTED);

    if (!ops->preflight(ops->context, original, patch, length))
        return result_for(NR_DETOUR_TX_ABORTED);

    result = result_for(NR_DETOUR_TX_PREFLIGHTED);

    outcome = ops->prepare_resources(ops->context);
    if (outcome == NR_DETOUR_TX_NO_EFFECT)
        return result_for(NR_DETOUR_TX_ABORTED);
    if (outcome != NR_DETOUR_TX_COMPLETE)
        return poison(0, 0, 1);
    result.state = NR_DETOUR_TX_RESOURCES_READY;
    result.resources_may_be_owned = 1;

    outcome = ops->publish_resources(ops->context);
    if (outcome == NR_DETOUR_TX_NO_EFFECT)
        return cleanup_before_mutation(ops, 0, 1);
    if (outcome != NR_DETOUR_TX_COMPLETE)
        return poison(0, 0, 1);

    outcome = ops->acquire_patch_window(ops->context);
    if (outcome == NR_DETOUR_TX_NO_EFFECT)
        return cleanup_before_mutation(ops, 0, 1);
    if (outcome != NR_DETOUR_TX_COMPLETE)
        return poison(0, 1, 1);

    result.state = NR_DETOUR_TX_PATCH_WINDOW_HELD;
    result.patch_window_may_be_held = 1;

    outcome = ops->make_target_writable(ops->context);
    if (outcome == NR_DETOUR_TX_NO_EFFECT)
        return cleanup_before_mutation(ops, 1, 1);
    if (outcome != NR_DETOUR_TX_COMPLETE)
        return poison(0, 1, 1);

    result.state = NR_DETOUR_TX_TARGET_WRITE_STARTED;
    result.target_may_be_modified = 1;

    outcome = ops->write_target(ops->context, patch, length);
    if (outcome == NR_DETOUR_TX_NO_EFFECT)
        return cleanup_after_writable(ops, 1);
    if (outcome == NR_DETOUR_TX_UNKNOWN)
        return rollback(ops, original, length);

    if (!ops->verify_target(ops->context, patch, length))
        return rollback(ops, original, length);

    if (ops->restore_target_protection(ops->context) !=
        NR_DETOUR_TX_COMPLETE)
        return poison(1, 1, 1);

    result.state = NR_DETOUR_TX_TARGET_VERIFIED;
    result.target_may_be_modified = 1;

    if (ops->release_patch_window(ops->context) !=
        NR_DETOUR_TX_COMPLETE)
        return poison(1, 1, 1);

    result.state = NR_DETOUR_TX_ACTIVE;
    result.patch_window_may_be_held = 0;
    result.resources_may_be_owned = 1;
    return result;
}

extern "C" nr_detour_tx_result nr_detour_tx_remove(
    const nr_detour_tx_ops *ops, const uint8_t *original,
    const uint8_t *patch, size_t length) {
    if (!ops_complete(ops) || !original || !patch || length == 0)
        return result_for(NR_DETOUR_TX_POISONED);

    if (!ops->preflight(ops->context, original, patch, length))
        return poison(1, 0, 1);

    nr_detour_tx_outcome outcome =
        ops->acquire_remove_window_and_drain(ops->context);
    if (outcome == NR_DETOUR_TX_NO_EFFECT) {
        nr_detour_tx_result active = result_for(NR_DETOUR_TX_ACTIVE);
        active.target_may_be_modified = 1;
        active.resources_may_be_owned = 1;
        return active;
    }
    if (outcome != NR_DETOUR_TX_COMPLETE)
        return poison(1, 1, 1);

    outcome = ops->make_target_writable(ops->context);
    if (outcome == NR_DETOUR_TX_NO_EFFECT) {
        if (ops->release_patch_window(ops->context) !=
            NR_DETOUR_TX_COMPLETE)
            return poison(1, 1, 1);
        nr_detour_tx_result active = result_for(NR_DETOUR_TX_ACTIVE);
        active.target_may_be_modified = 1;
        active.resources_may_be_owned = 1;
        return active;
    }
    if (outcome != NR_DETOUR_TX_COMPLETE)
        return poison(1, 1, 1);

    outcome = ops->write_target(ops->context, original, length);
    if (outcome != NR_DETOUR_TX_COMPLETE ||
        !ops->verify_target(ops->context, original, length)) {
        /* The desired state is the original bytes: retry once while the
         * execution gate is still held and the target remains writable. */
        if (ops->write_target(ops->context, original, length) !=
            NR_DETOUR_TX_COMPLETE)
            return poison(1, 1, 1);
        if (!ops->verify_target(ops->context, original, length))
            return poison(1, 1, 1);
    }

    if (ops->restore_target_protection(ops->context) !=
        NR_DETOUR_TX_COMPLETE)
        return poison(0, 1, 1);

    if (ops->release_patch_window(ops->context) !=
        NR_DETOUR_TX_COMPLETE)
        return poison(0, 1, 1);

    if (ops->release_resources(ops->context) != NR_DETOUR_TX_COMPLETE)
        return poison(0, 0, 1);

    return result_for(NR_DETOUR_TX_RETIRED);
}
