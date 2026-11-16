#ifndef DMNOTIFY_H
#define DMNOTIFY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "dmod_types.h"
#include "dmnotify_defs.h"
#include "dmnotify_types.h"

/**
 * Public API for the dmnotify module.
 *
 * Functions are declared with the dmod_dmnotify_api(...) macro - dmod's
 * standard pattern for functions callable from other modules (or from this
 * module's own tests/), resolved dynamically by the loader rather than
 * through normal static linkage. See dm_sw_ring/include/dm_sw_ring.h for a
 * fully worked real-world example of the same shape.
 *
 * Definitions in src/dmnotify.c use the matching
 * dmod_dmnotify_api_declaration(...) macro - a plain C function
 * definition here will NOT satisfy these declarations at link time.
 *
 * This is an example interface using the usual "opaque handle" pattern -
 * replace the handle, functions, and struct definition in
 * src/dmnotify.c with your module's real API.
 */

/* Opaque handle - the real struct is defined in src/dmnotify.c */
typedef struct dmnotify* dmnotify_t;

/**
 * Create a new dmnotify instance.
 *
 * @return A valid handle on success, or NULL on allocation failure.
 */
dmod_dmnotify_api(1.0, dmnotify_t, _create, ( void ));

/**
 * Destroy an instance created by dmnotify_create(). Safe to call with
 * NULL.
 */
dmod_dmnotify_api(1.0, void, _destroy, ( dmnotify_t handle ));

/**
 * Example accessor - replace with your module's real API.
 *
 * @return true if handle is a valid, non-NULL instance.
 */
dmod_dmnotify_api(1.0, bool, _is_valid, ( dmnotify_t handle ));

dmod_dmnotify_dif(1.0, void, _on_notification, ( void* user_ptr ) );

#endif // DMNOTIFY_H
