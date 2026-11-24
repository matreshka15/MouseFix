#pragma once

#include <stdbool.h>

/* Store license state. The app fails OPEN: anything it cannot determine
   cleanly runs as the full version, so a user is never locked out by a
   licensing hiccup (offline, sideloaded build, query failure). */
typedef enum
{
	STORE_LICENSE_UNKNOWN = 0,  /* no package identity / query failed -> full */
	STORE_LICENSE_FULL,         /* purchased, or active and not a trial */
	STORE_LICENSE_TRIAL,        /* active time-limited trial */
	STORE_LICENSE_EXPIRED       /* trial over, not purchased */
} StoreLicenseState;

typedef struct
{
	StoreLicenseState state;
	int days_remaining;         /* valid only when state == STORE_LICENSE_TRIAL */
} StoreLicenseInfo;

/* Map a successful license read to a state. Pure so the unit test can cover
   it without talking to the Store. */
static inline StoreLicenseState store_license_map(bool is_active, bool is_trial)
{
	if (!is_active)
		return STORE_LICENSE_EXPIRED;
	if (is_trial)
		return STORE_LICENSE_TRIAL;
	return STORE_LICENSE_FULL;
}

/* Resolve a query outcome to a state, keeping the unknown path distinct so
   the test can cover the "unknown" mapping too. */
static inline StoreLicenseState store_license_resolve(bool query_ok, bool is_active, bool is_trial)
{
	if (!query_ok)
		return STORE_LICENSE_UNKNOWN;
	return store_license_map(is_active, is_trial);
}

#ifdef __cplusplus
extern "C" {
#endif

/* Query the Store license. Blocks until the async call returns, so call it
   from a worker thread. Never fails hard: any error yields UNKNOWN. */
void store_license_query(StoreLicenseInfo *out);

/* Try to open the native rate-and-review dialog. Returns true when the native
   dialog was launched; false when it is unavailable (an OS too old to expose
   it, or no package identity), in which case the caller should fall back to
   the ms-windows-store review URL. Does not block the caller. */
bool store_license_request_rate_and_review(void);

#ifdef __cplusplus
}
#endif
