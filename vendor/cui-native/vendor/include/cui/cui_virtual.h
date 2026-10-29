/* SPDX-License-Identifier: MIT */
#ifndef CUI_VIRTUAL_H
#define CUI_VIRTUAL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct cui_virtual_list cui_virtual_list;
typedef struct {
    size_t capacity;
    double default_height;
} cui_virtual_config;
typedef enum {
    CUI_VIRTUAL_OK,
    CUI_VIRTUAL_INVALID,
    CUI_VIRTUAL_CAPACITY,
    CUI_VIRTUAL_DUPLICATE_KEY,
    CUI_VIRTUAL_NOT_FOUND
} cui_virtual_result;
typedef struct {
    size_t begin, end;
    double before, after, total, scroll;
} cui_virtual_view;
typedef struct {
    uint64_t key;
    size_t fallback_index;
    double within;
    bool valid;
} cui_virtual_anchor;
cui_virtual_list *cui_virtual_create(cui_virtual_config config);
void cui_virtual_destroy(cui_virtual_list *list);
/* Rebuild atomically. NULL heights retains measurements by key, assigning
   default_height to new keys. Zero is a valid key; duplicate keys reject. */
cui_virtual_result cui_virtual_rebuild(cui_virtual_list *list, const uint64_t *keys,
                                       const double *heights, size_t count);
size_t cui_virtual_count(const cui_virtual_list *list);
size_t cui_virtual_memory_bytes(const cui_virtual_list *list);
bool cui_virtual_key(const cui_virtual_list *list, size_t index, uint64_t *key);
bool cui_virtual_index(const cui_virtual_list *list, uint64_t key, size_t *index);
double cui_virtual_height(const cui_virtual_list *list, size_t index);
/* Prefix offset of index; index >= count returns total height. */
double cui_virtual_offset(const cui_virtual_list *list, size_t index);
double cui_virtual_total(const cui_virtual_list *list);
/* First row whose bottom is strictly beyond offset; returns count at end. */
size_t cui_virtual_lower_bound(const cui_virtual_list *list, double offset);
cui_virtual_result cui_virtual_set_height(cui_virtual_list *list, size_t index, double height);
cui_virtual_result cui_virtual_set_key_height(cui_virtual_list *list, uint64_t key, double height);
cui_virtual_view cui_virtual_viewport(const cui_virtual_list *list, double scroll, double viewport,
                                      size_t overscan);
cui_virtual_anchor cui_virtual_capture_anchor(const cui_virtual_list *list, double scroll);
double cui_virtual_restore_anchor(const cui_virtual_list *list, cui_virtual_anchor anchor,
                                  double viewport);
typedef enum {
    CUI_VIRTUAL_ALIGN_TOP,
    CUI_VIRTUAL_ALIGN_CENTER,
    CUI_VIRTUAL_ALIGN_BOTTOM,
    CUI_VIRTUAL_ALIGN_NEAREST
} cui_virtual_alignment;
/* Pure model helper; invalid input preserves output. Offsets are nonnegative.
 * For horizontal widgets TOP/BOTTOM mean left/right. */
cui_virtual_result cui_virtual_scroll_to(const cui_virtual_list *, size_t index, double viewport,
                                         double current, cui_virtual_alignment, double *output);
/* Stable keyed wrapper ID, also usable by custom model-only hosts. */
uint64_t cui_virtual_item_id(uint64_t root, uint64_t key);
#ifdef __cplusplus
}
#endif
#endif
