/* SPDX-License-Identifier: MIT */
#ifndef CUI_QUESTIONNAIRE_SNAPSHOT_MATH_H
#define CUI_QUESTIONNAIRE_SNAPSHOT_MATH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Private checked arithmetic, shared with a safe boundary unit probe. */
static inline bool cui_questionnaire_snapshot_add(size_t *sum, size_t amount) {
    if (amount > SIZE_MAX - *sum)
        return false;
    *sum += amount;
    return true;
}
static inline bool cui_questionnaire_snapshot_extent(size_t count, size_t width) {
    return !width || count <= SIZE_MAX / width;
}
#endif
