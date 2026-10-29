/* SPDX-License-Identifier: MIT */
#ifndef CUI_TEXTAREA_GEOMETRY_H
#define CUI_TEXTAREA_GEOMETRY_H
#include "cui/cui_textarea.h"
/* Cached handles are borrowed only until next begin_frame; owned is used outside frames. */
cui_text_layout_status cui_textarea_visual_geometry(cui_context *, cui_str, cui_textarea_line,
                                                    float, cui_text_style, cui_text_geometry *owned,
                                                    const cui_text_geometry **out);
float cui_textarea_visual_x(cui_context *, cui_str, cui_textarea_line, size_t, float,
                            cui_text_style, cui_text_affinity);
bool cui_textarea_visual_paint_range(cui_context *, cui_id, cui_str, cui_textarea_line, size_t,
                                     size_t, float, cui_text_style, cui_color, float, float);
#endif
