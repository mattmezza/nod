/* SPDX-License-Identifier: MIT */
#include "cui_questionnaire_observer_internal.h"
#include "cui_questionnaire_snapshot_math.h"
#include <string.h>
static void cui_questionnaire_observer_increment(size_t *n, bool *overflow) {
    if (*n == SIZE_MAX)
        *overflow = true;
    else
        ++*n;
}
static void cui_questionnaire_observer_clear(cui_questionnaire_observer *observer) {
    if (observer->private_state)
        observer->private_state->observer = NULL;
    observer->private_state = NULL;
    observer->private_frames = NULL;
    observer->private_frame_count = observer->private_used = 0;
    observer->private_handler = (cui_questionnaire_observation_handler){0};
    observer->private_target = 0;
}
cui_questionnaire_observer_result
cui_questionnaire_observer_attach(cui_questionnaire_state *s, cui_questionnaire_observer *observer,
                                  cui_id target, cui_questionnaire_observer_frame *frames,
                                  size_t count, cui_questionnaire_observation_handler handler) {
    if (!s || !s->initialized || !observer || !target || (count && !frames) ||
        !cui_questionnaire_snapshot_extent(count, sizeof *frames))
        return CUI_QUESTIONNAIRE_OBSERVER_INVALID;
    if (observer->private_inflight || (s->observer && s->observer->private_inflight))
        return CUI_QUESTIONNAIRE_OBSERVER_BUSY;
    if (observer->private_state || s->observer)
        return CUI_QUESTIONNAIRE_OBSERVER_ATTACHED;
    for (size_t i = 0; i < count; ++i) {
        cui_questionnaire_snapshot_workspace w = frames[i].workspace;
        if ((w.item_capacity && !w.items) || (w.choice_capacity && !w.choices) ||
            (w.text_capacity && !w.text))
            return CUI_QUESTIONNAIRE_OBSERVER_INVALID;
    }
    *observer = (cui_questionnaire_observer){.private_state = s,
                                             .private_frames = frames,
                                             .private_frame_count = count,
                                             .private_handler = handler,
                                             .private_target = target,
                                             .private_active = true};
    s->observer = observer;
    return CUI_QUESTIONNAIRE_OBSERVER_OK;
}
cui_questionnaire_observer_result cui_questionnaire_observer_detach(cui_questionnaire_observer *o) {
    if (!o)
        return CUI_QUESTIONNAIRE_OBSERVER_INVALID;
    o->private_active = false;
    if (o->private_inflight)
        return CUI_QUESTIONNAIRE_OBSERVER_BUSY;
    cui_questionnaire_observer_clear(o);
    return CUI_QUESTIONNAIRE_OBSERVER_OK;
}
cui_questionnaire_observer_statistics
cui_questionnaire_observer_get_statistics(const cui_questionnaire_observer *o) {
    return o ? o->private_statistics : (cui_questionnaire_observer_statistics){0};
}
static cui_questionnaire_snapshot_result
cui_questionnaire_observer_names(cui_questionnaire_state *s, size_t item, size_t previous,
                                 cui_questionnaire_snapshot_workspace w,
                                 cui_questionnaire_observation *payload) {
    size_t indices[] = {previous, item};
    size_t bytes = 0;
    for (size_t n = 0; n < 2; ++n) {
        if (indices[n] >= s->count)
            continue;
        if (!cui_questionnaire_snapshot_add(&bytes, s->items[indices[n]].name.length) ||
            !cui_questionnaire_snapshot_add(&bytes, 1))
            return CUI_QUESTIONNAIRE_SNAPSHOT_OVERFLOW;
    }
    payload->required.text_bytes = bytes;
    if (bytes > w.text_capacity)
        return CUI_QUESTIONNAIRE_SNAPSHOT_CAPACITY;
    size_t offset = 0;
    for (size_t n = 0; n < 2; ++n) {
        if (indices[n] >= s->count)
            continue;
        cui_str source = s->items[indices[n]].name;
        char *text = w.text + offset;
        if (source.length)
            memcpy(text, source.data, source.length);
        text[source.length] = 0;
        offset += source.length + 1;
        if (n == 0) {
            payload->has_previous_name = true;
            payload->previous_name = (cui_str){text, source.length};
        } else {
            payload->has_current_name = true;
            payload->current_name = (cui_str){text, source.length};
        }
    }
    return CUI_QUESTIONNAIRE_SNAPSHOT_OK;
}
cui_questionnaire_observation_scope
cui_questionnaire_observation_begin(cui_questionnaire_state *s, cui_questionnaire_event_kind kind,
                                    size_t item, size_t previous) {
    cui_questionnaire_observation_scope scope = {0};
    cui_questionnaire_observer *o = s->observer;
    if (!o || !o->private_active)
        return scope;
    scope.observer = o;
    ++o->private_inflight;
    scope.payload.item = item;
    scope.payload.previous = previous;
    if (!o->private_handler.callback)
        return scope;
    if (o->private_used == o->private_frame_count) {
        scope.payload.payload_status = CUI_QUESTIONNAIRE_PAYLOAD_NO_FRAME;
        return scope;
    }
    cui_questionnaire_snapshot_workspace w = o->private_frames[o->private_used++].workspace;
    scope.frame = true;
    cui_questionnaire_snapshot_result result;
    if (kind == CUI_QUESTIONNAIRE_CURRENT_CHANGED)
        result = cui_questionnaire_observer_names(s, item, previous, w, &scope.payload);
    else if (kind == CUI_QUESTIONNAIRE_ANSWER_CHANGED)
        result = cui_questionnaire_answer_copy(s, item, w, &scope.payload.required,
                                               &scope.payload.answer);
    else
        result = cui_questionnaire_submission_copy(s, w, &scope.payload.required,
                                                   &scope.payload.submission);
    if (result == CUI_QUESTIONNAIRE_SNAPSHOT_OK)
        return scope;
    scope.payload.payload_status =
        result == CUI_QUESTIONNAIRE_SNAPSHOT_CAPACITY   ? CUI_QUESTIONNAIRE_PAYLOAD_CAPACITY
        : result == CUI_QUESTIONNAIRE_SNAPSHOT_OVERFLOW ? CUI_QUESTIONNAIRE_PAYLOAD_OVERFLOW
                                                        : CUI_QUESTIONNAIRE_PAYLOAD_INVALID;
    if (result != CUI_QUESTIONNAIRE_SNAPSHOT_CAPACITY)
        scope.payload.required = (cui_questionnaire_snapshot_requirements){0};
    return scope;
}
void cui_questionnaire_observation_deliver(cui_questionnaire_observation_scope *scope,
                                           cui_questionnaire_event_kind kind) {
    cui_questionnaire_observer *o = scope->observer;
    if (!o || !o->private_active)
        return;
    cui_questionnaire_observer_statistics *stats = &o->private_statistics;
    cui_questionnaire_observer_increment(&stats->attempted, &stats->count_overflow);
    if (!o->private_handler.callback)
        return;
    scope->payload.kind = kind;
    cui_component_event_kind event_kind =
        kind == CUI_QUESTIONNAIRE_COMPLETED ? CUI_COMPONENT_EVENT_COMMIT
        : kind == CUI_QUESTIONNAIRE_SUBMIT  ? CUI_COMPONENT_EVENT_SUBMIT
                                            : CUI_COMPONENT_EVENT_CHANGE;
    (void)cui_component_event_init(&scope->payload.event, event_kind, o->private_target,
                                   CUI_COMPONENT_EVENT_SOURCE_UNKNOWN);
    cui_questionnaire_observer_increment(&stats->delivered, &stats->count_overflow);
    if (scope->payload.payload_status != CUI_QUESTIONNAIRE_PAYLOAD_READY)
        cui_questionnaire_observer_increment(&stats->payload_failures, &stats->count_overflow);
    o->private_handler.callback(o->private_handler.user, &scope->payload);
}
void cui_questionnaire_observation_end(cui_questionnaire_observation_scope *scope) {
    cui_questionnaire_observer *o = scope->observer;
    if (!o)
        return;
    if (scope->frame)
        --o->private_used;
    --o->private_inflight;
    if (!o->private_active && !o->private_inflight)
        cui_questionnaire_observer_clear(o);
}
