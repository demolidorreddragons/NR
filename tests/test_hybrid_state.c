#include "../installer/hybrid_state.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(pne_classify_install_state(0,0,0,0) == PNE_STATE_NEW);
    assert(pne_classify_install_state(1,1,1,0) == PNE_STATE_UPDATE);
    assert(pne_classify_install_state(1,0,0,0) == PNE_STATE_LEGACY);
    assert(pne_classify_install_state(1,1,0,0) == PNE_STATE_CONFLICT);
    assert(pne_classify_install_state(0,1,0,0) == PNE_STATE_CONFLICT);
    assert(pne_classify_install_state(0,0,0,1) == PNE_STATE_RECOVERY);
    assert(pne_classify_install_state(1,1,1,1) == PNE_STATE_RECOVERY);
    assert(pne_compare_release(6,7) == 1);
    assert(pne_compare_release(7,7) == 0);
    assert(pne_compare_release(8,7) == -1);
    assert(pne_install_state_name(PNE_STATE_NEW)[0] == 'N');
    puts("Hybrid installer state-machine tests passed");
    return 0;
}
