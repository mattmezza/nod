/* SPDX-License-Identifier: MIT */
#ifndef CUI_QUESTIONNAIRE_PRESENTATION_PRIVATE_H
#define CUI_QUESTIONNAIRE_PRESENTATION_PRIVATE_H
#include "cui/cui_questionnaire.h"
typedef struct {
    float font, line, gap;
} cui_questionnaire_text_metrics;
bool cui_questionnaire_text_prepare(cui_context *, const cui_questionnaire_state *,
                                    cui_questionnaire_options *, cui_questionnaire_text_metrics *);
#endif
