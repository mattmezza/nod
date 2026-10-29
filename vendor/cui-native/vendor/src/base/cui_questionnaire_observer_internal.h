/* SPDX-License-Identifier: MIT */
#ifndef CUI_QUESTIONNAIRE_OBSERVER_INTERNAL_H
#define CUI_QUESTIONNAIRE_OBSERVER_INTERNAL_H
#include "cui/cui_questionnaire_observer.h"
typedef struct {
    cui_questionnaire_observer *observer;
    cui_questionnaire_observation payload;
    bool frame;
} cui_questionnaire_observation_scope;
cui_questionnaire_observation_scope
cui_questionnaire_observation_begin(cui_questionnaire_state *, cui_questionnaire_event_kind,
                                    size_t item, size_t previous);
void cui_questionnaire_observation_deliver(cui_questionnaire_observation_scope *,
                                           cui_questionnaire_event_kind);
void cui_questionnaire_observation_end(cui_questionnaire_observation_scope *);
#endif
