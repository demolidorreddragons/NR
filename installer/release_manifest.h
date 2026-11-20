#ifndef PNE_RELEASE_MANIFEST_H
#define PNE_RELEASE_MANIFEST_H

#include <stddef.h>

/* Validate package identity and the SHA-256 of its bundled PRX before writes. */
int pne_validate_release_manifest(const char* manifest_path,
                                  const char* prx_path,
                                  char* error,
                                  size_t error_size);

#endif
