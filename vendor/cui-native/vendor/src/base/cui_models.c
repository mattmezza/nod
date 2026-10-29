/* SPDX-License-Identifier: MIT */
#include "cui/cui_base.h"
#include <math.h>
static float cui_models_clamp(float x, float a, float b) {
    return x < a ? a : x > b ? b : x;
}
static cui_rect cui_models_candidate(cui_rect a, cui_size s, cui_popup_side side, float g) {
    cui_rect r = {a.x, a.y, s.width, s.height};
    if (side == CUI_POPUP_BELOW)
        r.y = a.y + a.h + g;
    else if (side == CUI_POPUP_ABOVE)
        r.y = a.y - s.height - g;
    else if (side == CUI_POPUP_RIGHT)
        r.x = a.x + a.w + g;
    else
        r.x = a.x - s.width - g;
    return r;
}
static float cui_models_overflow(cui_rect r, cui_rect v) {
    return fmaxf(v.x - r.x, 0) + fmaxf(v.y - r.y, 0) + fmaxf(r.x + r.w - v.x - v.w, 0) +
           fmaxf(r.y + r.h - v.y - v.h, 0);
}
cui_popup_result cui_popup_place(cui_rect a, cui_size s, cui_rect v, cui_popup_side side,
                                 float gap) {
    if (side < CUI_POPUP_BELOW || side > CUI_POPUP_LEFT)
        side = CUI_POPUP_BELOW;
    if (!isfinite(gap) || gap < 0)
        gap = 0;
    v.w = fmaxf(v.w, 0);
    v.h = fmaxf(v.h, 0);
    s.width = fmaxf(s.width, 0);
    s.height = fmaxf(s.height, 0);
    cui_popup_side opposite = side == CUI_POPUP_BELOW   ? CUI_POPUP_ABOVE
                              : side == CUI_POPUP_ABOVE ? CUI_POPUP_BELOW
                              : side == CUI_POPUP_RIGHT ? CUI_POPUP_LEFT
                                                        : CUI_POPUP_RIGHT;
    cui_rect r = cui_models_candidate(a, s, side, gap),
             other = cui_models_candidate(a, s, opposite, gap);
    bool flip = cui_models_overflow(other, v) < cui_models_overflow(r, v);
    if (flip) {
        r = other;
        side = opposite;
    }
    r.w = fminf(r.w, v.w);
    r.h = fminf(r.h, v.h);
    r.x = cui_models_clamp(r.x, v.x, v.x + v.w - r.w);
    r.y = cui_models_clamp(r.y, v.y, v.y + v.h - r.h);
    return (cui_popup_result){r, side, flip};
}
static float cui_models_height(const float *h, size_t i) {
    return h && isfinite(h[i]) && h[i] > 0 ? h[i] : 0;
}
cui_virtual_range cui_virtualize(const float *h, size_t n, float scroll, float viewport,
                                 size_t overscan) {
    cui_virtual_range r = {0};
    if (!isfinite(viewport) || viewport < 0)
        viewport = 0;
    for (size_t i = 0; i < n; i++)
        r.total += cui_models_height(h, i);
    r.scroll = cui_models_clamp(isfinite(scroll) ? scroll : 0, 0, fmaxf(r.total - viewport, 0));
    float y = 0;
    size_t first = 0;
    while (first < n && y + cui_models_height(h, first) <= r.scroll) {
        y += cui_models_height(h, first);
        first++;
    }
    size_t last = first;
    float bottom = y;
    while (last < n && bottom < r.scroll + viewport) {
        bottom += cui_models_height(h, last);
        last++;
    }
    r.begin = first > overscan ? first - overscan : 0;
    r.end = overscan > n - last ? n : last + overscan;
    for (size_t i = 0; i < r.begin; i++)
        r.before += cui_models_height(h, i);
    float used = r.before;
    for (size_t i = r.begin; i < r.end; i++)
        used += cui_models_height(h, i);
    r.after = fmaxf(r.total - used, 0);
    return r;
}
int cui_date_days_in_month(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1 || year > 9999 || month < 1 || month > 12)
        return 0;
    return days[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
bool cui_date_valid(cui_date d) {
    int n = cui_date_days_in_month(d.year, d.month);
    return d.day >= 1 && d.day <= n;
}
int cui_date_weekday(cui_date d) {
    static const int offset[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (!cui_date_valid(d))
        return -1;
    int y = d.year - (d.month < 3);
    return (y + y / 4 - y / 100 + y / 400 + offset[d.month - 1] + d.day) % 7;
}
cui_date cui_date_add_months(cui_date d, int months) {
    if (!cui_date_valid(d))
        return (cui_date){0};
    long long index = (long long)(d.year - 1) * 12 + d.month - 1 + months;
    if (index < 0 || index >= 9999 * 12)
        return (cui_date){0};
    d.year = (int)(index / 12) + 1;
    d.month = (int)(index % 12) + 1;
    int n = cui_date_days_in_month(d.year, d.month);
    if (d.day > n)
        d.day = n;
    return d;
}
cui_date cui_date_add_days(cui_date d, int days) {
    if (!cui_date_valid(d))
        return (cui_date){0};
    long long remain = days;
    while (remain > 0) {
        int left = cui_date_days_in_month(d.year, d.month) - d.day;
        if (remain <= left) {
            d.day += (int)remain;
            return d;
        }
        remain -= left + 1;
        d.day = 1;
        d = cui_date_add_months(d, 1);
        if (!cui_date_valid(d))
            return d;
    }
    while (remain < 0) {
        if (-remain < d.day) {
            d.day += (int)remain;
            return d;
        }
        remain += d.day;
        d.day = 1;
        d = cui_date_add_months(d, -1);
        if (!cui_date_valid(d))
            return d;
        d.day = cui_date_days_in_month(d.year, d.month);
    }
    return d;
}
