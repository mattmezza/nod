/* SPDX-License-Identifier: MIT */
#ifndef CUI_REGEX_H
#define CUI_REGEX_H
#include "cui_text_match.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional pinned Rust-regex provider; no Rust types or default Core dependency.
 * Zero-initialize handles. Same-thread/external synchronization required.
 * Compile/session creation may allocate; begin/match/end never allocate.
 * Source parser/compiler OOM and WASI panics may terminate the process. */
typedef struct {
    void *private_state;
    uint64_t generation;
} cui_regex;
typedef struct {
    void *private_state;
    uint64_t generation;
} cui_regex_session;
typedef enum {
    CUI_REGEX_OK = 0,
    CUI_REGEX_INVALID,
    CUI_REGEX_CAPACITY,
    CUI_REGEX_STALE,
    CUI_REGEX_BUSY,
    CUI_REGEX_FAILED,
    CUI_REGEX_PATTERN
} cui_regex_result;
enum {
    CUI_REGEX_CASE_INSENSITIVE = 1u << 0,
    CUI_REGEX_MULTI_LINE = 1u << 1,
    CUI_REGEX_DOT_NEW_LINE = 1u << 2,
    CUI_REGEX_CRLF = 1u << 3,
    CUI_REGEX_SWAP_GREED = 1u << 4,
    CUI_REGEX_IGNORE_WHITESPACE = 1u << 5,
    CUI_REGEX_DISABLE_UNICODE = 1u << 6,
    CUI_REGEX_OCTAL = 1u << 7,
    CUI_REGEX_LINE_TERMINATOR = 1u << 8,
    CUI_REGEX_EXPLICIT_LIMITS = 1u << 9
};
typedef struct {
    uint32_t flags, line_terminator;               /* With flag: byte0..255, otherwise must be0. */
    size_t size_limit, dfa_size_limit, nest_limit; /* Explicit flag, else all0. */
    size_t max_pattern_bytes, max_text_bytes;      /* 0 => 1MiB each. */
} cui_regex_options;
/* Exact Source RegexBuilder admission precedes capture-free Pike compilation.
 * Empty output handles required; outputs unchanged on failure. Text is immutable
 * valid UTF-8. Pattern is copied by compile; sessions retain compiled ownership.
 * Known input/handle extents must not overlap; forged pointers are unsupported. */
cui_regex_result cui_regex_compile(cui_str, cui_regex_options, cui_regex *out);
cui_regex_result cui_regex_destroy(cui_regex *);
/* Cache budget is a hard heap-byte bound (excluding handle/NFA/allocator overhead).
 * Rebind prepares first and replaces atomically; existing descriptors become STALE.
 * Compiled handle may be destroyed while sessions remain alive. */
cui_regex_result cui_regex_session_create(const cui_regex *, size_t cache_budget,
                                          cui_regex_session *out);
cui_regex_result cui_regex_session_rebind(cui_regex_session *, const cui_regex *,
                                          size_t cache_budget);
cui_regex_result cui_regex_session_destroy(cui_regex_session *);
/* Borrowed descriptor: session must remain alive throughout checked call. Destroy
 * and rebind return BUSY during a lease. Scratch is empty; all runtime state is
 * prepared session-owned. Direct callback API obeys the same lease rules. */
cui_text_match cui_regex_matcher(cui_regex_session *);
#ifdef __cplusplus
}
#endif
#endif
