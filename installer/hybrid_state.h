#ifndef PNE_HYBRID_STATE_H
#define PNE_HYBRID_STATE_H

typedef enum {
    PNE_STATE_NEW = 0,
    PNE_STATE_UPDATE,
    PNE_STATE_LEGACY,
    PNE_STATE_RECOVERY,
    PNE_STATE_CONFLICT
} pne_install_state;

pne_install_state pne_classify_install_state(int prx_exists,
                                             int manifest_exists,
                                             int manifest_valid,
                                             int transaction_marker_exists);
const char* pne_install_state_name(pne_install_state state);
/* Returns -1 when installed is older, 0 when equal, 1 when incoming is newer. */
int pne_compare_release(unsigned long installed, unsigned long incoming);
/* True only when a verified staged candidate matches the incoming hash and is not newer. */
int pne_recovery_candidate_is_compatible(int staged_file_valid, int manifest_valid,
                                         unsigned long staged_release, unsigned long incoming_release,
                                         int hashes_match);

#endif
