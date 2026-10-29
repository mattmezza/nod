/* SPDX-License-Identifier: MIT */
#ifndef CUI_TEXT_MATCH_H
#define CUI_TEXT_MATCH_H
#include "cui.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional portable synchronous boolean matcher. Zero descriptor disables it.
 * All extents are exact, borrowed for one checked edit only, and disjoint from
 * model, input, composition and presentation storage. Functions never retain
 * candidate bytes. A successful begin returns one non-null lease; end runs once
 * before adoption, even on rejection/error. Failure outputs remain unchanged.
 * Callers keep object/generation alive throughout begin/match/end. */
typedef enum {
    CUI_TEXT_MATCH_OK = 0,
    CUI_TEXT_MATCH_INVALID,
    CUI_TEXT_MATCH_CAPACITY,
    CUI_TEXT_MATCH_STALE,
    CUI_TEXT_MATCH_BUSY,
    CUI_TEXT_MATCH_FAILED
} cui_text_match_result;
typedef struct {
    void *data;
    size_t bytes;
} cui_text_match_buffer;
typedef struct {
    cui_text_match_result (*begin)(void *object, uint64_t generation, void **lease);
    cui_text_match_result (*match)(void *lease, cui_str text, cui_text_match_buffer scratch,
                                   bool *matched);
    cui_text_match_result (*end)(void *object, void *lease);
} cui_text_match_ops;
typedef struct {
    cui_text_match_ops ops;
    void *object;
    size_t object_bytes;
    uint64_t generation;
    cui_text_match_buffer scratch;
} cui_text_match;
#ifdef __cplusplus
}
#endif
#endif
