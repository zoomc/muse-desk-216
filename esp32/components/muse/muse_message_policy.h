#pragma once
#include <stdbool.h>
#include <stdint.h>
/* UTC epoch, independent of the host timezone; unknown clock fails closed. */
static inline bool muse_push_quiet(bool enabled, int64_t utc)
{
    if (!enabled) return false;
    if (utc < 1700000000) return true;
    unsigned hour = (unsigned)((utc + 8 * 3600) / 3600 % 24);
    return hour >= 23 || hour < 6;
}
static inline bool muse_push_allowed(bool enabled, bool own_only, bool quiet, int64_t utc)
{
    return enabled && !own_only && !muse_push_quiet(quiet, utc);
}
/* Muse omits reply parents: allow them only after this device's send ACK.
 * This is a request-window filter, not proof of origin for concurrent clients. */
static inline bool muse_reply_allowed(bool strict, bool bound, bool acked, bool parent_present, bool parent_matches)
{
    return !strict || bound || (acked && (!parent_present || parent_matches));
}
