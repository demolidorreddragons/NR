#include "hybrid_state.h"

pne_install_state pne_classify_install_state(int prx_exists,
                                             int manifest_exists,
                                             int manifest_valid,
                                             int transaction_marker_exists) {
    if (transaction_marker_exists) return PNE_STATE_RECOVERY;
    if (!prx_exists && !manifest_exists) return PNE_STATE_NEW;
    if (prx_exists && !manifest_exists) return PNE_STATE_LEGACY;
    if (!prx_exists && manifest_exists) return PNE_STATE_CONFLICT;
    return manifest_valid ? PNE_STATE_UPDATE : PNE_STATE_CONFLICT;
}

const char* pne_install_state_name(pne_install_state state) {
    switch (state) {
        case PNE_STATE_NEW: return "NEW";
        case PNE_STATE_UPDATE: return "UPDATE";
        case PNE_STATE_LEGACY: return "LEGACY";
        case PNE_STATE_RECOVERY: return "RECOVERY";
        case PNE_STATE_CONFLICT: return "CONFLICT";
        default: return "CONFLICT";
    }
}

int pne_compare_release(unsigned long installed, unsigned long incoming) {
    if (incoming < installed) return -1;
    if (incoming > installed) return 1;
    return 0;
}

int pne_recovery_candidate_is_compatible(int staged_file_valid, int manifest_valid,
                                         unsigned long staged_release, unsigned long incoming_release,
                                         int hashes_match) {
    if (!staged_file_valid || !manifest_valid || !hashes_match) return 0;
    return pne_compare_release(staged_release, incoming_release) >= 0;
}
