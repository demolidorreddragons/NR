#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum nr_detour_tx_state {
    NR_DETOUR_TX_EMPTY = 0,
    NR_DETOUR_TX_PREFLIGHTED,
    NR_DETOUR_TX_RESOURCES_READY,
    NR_DETOUR_TX_PATCH_WINDOW_HELD,
    NR_DETOUR_TX_TARGET_WRITE_STARTED,
    NR_DETOUR_TX_TARGET_VERIFIED,
    NR_DETOUR_TX_ACTIVE,
    NR_DETOUR_TX_REMOVE_WINDOW_HELD,
    NR_DETOUR_TX_RESTORE_STARTED,
    NR_DETOUR_TX_RESTORED_VERIFIED,
    NR_DETOUR_TX_RETIRED,
    NR_DETOUR_TX_ROLLBACK_STARTED,
    NR_DETOUR_TX_ABORTED,
    NR_DETOUR_TX_POISONED
} nr_detour_tx_state;

typedef enum nr_detour_tx_outcome {
    /* Operation completed and its documented postcondition is true. */
    NR_DETOUR_TX_COMPLETE = 0,
    /* Operation is guaranteed to have made no side effect. */
    NR_DETOUR_TX_NO_EFFECT,
    /* Operation may have made a partial or otherwise uncertain side effect. */
    NR_DETOUR_TX_UNKNOWN
} nr_detour_tx_outcome;

typedef struct nr_detour_tx_ops {
    void *context;

    /* Pure validation only: must not mutate target or allocate executable memory. */
    int (*preflight)(void *context, const uint8_t *original,
                     const uint8_t *patch, size_t length);

    /* Prepare and fully populate trampoline/stub; target remains untouched. */
    nr_detour_tx_outcome (*prepare_resources)(void *context);
    /* Verify/publish generated executable resources before target mutation. */
    nr_detour_tx_outcome (*publish_resources)(void *context);

    /* Must exclude execution/fetch from target range until release_patch_window. */
    nr_detour_tx_outcome (*acquire_patch_window)(void *context);
    /* Removal gate: prevent new entries and drain target/hook/stub/trampoline execution. */
    nr_detour_tx_outcome (*acquire_remove_window_and_drain)(void *context);
    nr_detour_tx_outcome (*make_target_writable)(void *context);
    /* COMPLETE means all bytes were written; UNKNOWN means any prefix may be written. */
    nr_detour_tx_outcome (*write_target)(void *context,
                                         const uint8_t *bytes, size_t length);
    int (*verify_target)(void *context, const uint8_t *expected, size_t length);
    nr_detour_tx_outcome (*restore_target_protection)(void *context);
    nr_detour_tx_outcome (*release_patch_window)(void *context);

    /* May free prepared code only when target is known to contain original bytes. */
    nr_detour_tx_outcome (*release_resources)(void *context);
} nr_detour_tx_ops;

typedef struct nr_detour_tx_result {
    nr_detour_tx_state state;
    int target_may_be_modified;
    int patch_window_may_be_held;
    int resources_may_be_owned;
} nr_detour_tx_result;

/*
 * Host-testable transaction coordinator. This code performs no OS calls and is
 * not wired into the PS4 hook. Platform callbacks must satisfy the contracts
 * above; in particular, acquire_patch_window must be a real cross-thread
 * execution rendezvous, not an installer-only mutex.
 *
 * original and patch must each contain length readable bytes for the call.
 * A POISONED result requires retaining all callback-owned state/resources.
 */
nr_detour_tx_result nr_detour_tx_install(const nr_detour_tx_ops *ops,
                                         const uint8_t *original,
                                         const uint8_t *patch,
                                         size_t length);

/* Remove an ACTIVE hook. A failed pre-write gate/protection step returns ACTIVE
 * only when the target is still patched and the gate is known to be released.
 * Uncertain restore/drain/cleanup outcomes return POISONED and retain resources.
 */
nr_detour_tx_result nr_detour_tx_remove(const nr_detour_tx_ops *ops,
                                        const uint8_t *original,
                                        const uint8_t *patch,
                                        size_t length);

#ifdef __cplusplus
}
#endif
