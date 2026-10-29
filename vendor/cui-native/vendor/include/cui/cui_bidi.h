/* SPDX-License-Identifier: MIT */
#ifndef CUI_BIDI_H
#define CUI_BIDI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum {
    CUI_BIDI_AUTO_LTR,
    CUI_BIDI_AUTO_RTL,
    CUI_BIDI_LTR,
    CUI_BIDI_RTL
} cui_bidi_direction;
typedef enum { CUI_BIDI_OK, CUI_BIDI_INVALID, CUI_BIDI_CAPACITY, CUI_BIDI_MEMORY } cui_bidi_status;
typedef struct {
    size_t logical_start, length;
    uint8_t level;
} cui_bidi_run;
typedef struct {
    size_t scalar_count, run_count;
    uint8_t paragraph_level;
} cui_bidi_result;
/* One paragraph, one unwrapped line. All offsets are Unicode scalar indices.
 * Workspace holds decoded scalars and must have at least utf8_length entries.
 * Output buffers each need at least utf8_length entries (conservative bound).
 * Levels are logical; mapping/mirrors are visual. Runs are in visual order,
 * with logical ranges; odd levels read right-to-left. Formatting controls remain
 * represented and must not be painted. Outputs/result stay unchanged on failure;
 * workspace may change. Buffers must be disjoint. Backend uses transient heap
 * allocations, released before return; no global allocator changes are made. */
cui_bidi_status cui_bidi_resolve(const char *utf8, size_t utf8_length, cui_bidi_direction direction,
                                 uint32_t *workspace, size_t workspace_capacity, uint8_t *levels,
                                 size_t *visual_to_logical, uint32_t *mirrored, cui_bidi_run *runs,
                                 size_t output_capacity, cui_bidi_result *result);
/* Reusable paragraph analysis. Creation copies decoded scalars into an owned
 * object; input/workspace may be reused after return. Destroy exactly once.
 * Immutable objects may be read concurrently; destruction requires exclusive
 * ownership. Output paragraph pointer is unchanged on failure. */
typedef struct cui_bidi_paragraph cui_bidi_paragraph;
cui_bidi_status cui_bidi_paragraph_create(const char *utf8, size_t utf8_length,
                                          cui_bidi_direction direction, uint32_t *workspace,
                                          size_t workspace_capacity,
                                          cui_bidi_paragraph **paragraph);
void cui_bidi_paragraph_destroy(cui_bidi_paragraph *paragraph);
/* Copies logical paragraph levels before L1. Result run_count is zero.
 * Capacity must cover scalar_count. Outputs stay unchanged on failure. */
cui_bidi_status cui_bidi_paragraph_levels(const cui_bidi_paragraph *paragraph, uint8_t *levels,
                                          size_t capacity, cui_bidi_result *result);
/* A caller-selected nonempty line inside the paragraph, in scalar coordinates.
 * L1/L2 are applied to this range using the complete paragraph analysis.
 * Output buffers must be disjoint; each capacity must cover length. Levels are relative to the
 * line; mapping and run logical_start are absolute paragraph scalar offsets.
 * Result scalar_count is length. Outputs/result stay unchanged on failure.
 * Choose boundaries between complete shaping/grapheme clusters; this API does
 * not choose wrap positions or apply cluster-level L3 reordering. */
cui_bidi_status cui_bidi_paragraph_line(const cui_bidi_paragraph *paragraph, size_t logical_start,
                                        size_t length, uint8_t *levels, size_t *visual_to_logical,
                                        uint32_t *mirrored, cui_bidi_run *runs, size_t capacity,
                                        cui_bidi_result *result);
#ifdef __cplusplus
}
#endif
#endif
