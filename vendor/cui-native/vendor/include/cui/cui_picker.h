/* SPDX-License-Identifier: MIT */
#ifndef CUI_PICKER_H
#define CUI_PICKER_H
#include "cui_base.h"
#include "cui_overlay.h"
#include "cui_theme.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    cui_date start, end;
} cui_date_selection;
typedef bool (*cui_date_disabled_fn)(void *user, cui_date date);
typedef enum {
    CUI_DATE_ISO,
    CUI_DATE_MONTH_DAY_YEAR,
    CUI_DATE_DAY_MONTH_YEAR,
    CUI_DATE_YEAR_MONTH_DAY
} cui_date_order;
/* Borrowed UTF-8 labels; keep the table/strings alive while using the options.
 * NULL labels fall back to English. Numeric formatting uses the chosen order
 * and literal separator; this is not an OS locale or arbitrary pattern parser. */
typedef struct {
    const char *months[12], *weekdays[7]; /* weekdays indexed Sunday=0 */
    unsigned first_weekday;
    cui_date_order order;
    const char *separator;
    const char *calendar, *choose_year, *choose_month;
    const char *navigation[4];
} cui_date_locale;
typedef enum {
    CUI_DATE_LOCALE_EN_US,
    CUI_DATE_LOCALE_EN_GB,
    CUI_DATE_LOCALE_IT,
    CUI_DATE_LOCALE_FR,
    CUI_DATE_LOCALE_DE,
    CUI_DATE_LOCALE_ES,
    CUI_DATE_LOCALE_JA
} cui_date_locale_kind;
const cui_date_locale *cui_date_locale_builtin(cui_date_locale_kind);
size_t cui_date_format_locale(cui_date, const cui_date_locale *, char *, size_t capacity);
typedef enum { CUI_CALENDAR_DAYS, CUI_CALENDAR_YEARS, CUI_CALENDAR_MONTHS } cui_calendar_view;
typedef struct {
    bool range, disabled;
    unsigned first_weekday, months; /* Sunday=0; months 1..3 */
    cui_date minimum, maximum;      /* zero means no bound */
    cui_date_disabled_fn is_disabled;
    void *disabled_user;
    float cell_size;
    const cui_date_locale *locale; /* NULL preserves ISO formatting/legacy week start. */
    bool first_weekday_override;   /* explicit locale uses its week start unless true */
} cui_calendar_options;
typedef struct {
    cui_date month, cursor;
    bool selecting_end;
    cui_calendar_view view;
    unsigned year_page, year_cursor, month_cursor;
} cui_calendar_state;
typedef struct {
    bool changed, activated, navigated;
} cui_calendar_result;
void cui_calendar_init(cui_calendar_state *, cui_date initial);
int cui_date_compare(cui_date a, cui_date b);
/* 42 cells, including neighboring months and invalid zero at year boundaries. */
void cui_calendar_grid(cui_date month, unsigned first_weekday, cui_date output[42]);
bool cui_calendar_date_enabled(cui_date, cui_calendar_options);
cui_id cui_calendar_day_id(cui_id, cui_date);
/* navigation parts 1=previous year/page,2=previous month,3=next month,4=next year/page,
 * 5=year header. Year grid is 20 entries; month grid is 12. */
unsigned cui_calendar_first_weekday(cui_calendar_options);
bool cui_calendar_year_enabled(unsigned year, cui_calendar_options);
bool cui_calendar_month_enabled(unsigned year, unsigned month, cui_calendar_options);
cui_id cui_calendar_year_id(cui_id, unsigned year);
cui_id cui_calendar_month_id(cui_id, unsigned month);
cui_id cui_calendar_navigation_id(cui_id, unsigned part);
/* Separate reduction/submission lets popups close before adding a modal scope. */
cui_calendar_result cui_calendar_reduce(cui_context *, cui_id, cui_calendar_state *,
                                        cui_date_selection *, cui_calendar_options);
void cui_calendar_submit(cui_context *, cui_id, cui_calendar_state *, cui_date_selection,
                         cui_calendar_options);
cui_calendar_result cui_base_calendar(cui_context *, cui_id, cui_calendar_state *,
                                      cui_date_selection *, cui_calendar_options);
cui_calendar_result cui_calendar(cui_context *, cui_id, const cui_theme *, cui_calendar_state *,
                                 cui_date_selection *, cui_calendar_options);
typedef struct {
    cui_overlay overlay;
    cui_calendar_state calendar;
} cui_date_picker_state;
typedef struct {
    cui_calendar_options calendar;
    cui_rect viewport;
    cui_str placeholder;
    bool disabled;
    float width;
} cui_date_picker_options;
void cui_date_picker_init(cui_date_picker_state *, cui_id popup, cui_date initial);
/* Base submits trigger semantics and updates dismissal/toggle; layout is caller-owned. */
void cui_base_date_picker_trigger(cui_context *, cui_id, cui_str, cui_date_picker_state *,
                                  bool disabled);
cui_calendar_result cui_base_date_picker(cui_context *, cui_id trigger, cui_id calendar, cui_str,
                                         cui_date_picker_state *, cui_date_selection *,
                                         cui_date_picker_options);
bool cui_date_picker(cui_context *, cui_id, const cui_theme *, cui_str, cui_date_picker_state *,
                     cui_date_selection *, cui_date_picker_options);
typedef struct {
    unsigned hour, minute, second;
} cui_time;
typedef struct {
    unsigned segment;
    unsigned digit_count, draft;
} cui_time_state;
typedef struct {
    bool seconds, twelve_hour, disabled, readonly;
} cui_time_options;
bool cui_time_valid(cui_time);
/* Strict HH:MM[:SS] 24h model format; 12h appearance affects the widget only. */
bool cui_time_parse(cui_str, cui_time *);
size_t cui_time_format(cui_time, bool seconds, char *output, size_t capacity);
cui_id cui_time_segment_id(cui_id, unsigned segment);
bool cui_base_time_field(cui_context *, cui_id, cui_str, cui_time_state *, cui_time *,
                         cui_time_options);
bool cui_time_field(cui_context *, cui_id, const cui_theme *, cui_str, cui_time_state *, cui_time *,
                    cui_time_options);
typedef struct {
    float hue, saturation, value, alpha;
} cui_hsv;
typedef struct {
    cui_hsv hsv;
    cui_color last_color;
    bool initialized;
    unsigned dragging;
} cui_color_state;
typedef struct {
    bool alpha, disabled, readonly;
    float width;
    const cui_color *palette;
    size_t palette_count;
} cui_color_picker_options;
cui_hsv cui_color_to_hsv(cui_color);
cui_color cui_color_from_hsv(cui_hsv);
bool cui_color_parse_hex(cui_str, cui_color *);
size_t cui_color_format_hex(cui_color, bool alpha, char *output, size_t capacity);
/* Parts 1=hue,2=saturation/value plane,3=alpha;20..23=RGBA,100+i=palette. */
cui_id cui_color_part_id(cui_id, unsigned part);
bool cui_base_color_picker(cui_context *, cui_id, cui_color_state *, cui_color *,
                           cui_color_picker_options);
bool cui_color_picker(cui_context *, cui_id, const cui_theme *, cui_color_state *, cui_color *,
                      cui_color_picker_options);
#ifdef __cplusplus
}
#endif
#endif
