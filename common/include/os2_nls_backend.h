#ifndef OS2_NLS_BACKEND_H
#define OS2_NLS_BACKEND_H

#include <stdint.h>

/*
 * Host-service contract for backend-neutral OS/2 NLS state.
 *
 * The common layer owns all guest-visible country/codepage/table semantics.
 * Backends may suggest an initial host profile, but the common layer validates
 * it against the OS/2 data it actually implements and remains authoritative
 * after session initialization.
 */
struct Os2NlsBackendOps {
    int (*query_initial_profile)(void *opaque,
                                 uint32_t *country,
                                 uint32_t *codepage);
};

#endif
