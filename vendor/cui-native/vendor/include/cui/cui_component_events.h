/* SPDX-License-Identifier: MIT */
#ifndef CUI_COMPONENT_EVENTS_H
#define CUI_COMPONENT_EVENTS_H
#include "cui_edit.h"
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Shared metadata only: component-specific values/reasons remain typed separately. */
typedef enum {
    CUI_COMPONENT_EVENT_ACTIVATE,
    CUI_COMPONENT_EVENT_REQUEST,
    CUI_COMPONENT_EVENT_CHANGE,
    CUI_COMPONENT_EVENT_COMMIT,
    CUI_COMPONENT_EVENT_SUBMIT,
    CUI_COMPONENT_EVENT_LIFECYCLE
} cui_component_event_kind;
typedef enum {
    CUI_COMPONENT_EVENT_SOURCE_UNKNOWN,
    CUI_COMPONENT_EVENT_SOURCE_PROGRAMMATIC,
    CUI_COMPONENT_EVENT_SOURCE_POINTER,
    CUI_COMPONENT_EVENT_SOURCE_KEYBOARD,
    CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY
} cui_component_event_source;
typedef enum {
    CUI_COMPONENT_EVENT_HAS_FRAME = 1u,
    CUI_COMPONENT_EVENT_HAS_INDEX = 2u,
    CUI_COMPONENT_EVENT_HAS_SOURCE = 4u,
    CUI_COMPONENT_EVENT_HAS_ACTIVATION = 8u
} cui_component_event_flags;
/* Pointer-free, copyable beyond callback/frame/context lifetime. Frame/index
 * identity is context-local: do not correlate across recreated contexts. Flags
 * alone establish presence; zero-filled unused activation is NOT evidence. */
typedef struct {
    cui_component_event_kind kind;
    cui_id target;
    unsigned flags;
    uint64_t frame_sequence;
    size_t event_index;
    cui_component_event_source source;
    cui_activation_snapshot activation;
} cui_component_event_metadata;
/* UNKNOWN omits HAS_SOURCE. PROGRAMMATIC is an explicit origin, not a fake
 * input event; neither origin adds frame/index/activation evidence. Failure
 * leaves output untouched. Nonzero target required. No global ID allocation. */
static inline bool cui_component_event_init(cui_component_event_metadata *out,
                                            cui_component_event_kind kind, cui_id target,
                                            cui_component_event_source source) {
    if (!out || !target || kind < CUI_COMPONENT_EVENT_ACTIVATE ||
        kind > CUI_COMPONENT_EVENT_LIFECYCLE || source < CUI_COMPONENT_EVENT_SOURCE_UNKNOWN ||
        source > CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY)
        return false;
    cui_component_event_metadata value;
    memset(&value, 0, sizeof(value));
    value.kind = kind;
    value.target = target;
    value.source = source;
    if (source != CUI_COMPONENT_EVENT_SOURCE_UNKNOWN)
        value.flags = CUI_COMPONENT_EVENT_HAS_SOURCE;
    *out = value;
    return true;
}
/* Record an explicitly known local frame/index pair. Never infer these from
 * a programmatic request. A copied activation's identity cannot be overwritten.
 * Zero frame and SIZE_MAX index are invalid. Failure preserves metadata. */
static inline bool cui_component_event_set_frame_index(cui_component_event_metadata *metadata,
                                                       uint64_t frame_sequence,
                                                       size_t event_index) {
    if (!metadata || !metadata->target || !frame_sequence || event_index == SIZE_MAX ||
        ((metadata->flags & CUI_COMPONENT_EVENT_HAS_ACTIVATION) &&
         (metadata->activation.frame_sequence != frame_sequence ||
          metadata->activation.event_index != event_index)))
        return false;
    metadata->frame_sequence = frame_sequence;
    metadata->event_index = event_index;
    metadata->flags |= CUI_COMPONENT_EVENT_HAS_FRAME | CUI_COMPONENT_EVENT_HAS_INDEX;
    return true;
}
/* Pass evidence obtained from cui_event_activation_snapshot (or an unchanged
 * copy). This copies it; it does not establish acceptance of arbitrary fabricated
 * snapshots. Unknown event types/zero identities fail without touching output.
 * Freeze evidence before mutable callbacks; this helper never consumes input. */
static inline bool cui_component_event_from_activation(cui_component_event_metadata *out,
                                                       cui_component_event_kind kind,
                                                       const cui_activation_snapshot *snapshot) {
    if (!out || !snapshot || !snapshot->frame_sequence || snapshot->event_index == SIZE_MAX)
        return false;
    cui_component_event_source source;
    switch (snapshot->event_type) {
    case CUI_EVENT_POINTER_UP:
        source = CUI_COMPONENT_EVENT_SOURCE_POINTER;
        break;
    case CUI_EVENT_KEY_DOWN:
    case CUI_EVENT_KEY_UP:
        source = CUI_COMPONENT_EVENT_SOURCE_KEYBOARD;
        break;
    case CUI_EVENT_ACCESSIBILITY_ACTION:
        source = CUI_COMPONENT_EVENT_SOURCE_ACCESSIBILITY;
        break;
    default:
        return false;
    }
    cui_component_event_metadata value;
    if (!cui_component_event_init(&value, kind, snapshot->target, source))
        return false;
    value.activation = *snapshot;
    value.frame_sequence = snapshot->frame_sequence;
    value.event_index = snapshot->event_index;
    value.flags |= CUI_COMPONENT_EVENT_HAS_FRAME | CUI_COMPONENT_EVENT_HAS_INDEX |
                   CUI_COMPONENT_EVENT_HAS_ACTIVATION;
    *out = value;
    return true;
}
typedef enum {
    CUI_COMPONENT_EVENT_COPY_OK,
    CUI_COMPONENT_EVENT_COPY_INVALID_ARGUMENT,
    CUI_COMPONENT_EVENT_COPY_LIMIT,
    CUI_COMPONENT_EVENT_COPY_CAPACITY,
    CUI_COMPONENT_EVENT_COPY_INVALID_UTF8
} cui_component_event_copy_status;
/* Copy borrowed UTF-8 into caller storage, adding NUL (not included in out.length).
 * maximum_bytes bounds payload scanning/copying; capacity includes NUL. Empty
 * {NULL,0} is valid. Embedded NUL/invalid UTF-8 fail using Base's existing policy.
 * Validate sizes before reading payload. No allocation or truncation. All failure
 * paths preserve storage and *out. Source/storage may overlap (memmove); output
 * descriptor must be disjoint from source/storage and remain caller-owned.
 * Caller guarantees source.length readable bytes and capacity writable bytes.
 * This copies text only, not cui_event or arbitrary pointer-containing payloads. */
static inline cui_component_event_copy_status
cui_component_event_copy_utf8(cui_str source, size_t maximum_bytes, char *storage, size_t capacity,
                              cui_str *out) {
    if (!out || !storage || (source.length && !source.data))
        return CUI_COMPONENT_EVENT_COPY_INVALID_ARGUMENT;
    if (source.length > maximum_bytes || source.length == SIZE_MAX)
        return CUI_COMPONENT_EVENT_COPY_LIMIT;
    if (capacity <= source.length)
        return CUI_COMPONENT_EVENT_COPY_CAPACITY;
    if (!cui_edit_valid_utf8(source))
        return CUI_COMPONENT_EVENT_COPY_INVALID_UTF8;
    if (source.length)
        memmove(storage, source.data, source.length);
    storage[source.length] = '\0';
    out->data = storage;
    out->length = source.length;
    return CUI_COMPONENT_EVENT_COPY_OK;
}
#ifdef __cplusplus
}
#endif
#endif
