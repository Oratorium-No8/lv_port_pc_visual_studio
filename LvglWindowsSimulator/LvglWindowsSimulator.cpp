#include <Windows.h>
#include <math.h>

#include <LvglWindowsIconResource.h>

#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"

// ============================================================
//  scr_main  骨格実装  (ITGC-100 / 800x480 / LVGL v9)
// ============================================================

// ---- 色定数 ----
#define COL_BG          lv_color_hex(0x0f172a)
#define COL_SURFACE     lv_color_hex(0x1e293b)
#define COL_CELL_IDLE   lv_color_hex(0x1f2937)
#define COL_CELL_OK     lv_color_hex(0x14532d)
#define COL_CELL_NG     lv_color_hex(0x7f1d1d)
#define COL_ACCENT_OK   lv_color_hex(0x22c55e)
#define COL_ACCENT_NG   lv_color_hex(0xef4444)
#define COL_ACCENT_IDLE lv_color_hex(0x374151)
#define COL_BLUE        lv_color_hex(0x38bdf8)
#define COL_WHITE       lv_color_hex(0xffffff)
#define COL_MUTED       lv_color_hex(0x6b7280)
#define COL_BTN_GRAY    lv_color_hex(0x334155)
#define COL_BTN_GREEN   lv_color_hex(0x16a34a)
#define COL_BTN_RED     lv_color_hex(0xdc2626)

// ---- チャンネル状態 ----
typedef enum { CH_INACTIVE, CH_OK, CH_NG } ch_state_t;

// ---- ウィジェットポインタ ----

// 画面
static lv_obj_t *scr_main;
static lv_obj_t *scr_settings_home;
static lv_obj_t *scr_meas_cond;
static lv_obj_t *scr_threshold;
static lv_obj_t *scr_history;
static lv_obj_t *scr_correction;
static lv_obj_t *scr_detail;
static lv_obj_t *scr_threshold_method = NULL;
static lv_obj_t *scr_threshold_manual  = NULL;

// scr_settings_home タイル
static lv_obj_t *settings_tile[5];

// header_bar (全画面共通コンポーネント)
static lv_obj_t *header_bar;
static lv_obj_t *btn_back;
static lv_obj_t *lbl_title;
static lv_obj_t *mem_selector;
static lv_obj_t *btn_mem_prev;
static lv_obj_t *lbl_mem_no;
static lv_obj_t *btn_mem_next;
static lv_obj_t *lbl_mem_name;
static lv_obj_t *header_right;

// header_right (scr_main 固有)
static lv_obj_t *lbl_judge_badge;
static lv_obj_t *btn_start;
static lv_obj_t *btn_stop;
static lv_obj_t *btn_gear;

// matrix_area
static lv_obj_t *matrix_area;
static lv_obj_t *ch_cell[16];
static lv_obj_t *accent_bar[16];
static lv_obj_t *lbl_ch_no[16];
static lv_obj_t *lbl_result[16];
static lv_obj_t *lbl_voltage_x[16];
static lv_obj_t *lbl_voltage_y[16];

// right_panel
static lv_obj_t *right_panel;
static lv_obj_t *lbl_trend_title;
static lv_obj_t *chart_trend;
static lv_obj_t *pie_area;
static lv_obj_t *count_ok;
static lv_obj_t *lbl_ok_title;
static lv_obj_t *lbl_ok_val;
static lv_obj_t *count_ng;
static lv_obj_t *lbl_ng_title;
static lv_obj_t *lbl_ng_val;
static lv_obj_t *lbl_ellipse_a;
static lv_obj_t *lbl_ellipse_b;
static lv_obj_t *btn_counter_reset;

// status_bar
static lv_obj_t *status_bar;
static lv_obj_t *lbl_datetime;
static lv_obj_t *lbl_latest_ng;

// pie_area バッファ (100x100 / ARGB8888 想定で 4byte/pixel)
static uint8_t pie_buf[100 * 100 * 4];

// threshold XY plot canvas (256x256)
static uint8_t    threshold_plot_buf[256 * 256 * 4];
static lv_obj_t  *threshold_plot_canvas = NULL;

// history XY plot canvas (400x290)
static uint8_t    history_plot_buf[400 * 290 * 4];
static lv_obj_t  *history_plot_canvas = NULL;

// threshold-manual preview canvas (244x244)
static uint8_t    thresh_manual_plot_buf[244 * 244 * 4];
static lv_obj_t  *thresh_manual_canvas = NULL;

// memory selector state
static uint32_t   g_mem_no = 42;
static lv_obj_t  *mem_popup_overlay = NULL;

// ---- 前方宣言 ----
static void btn_start_cb(lv_event_t *e);
static void btn_stop_cb(lv_event_t *e);
static void btn_counter_reset_cb(lv_event_t *e);
static void btn_gear_cb(lv_event_t *e);
static void btn_back_settings_cb(lv_event_t *e);
static void btn_back_to_settings_cb(lv_event_t *e);
static void tile_open_cb(lv_event_t *e);
static void draw_pie_chart(int32_t ok, int32_t ng);
static void init_trend_chart(void);
static void draw_threshold_plot(lv_timer_t *timer);
static void draw_history_plot(lv_timer_t *timer);
static void draw_thresh_manual_preview(lv_timer_t *timer);
static void mem_prev_cb(lv_event_t *e);
static void mem_next_cb(lv_event_t *e);
static void show_mem_popup_cb(lv_event_t *e);
static void hide_mem_popup_cb(lv_event_t *e);
static void create_mem_popup(void);

// ---- トレンドチャート定数・グローバル ----
#define TREND_CH_MAX  5

static const uint32_t TREND_COLOR_HEX[TREND_CH_MAX] = {
    0x22c55e,   // Ch1: 緑
    0x38bdf8,   // Ch2: 水色
    0xfbbf24,   // Ch3: 黄
    0xf472b6,   // Ch4: ピンク
    0xa78bfa,   // Ch5: 紫
};
static lv_chart_series_t *trend_series[TREND_CH_MAX];

// ---- ヘルパー: スクロール/ボーダー/パッドを除去 ----
static void bare_obj(lv_obj_t *obj)
{
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// ============================================================
//  Memory selector callbacks + popup
// ============================================================
static void mem_prev_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (g_mem_no > 1) {
        g_mem_no--;
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%03u", (unsigned)g_mem_no);
        lv_label_set_text(lbl_mem_no, buf);
    }
}

static void mem_next_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (g_mem_no < 255) {
        g_mem_no++;
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%03u", (unsigned)g_mem_no);
        lv_label_set_text(lbl_mem_no, buf);
    }
}

static void show_mem_popup_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (mem_popup_overlay) lv_obj_remove_flag(mem_popup_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void hide_mem_popup_cb(lv_event_t *e)
{
    lv_obj_t *target  = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *current = (lv_obj_t *)lv_event_get_current_target(e);
    if (target == current) {   /* clicked overlay directly, not a child */
        lv_obj_add_flag(mem_popup_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void mem_popup_close_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_obj_add_flag(mem_popup_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void create_mem_popup(void)
{
    /* Full-screen semi-transparent overlay on top layer */
    mem_popup_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(mem_popup_overlay, 800, 480);
    lv_obj_set_pos(mem_popup_overlay, 0, 0);
    lv_obj_set_style_bg_color(mem_popup_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(mem_popup_overlay, (lv_opa_t)153, 0);
    lv_obj_set_style_border_width(mem_popup_overlay, 0, 0);
    lv_obj_set_style_pad_all(mem_popup_overlay, 0, 0);
    lv_obj_remove_flag(mem_popup_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(mem_popup_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(mem_popup_overlay, hide_mem_popup_cb, LV_EVENT_CLICKED, NULL);

    /* Popup panel: 400×264, centered (200,108) */
    lv_obj_t *pp = lv_obj_create(mem_popup_overlay);
    lv_obj_set_pos(pp, 200, 108);
    lv_obj_set_size(pp, 400, 264);
    lv_obj_set_style_bg_color(pp, COL_BG, 0);
    lv_obj_set_style_bg_opa(pp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pp, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(pp, 2, 0);
    lv_obj_set_style_radius(pp, 8, 0);
    lv_obj_set_style_pad_all(pp, 0, 0);
    lv_obj_remove_flag(pp, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Header row: h=36 ───────────────────────────────────── */
    lv_obj_t *phdr = lv_obj_create(pp);
    lv_obj_set_pos(phdr, 0, 0);
    lv_obj_set_size(phdr, 400, 36);
    lv_obj_set_style_bg_color(phdr, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(phdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(phdr, 0, 0);
    lv_obj_set_style_border_width(phdr, 0, 0);
    lv_obj_set_style_pad_all(phdr, 0, 0);
    lv_obj_remove_flag(phdr, LV_OBJ_FLAG_SCROLLABLE);

    /* round top corners */
    lv_obj_set_style_radius(phdr, 8, 0);
    lv_obj_t *phdr_sq = lv_obj_create(phdr);
    lv_obj_set_pos(phdr_sq, 0, 18);
    lv_obj_set_size(phdr_sq, 400, 18);
    lv_obj_set_style_bg_color(phdr_sq, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(phdr_sq, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(phdr_sq, 0, 0);
    lv_obj_set_style_radius(phdr_sq, 0, 0);
    lv_obj_set_style_pad_all(phdr_sq, 0, 0);
    lv_obj_remove_flag(phdr_sq, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *p_title = lv_label_create(phdr);
    lv_label_set_text(p_title, "Select Memory  (1 ~ 255)");
    lv_obj_set_pos(p_title, 16, 11);
    lv_obj_set_style_text_color(p_title, COL_WHITE, 0);
    lv_obj_set_style_text_font(p_title, &lv_font_montserrat_14, 0);

    lv_obj_t *p_close = lv_btn_create(phdr);
    lv_obj_set_pos(p_close, 368, 6);
    lv_obj_set_size(p_close, 24, 24);
    lv_obj_set_style_bg_color(p_close, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(p_close, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p_close, 0, 0);
    lv_obj_set_style_pad_all(p_close, 0, 0);
    lv_obj_t *p_close_lbl = lv_label_create(p_close);
    lv_label_set_text(p_close_lbl, LV_SYMBOL_CLOSE);
    lv_obj_center(p_close_lbl);
    lv_obj_set_style_text_color(p_close_lbl, COL_MUTED, 0);
    lv_obj_add_event_cb(p_close, mem_popup_close_cb, LV_EVENT_CLICKED, NULL);

    /* ── Current MEM info bar ──────────────────────────────── */
    lv_obj_t *cur_bar = lv_label_create(pp);
    {
        char buf[64];
        lv_snprintf(buf, sizeof(buf), "Current: MEM %03u  —  M42: phi8 M8 tap", (unsigned)g_mem_no);
        lv_label_set_text(cur_bar, buf);
    }
    lv_obj_set_pos(cur_bar, 16, 42);
    lv_obj_set_style_text_color(cur_bar, COL_BLUE, 0);
    lv_obj_set_style_text_font(cur_bar, &lv_font_montserrat_10, 0);

    /* ── Search + filter row  pp(20,58,360,22) ──────────────── */
    lv_obj_t *search_box = lv_obj_create(pp);
    lv_obj_set_pos(search_box, 16, 58);
    lv_obj_set_size(search_box, 120, 22);
    lv_obj_set_style_bg_color(search_box, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(search_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(search_box, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(search_box, 1, 0);
    lv_obj_set_style_radius(search_box, 4, 0);
    lv_obj_set_style_pad_all(search_box, 0, 0);
    lv_obj_remove_flag(search_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *search_lbl = lv_label_create(search_box);
    lv_label_set_text(search_lbl, "No. direct input");
    lv_obj_set_pos(search_lbl, 6, 6);
    lv_obj_set_style_text_color(search_lbl, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(search_lbl, &lv_font_montserrat_10, 0);

    lv_obj_t *jump_btn = lv_btn_create(pp);
    lv_obj_set_pos(jump_btn, 142, 58);
    lv_obj_set_size(jump_btn, 48, 22);
    lv_obj_set_style_bg_color(jump_btn, lv_color_hex(0x1e3a5f), 0);
    lv_obj_set_style_border_color(jump_btn, lv_color_hex(0x1d4ed8), 0);
    lv_obj_set_style_border_width(jump_btn, 1, 0);
    lv_obj_set_style_radius(jump_btn, 4, 0);
    lv_obj_set_style_pad_all(jump_btn, 0, 0);
    lv_obj_t *jump_lbl = lv_label_create(jump_btn);
    lv_label_set_text(jump_lbl, "Jump");
    lv_obj_center(jump_lbl);
    lv_obj_set_style_text_color(jump_lbl, lv_color_hex(0x93c5fd), 0);
    lv_obj_set_style_text_font(jump_lbl, &lv_font_montserrat_10, 0);

    /* filter buttons */
    lv_obj_t *flt_all = lv_btn_create(pp);
    lv_obj_set_pos(flt_all, 198, 58);
    lv_obj_set_size(flt_all, 60, 22);
    lv_obj_set_style_bg_color(flt_all, lv_color_hex(0x1e3a5f), 0);
    lv_obj_set_style_border_color(flt_all, COL_BLUE, 0);
    lv_obj_set_style_border_width(flt_all, 1, 0);
    lv_obj_set_style_radius(flt_all, 4, 0);
    lv_obj_set_style_pad_all(flt_all, 0, 0);
    lv_obj_t *flt_all_lbl = lv_label_create(flt_all);
    lv_label_set_text(flt_all_lbl, "All");
    lv_obj_center(flt_all_lbl);
    lv_obj_set_style_text_color(flt_all_lbl, lv_color_hex(0x93c5fd), 0);
    lv_obj_set_style_text_font(flt_all_lbl, &lv_font_montserrat_10, 0);

    lv_obj_t *flt_used = lv_btn_create(pp);
    lv_obj_set_pos(flt_used, 264, 58);
    lv_obj_set_size(flt_used, 80, 22);
    lv_obj_set_style_bg_color(flt_used, lv_color_hex(0x111827), 0);
    lv_obj_set_style_border_color(flt_used, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(flt_used, 1, 0);
    lv_obj_set_style_radius(flt_used, 4, 0);
    lv_obj_set_style_pad_all(flt_used, 0, 0);
    lv_obj_t *flt_used_lbl = lv_label_create(flt_used);
    lv_label_set_text(flt_used_lbl, "Configured");
    lv_obj_center(flt_used_lbl);
    lv_obj_set_style_text_color(flt_used_lbl, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(flt_used_lbl, &lv_font_montserrat_10, 0);

    /* ── Memory grid: 8 cols × 4 rows  pp(16,88) ──────────── */
    /* col_w=44, gap=4 → total row width = 8×44 + 7×4 = 380  fits in 400-16-4=380 */
    /* row_h=26, gap=4 → 4 rows total height = 4×26 + 3×4 = 116 */

    typedef struct { int no; bool used; bool selected; const char *short_name; } MemCell;
    static const MemCell cells[32] = {
        {  1, true,  false, "M8 phi5" },
        {  2, true,  false, "M6 phi3" },
        {  3, false, false, NULL },
        {  4, true,  false, "M10 phi8" },
        {  5, false, false, NULL },
        {  6, false, false, NULL },
        {  7, false, false, NULL },
        {  8, false, false, NULL },
        {  9, false, false, NULL },
        { 10, false, false, NULL },
        { 11, false, false, NULL },
        { 12, false, false, NULL },
        { 13, false, false, NULL },
        { 14, false, false, NULL },
        { 15, false, false, NULL },
        { 16, false, false, NULL },
        { 17, false, false, NULL },
        { 18, false, false, NULL },
        { 19, false, false, NULL },
        { 20, false, false, NULL },
        { 21, false, false, NULL },
        { 22, false, false, NULL },
        { 23, false, false, NULL },
        { 24, false, false, NULL },
        { 25, false, false, NULL },
        { 26, false, false, NULL },
        { 27, false, false, NULL },
        { 28, false, false, NULL },
        { 29, false, false, NULL },
        { 30, false, false, NULL },
        { 31, false, false, NULL },
        { 32, false, false, NULL },
    };

    for (int i = 0; i < 32; i++) {
        const MemCell *mc = &cells[i];
        int32_t col = i % 8;
        int32_t row = i / 8;
        int32_t cx  = 16 + col * (44 + 4);
        int32_t cy  = 88 + row * (26 + 4);

        bool is_sel = ((uint32_t)mc->no == g_mem_no);

        lv_obj_t *cell = lv_obj_create(pp);
        lv_obj_set_pos(cell, cx, cy);
        lv_obj_set_size(cell, 44, 26);
        lv_obj_set_style_bg_color(cell,
            is_sel  ? lv_color_hex(0x1e3a5f) :
            mc->used ? lv_color_hex(0x14532d) :
                       COL_SURFACE, 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(cell,
            is_sel  ? COL_BLUE :
            mc->used ? lv_color_hex(0x22c55e) :
                       lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(cell, is_sel ? 2 : 1, 0);
        lv_obj_set_style_radius(cell, 4, 0);
        lv_obj_set_style_pad_all(cell, 0, 0);
        lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

        /* number label */
        lv_obj_t *no_lbl = lv_label_create(cell);
        char no_buf[8];
        lv_snprintf(no_buf, sizeof(no_buf), "%03d", mc->no);
        lv_label_set_text(no_lbl, no_buf);
        lv_obj_set_pos(no_lbl, 0, 3);
        lv_obj_set_width(no_lbl, 44);
        lv_obj_set_style_text_align(no_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(no_lbl,
            is_sel  ? lv_color_hex(0x93c5fd) :
            mc->used ? lv_color_hex(0x86efac) :
                       lv_color_hex(0x374151), 0);
        lv_obj_set_style_text_font(no_lbl, &lv_font_montserrat_10, 0);

        /* short name or "selected" indicator */
        if (is_sel) {
            lv_obj_t *sel_lbl = lv_label_create(cell);
            lv_label_set_text(sel_lbl, "sel.");
            lv_obj_set_pos(sel_lbl, 0, 14);
            lv_obj_set_width(sel_lbl, 44);
            lv_obj_set_style_text_align(sel_lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(sel_lbl, COL_BLUE, 0);
            lv_obj_set_style_text_font(sel_lbl, &lv_font_montserrat_10, 0);
        } else if (mc->used && mc->short_name) {
            lv_obj_t *name_lbl = lv_label_create(cell);
            lv_label_set_text(name_lbl, mc->short_name);
            lv_obj_set_pos(name_lbl, 0, 14);
            lv_obj_set_width(name_lbl, 44);
            lv_obj_set_style_text_align(name_lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(name_lbl, lv_color_hex(0x4ade80), 0);
            lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_10, 0);
        }
    }

    /* ── Scroll hint ────────────────────────────────────────── */
    lv_obj_t *scroll_hint = lv_obj_create(pp);
    lv_obj_set_pos(scroll_hint, 16, 208);
    lv_obj_set_size(scroll_hint, 368, 16);
    lv_obj_set_style_bg_color(scroll_hint, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(scroll_hint, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(scroll_hint, 4, 0);
    lv_obj_set_style_border_width(scroll_hint, 0, 0);
    lv_obj_set_style_pad_all(scroll_hint, 0, 0);
    lv_obj_remove_flag(scroll_hint, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sh_lbl = lv_label_create(scroll_hint);
    lv_label_set_text(sh_lbl, "v  033 ~ 255  (scroll)");
    lv_obj_set_pos(sh_lbl, 0, 2);
    lv_obj_set_width(sh_lbl, 368);
    lv_obj_set_style_text_align(sh_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(sh_lbl, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_text_font(sh_lbl, &lv_font_montserrat_10, 0);

    /* ── Legend ─────────────────────────────────────────────── */
    /* Used swatch */
    lv_obj_t *sw_used = lv_obj_create(pp);
    lv_obj_set_pos(sw_used, 16, 228);
    lv_obj_set_size(sw_used, 16, 10);
    lv_obj_set_style_bg_color(sw_used, lv_color_hex(0x14532d), 0);
    lv_obj_set_style_bg_opa(sw_used, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(sw_used, lv_color_hex(0x22c55e), 0);
    lv_obj_set_style_border_width(sw_used, 1, 0);
    lv_obj_set_style_radius(sw_used, 2, 0);
    lv_obj_set_style_pad_all(sw_used, 0, 0);
    lv_obj_remove_flag(sw_used, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sw_used_lbl = lv_label_create(pp);
    lv_label_set_text(sw_used_lbl, "Configured");
    lv_obj_set_pos(sw_used_lbl, 36, 226);
    lv_obj_set_style_text_color(sw_used_lbl, lv_color_hex(0x4ade80), 0);
    lv_obj_set_style_text_font(sw_used_lbl, &lv_font_montserrat_10, 0);

    /* Selected swatch */
    lv_obj_t *sw_sel = lv_obj_create(pp);
    lv_obj_set_pos(sw_sel, 110, 228);
    lv_obj_set_size(sw_sel, 16, 10);
    lv_obj_set_style_bg_color(sw_sel, lv_color_hex(0x1e3a5f), 0);
    lv_obj_set_style_bg_opa(sw_sel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(sw_sel, COL_BLUE, 0);
    lv_obj_set_style_border_width(sw_sel, 2, 0);
    lv_obj_set_style_radius(sw_sel, 2, 0);
    lv_obj_set_style_pad_all(sw_sel, 0, 0);
    lv_obj_remove_flag(sw_sel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sw_sel_lbl = lv_label_create(pp);
    lv_label_set_text(sw_sel_lbl, "Selected");
    lv_obj_set_pos(sw_sel_lbl, 130, 226);
    lv_obj_set_style_text_color(sw_sel_lbl, lv_color_hex(0x93c5fd), 0);
    lv_obj_set_style_text_font(sw_sel_lbl, &lv_font_montserrat_10, 0);

    /* Empty swatch */
    lv_obj_t *sw_emp = lv_obj_create(pp);
    lv_obj_set_pos(sw_emp, 200, 228);
    lv_obj_set_size(sw_emp, 16, 10);
    lv_obj_set_style_bg_color(sw_emp, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(sw_emp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(sw_emp, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(sw_emp, 1, 0);
    lv_obj_set_style_radius(sw_emp, 2, 0);
    lv_obj_set_style_pad_all(sw_emp, 0, 0);
    lv_obj_remove_flag(sw_emp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *sw_emp_lbl = lv_label_create(pp);
    lv_label_set_text(sw_emp_lbl, "Empty");
    lv_obj_set_pos(sw_emp_lbl, 220, 226);
    lv_obj_set_style_text_color(sw_emp_lbl, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(sw_emp_lbl, &lv_font_montserrat_10, 0);

    /* ── Bottom buttons ─────────────────────────────────────── */
    lv_obj_t *btn_cancel = lv_btn_create(pp);
    lv_obj_set_pos(btn_cancel, 16, 234);
    lv_obj_set_size(btn_cancel, 100, 24);
    lv_obj_set_style_bg_color(btn_cancel, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn_cancel, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(btn_cancel, 1, 0);
    lv_obj_set_style_radius(btn_cancel, 4, 0);
    lv_obj_set_style_pad_all(btn_cancel, 0, 0);
    lv_obj_t *cancel_lbl = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_lbl, "Close");
    lv_obj_center(cancel_lbl);
    lv_obj_set_style_text_color(cancel_lbl, COL_MUTED, 0);
    lv_obj_set_style_text_font(cancel_lbl, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(btn_cancel, mem_popup_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_ok = lv_btn_create(pp);
    lv_obj_set_pos(btn_ok, 284, 234);
    lv_obj_set_size(btn_ok, 100, 24);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x0f6e56), 0);
    lv_obj_set_style_bg_opa(btn_ok, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_set_style_radius(btn_ok, 4, 0);
    lv_obj_set_style_pad_all(btn_ok, 0, 0);
    lv_obj_t *ok_lbl = lv_label_create(btn_ok);
    lv_label_set_text(ok_lbl, "Confirm");
    lv_obj_center(ok_lbl);
    lv_obj_set_style_text_color(ok_lbl, COL_WHITE, 0);
    lv_obj_set_style_text_font(ok_lbl, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(btn_ok, mem_popup_close_cb, LV_EVENT_CLICKED, NULL);
}

// ---- header_bar 生成 ----
// metr. w=800 h=44  bg=COL_BG
// scr_main では btn_back を HIDDEN にして呼び出す
static void create_header_bar(lv_obj_t *parent)
{
    header_bar = lv_obj_create(parent);
    lv_obj_set_size(header_bar, 800, 44);
    lv_obj_set_pos(header_bar, 0, 0);
    lv_obj_set_style_bg_color(header_bar, COL_BG, 0);
    lv_obj_set_style_bg_opa(header_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(header_bar, 0, 0);
    bare_obj(header_bar);

    // btn_back: 36x28  (metr. 画面では非表示)
    btn_back = lv_btn_create(header_bar);
    lv_obj_set_size(btn_back, 36, 28);
    lv_obj_set_pos(btn_back, 4, 8);
    lv_obj_set_style_bg_color(btn_back, COL_BTN_GRAY, 0);
    lv_obj_t *lbl_back_icon = lv_label_create(btn_back);
    lv_label_set_text(lbl_back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back_icon);
    // scr_main では戻るボタン不要
    lv_obj_add_flag(btn_back, LV_OBJ_FLAG_HIDDEN);

    // lbl_title: 画面タイトル  font=bold-16
    lbl_title = lv_label_create(header_bar);
    lv_label_set_text(lbl_title, "ITGC-100");
    lv_obj_set_pos(lbl_title, 12, 14);
    lv_obj_set_style_text_color(lbl_title, COL_WHITE, 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);

    // mem_selector pill: 168×30
    mem_selector = lv_obj_create(header_bar);
    lv_obj_set_size(mem_selector, 168, 30);
    lv_obj_set_pos(mem_selector, 210, 7);
    lv_obj_set_style_bg_color(mem_selector, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(mem_selector, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(mem_selector, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(mem_selector, 1, 0);
    lv_obj_set_style_radius(mem_selector, 6, 0);
    lv_obj_set_style_pad_all(mem_selector, 0, 0);
    lv_obj_remove_flag(mem_selector, LV_OBJ_FLAG_SCROLLABLE);

    // prev button
    btn_mem_prev = lv_btn_create(mem_selector);
    lv_obj_set_size(btn_mem_prev, 24, 22);
    lv_obj_set_pos(btn_mem_prev, 4, 4);
    lv_obj_set_style_bg_color(btn_mem_prev, lv_color_hex(0x111827), 0);
    lv_obj_set_style_radius(btn_mem_prev, 4, 0);
    lv_obj_set_style_pad_all(btn_mem_prev, 0, 0);
    lv_obj_t *lbl_prev = lv_label_create(btn_mem_prev);
    lv_label_set_text(lbl_prev, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_prev);
    lv_obj_set_style_text_color(lbl_prev, lv_color_hex(0x64748b), 0);
    lv_obj_add_event_cb(btn_mem_prev, mem_prev_cb, LV_EVENT_CLICKED, NULL);

    // "MEM" caption
    lv_obj_t *lbl_mem_cap = lv_label_create(mem_selector);
    lv_label_set_text(lbl_mem_cap, "MEM");
    lv_obj_set_pos(lbl_mem_cap, 28, 2);
    lv_obj_set_width(lbl_mem_cap, 112);
    lv_obj_set_style_text_color(lbl_mem_cap, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(lbl_mem_cap, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(lbl_mem_cap, LV_TEXT_ALIGN_CENTER, 0);

    // mem number (clickable → open popup)
    lbl_mem_no = lv_label_create(mem_selector);
    {
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%03u", (unsigned)g_mem_no);
        lv_label_set_text(lbl_mem_no, buf);
    }
    lv_obj_set_pos(lbl_mem_no, 28, 14);
    lv_obj_set_width(lbl_mem_no, 112);
    lv_obj_set_style_text_color(lbl_mem_no, COL_BLUE, 0);
    lv_obj_set_style_text_font(lbl_mem_no, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(lbl_mem_no, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(lbl_mem_no, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_mem_no, show_mem_popup_cb, LV_EVENT_CLICKED, NULL);

    // next button
    btn_mem_next = lv_btn_create(mem_selector);
    lv_obj_set_size(btn_mem_next, 24, 22);
    lv_obj_set_pos(btn_mem_next, 140, 4);
    lv_obj_set_style_bg_color(btn_mem_next, lv_color_hex(0x111827), 0);
    lv_obj_set_style_radius(btn_mem_next, 4, 0);
    lv_obj_set_style_pad_all(btn_mem_next, 0, 0);
    lv_obj_t *lbl_next = lv_label_create(btn_mem_next);
    lv_label_set_text(lbl_next, LV_SYMBOL_RIGHT);
    lv_obj_center(lbl_next);
    lv_obj_set_style_text_color(lbl_next, lv_color_hex(0x64748b), 0);
    lv_obj_add_event_cb(btn_mem_next, mem_next_cb, LV_EVENT_CLICKED, NULL);

    // mem name + sub (sibling in header_bar, to the right of the pill)
    lbl_mem_name = lv_label_create(header_bar);
    lv_label_set_text(lbl_mem_name, "M42: phi8 M8 tap");
    lv_obj_set_pos(lbl_mem_name, 386, 9);
    lv_obj_set_style_text_color(lbl_mem_name, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(lbl_mem_name, &lv_font_montserrat_10, 0);

    lv_obj_t *lbl_mem_sub = lv_label_create(header_bar);
    lv_label_set_text(lbl_mem_sub, "Ch1,2,5,8 / 10kHz / x32");
    lv_obj_set_pos(lbl_mem_sub, 386, 22);
    lv_obj_set_style_text_color(lbl_mem_sub, lv_color_hex(0x334155), 0);
    lv_obj_set_style_text_font(lbl_mem_sub, &lv_font_montserrat_10, 0);

    // header_right: scr_main 固有ボタン群
    // 総合判定バッジ / START / STOP / 歯車
    header_right = lv_obj_create(header_bar);
    lv_obj_set_size(header_right, 224, 44);
    lv_obj_set_pos(header_right, 576, 0);
    lv_obj_set_style_bg_opa(header_right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header_right, 0, 0);
    lv_obj_set_style_pad_all(header_right, 0, 0);
    lv_obj_remove_flag(header_right, LV_OBJ_FLAG_SCROLLABLE);

    // 総合判定バッジ: "--" / "ALL OK" / "NG"
    lbl_judge_badge = lv_label_create(header_right);
    lv_label_set_text(lbl_judge_badge, "--");
    lv_obj_set_pos(lbl_judge_badge, 2, 14);
    lv_obj_set_style_text_color(lbl_judge_badge, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_judge_badge, &lv_font_montserrat_16, 0);

    // STARTボタン: 56x30  緑
    btn_start = lv_btn_create(header_right);
    lv_obj_set_size(btn_start, 56, 30);
    lv_obj_set_pos(btn_start, 46, 7);
    lv_obj_set_style_bg_color(btn_start, COL_BTN_GREEN, 0);
    lv_obj_t *lbl_start = lv_label_create(btn_start);
    lv_label_set_text(lbl_start, "START");
    lv_obj_center(lbl_start);
    lv_obj_set_style_text_font(lbl_start, &lv_font_montserrat_12, 0);

    // STOPボタン: 56x30  赤
    btn_stop = lv_btn_create(header_right);
    lv_obj_set_size(btn_stop, 56, 30);
    lv_obj_set_pos(btn_stop, 108, 7);
    lv_obj_set_style_bg_color(btn_stop, COL_BTN_RED, 0);
    lv_obj_t *lbl_stop = lv_label_create(btn_stop);
    lv_label_set_text(lbl_stop, "STOP");
    lv_obj_center(lbl_stop);
    lv_obj_set_style_text_font(lbl_stop, &lv_font_montserrat_12, 0);

    // 歯車ボタン: 36x30  グレー → scr_settings_home へ遷移
    btn_gear = lv_btn_create(header_right);
    lv_obj_set_size(btn_gear, 36, 30);
    lv_obj_set_pos(btn_gear, 170, 7);
    lv_obj_set_style_bg_color(btn_gear, COL_BTN_GRAY, 0);
    lv_obj_t *lbl_gear = lv_label_create(btn_gear);
    lv_label_set_text(lbl_gear, LV_SYMBOL_SETTINGS);
    lv_obj_center(lbl_gear);

    // イベントコールバック登録
    lv_obj_add_event_cb(btn_start, btn_start_cb,  LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_stop,  btn_stop_cb,   LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_gear,  btn_gear_cb,   LV_EVENT_CLICKED, NULL);
}

// ---- ch_cell 単体生成 ----
// idx: 0〜15  4列×4行 (左→右, 上→下)
// グリッド座標: col=idx%4, row=idx/4
// セル間: 7px gap  上マージン: 4px  左マージン: 7px
static void create_ch_cell(lv_obj_t *parent, int idx)
{
    int col = idx % 4;
    int row = idx / 4;
    // 562 = 4*132 + 3*7 + 7*2  → x = col*(132+7) + 7
    int x   = col * 139 + 7;
    // 418 = 4*98  + 3*6  + 4*2 → y = row*(98+6) + 4
    int y   = row * 104 + 4;

    // セル本体
    ch_cell[idx] = lv_obj_create(parent);
    lv_obj_set_size(ch_cell[idx], 132, 98);
    lv_obj_set_pos(ch_cell[idx], x, y);
    lv_obj_set_style_bg_color(ch_cell[idx], COL_CELL_IDLE, 0);
    lv_obj_set_style_bg_opa(ch_cell[idx], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ch_cell[idx], 4, 0);
    lv_obj_set_style_border_width(ch_cell[idx], 0, 0);
    lv_obj_set_style_pad_all(ch_cell[idx], 0, 0);
    lv_obj_remove_flag(ch_cell[idx], LV_OBJ_FLAG_SCROLLABLE);

    // accent_bar: 132x4  上端  OK=緑 / NG=赤 / 未選択=グレー
    accent_bar[idx] = lv_obj_create(ch_cell[idx]);
    lv_obj_set_size(accent_bar[idx], 132, 4);
    lv_obj_set_pos(accent_bar[idx], 0, 0);
    lv_obj_set_style_bg_color(accent_bar[idx], COL_ACCENT_IDLE, 0);
    lv_obj_set_style_bg_opa(accent_bar[idx], LV_OPA_COVER, 0);
    lv_obj_set_style_radius(accent_bar[idx], 0, 0);
    lv_obj_set_style_border_width(accent_bar[idx], 0, 0);
    lv_obj_set_style_pad_all(accent_bar[idx], 0, 0);
    lv_obj_remove_flag(accent_bar[idx], LV_OBJ_FLAG_SCROLLABLE);

    // lbl_ch_no: font=12  例: "Ch 1"
    char ch_text[8];
    lv_snprintf(ch_text, sizeof(ch_text), "Ch %d", idx + 1);
    lbl_ch_no[idx] = lv_label_create(ch_cell[idx]);
    lv_label_set_text(lbl_ch_no[idx], ch_text);
    lv_obj_set_pos(lbl_ch_no[idx], 8, 9);
    lv_obj_set_style_text_color(lbl_ch_no[idx], COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_ch_no[idx], &lv_font_montserrat_12, 0);

    // lbl_result: font=bold-20  "OK" / "NG" / "--"
    lbl_result[idx] = lv_label_create(ch_cell[idx]);
    lv_label_set_text(lbl_result[idx], "--");
    lv_obj_set_pos(lbl_result[idx], 8, 28);
    lv_obj_set_style_text_color(lbl_result[idx], COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_result[idx], &lv_font_montserrat_20, 0);

    // lbl_voltage_x: font=10 mono  例: "X  +0.000 V"
    lbl_voltage_x[idx] = lv_label_create(ch_cell[idx]);
    lv_label_set_text(lbl_voltage_x[idx], "X  +0.000 V");
    lv_obj_set_pos(lbl_voltage_x[idx], 8, 60);
    lv_obj_set_style_text_color(lbl_voltage_x[idx], COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_voltage_x[idx], &lv_font_montserrat_10, 0);

    // lbl_voltage_y: font=10 mono  例: "Y  +0.000 V"
    lbl_voltage_y[idx] = lv_label_create(ch_cell[idx]);
    lv_label_set_text(lbl_voltage_y[idx], "Y  +0.000 V");
    lv_obj_set_pos(lbl_voltage_y[idx], 8, 76);
    lv_obj_set_style_text_color(lbl_voltage_y[idx], COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_voltage_y[idx], &lv_font_montserrat_10, 0);
}

// ---- ch_cell 状態更新 (task_ui から呼ぶ) ----
void update_ch_cell(int ch, ch_state_t state, float vx, float vy)
{
    if (ch < 0 || ch >= 16) return;

    char buf[16];
    switch (state) {
    case CH_INACTIVE:
        lv_obj_set_style_bg_color(ch_cell[ch], COL_CELL_IDLE, 0);
        lv_obj_set_style_bg_color(accent_bar[ch], COL_ACCENT_IDLE, 0);
        lv_obj_set_style_text_color(lbl_result[ch], COL_MUTED, 0);
        lv_label_set_text(lbl_result[ch], "--");
        break;
    case CH_OK:
        lv_obj_set_style_bg_color(ch_cell[ch], COL_CELL_OK, 0);
        lv_obj_set_style_bg_color(accent_bar[ch], COL_ACCENT_OK, 0);
        lv_obj_set_style_text_color(lbl_result[ch], COL_ACCENT_OK, 0);
        lv_label_set_text(lbl_result[ch], "OK");
        break;
    case CH_NG:
        lv_obj_set_style_bg_color(ch_cell[ch], COL_CELL_NG, 0);
        lv_obj_set_style_bg_color(accent_bar[ch], COL_ACCENT_NG, 0);
        lv_obj_set_style_text_color(lbl_result[ch], COL_ACCENT_NG, 0);
        lv_label_set_text(lbl_result[ch], "NG");
        break;
    }

    lv_snprintf(buf, sizeof(buf), "X %+.3f V", vx);
    lv_label_set_text(lbl_voltage_x[ch], buf);
    lv_snprintf(buf, sizeof(buf), "Y %+.3f V", vy);
    lv_label_set_text(lbl_voltage_y[ch], buf);
}

// ============================================================
//  ダミーデータタイマー  (動作確認用)
// ============================================================

static lv_timer_t *timer_dummy = NULL;
static lv_timer_t *timer_clock = NULL;
static uint32_t    dummy_tick  = 0;
static int32_t     ok_count    = 0;
static int32_t     ng_count    = 0;

// 1秒ごとに status_bar の時刻を更新
static void clock_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[24];
    lv_snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                st.wYear, st.wMonth, st.wDay,
                st.wHour, st.wMinute, st.wSecond);
    lv_label_set_text(lbl_datetime, buf);
}

// 800ms ごとに呼ばれる。楕円軌道上のダミー電圧を全16Chに書き込む。
static void dummy_timer_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    dummy_tick++;
    ok_count = 0;
    ng_count = 0;

    // 楕円パラメータ (固定値)
    const float a = 1.0f;   // X半径 [V]
    const float b = 0.55f;  // Y半径 [V]
    const float k = 1.2f;   // 閾値拡大係数

    float dist_vals[16] = {0};  // トレンドチャート用に保持

    for (int ch = 0; ch < 16; ch++) {
        // Chごとに位相をずらして楕円軌道上を移動
        float phase = (float)dummy_tick * 0.18f
                    + (float)ch * (2.0f * 3.14159265f / 16.0f);
        float vx = a * cosf(phase);
        float vy = b * sinf(phase);

        // 擬似ノイズ (tick と ch の組み合わせで決定論的に生成)
        vx += 0.07f * sinf((float)(dummy_tick * 7  + ch * 13));
        vy += 0.07f * cosf((float)(dummy_tick * 11 + ch *  5));

        // 楕円距離 d: d > 1.0 なら閾値楕円の外 → NG
        float dx = vx / (a * k);
        float dy = vy / (b * k);
        float dist = sqrtf(dx * dx + dy * dy);
        dist_vals[ch] = dist;

        ch_state_t state = (dist > 1.0f) ? CH_NG : CH_OK;
        update_ch_cell(ch, state, vx, vy);

        if (state == CH_OK) ok_count++;
        else                ng_count++;
    }

    // トレンドチャート更新 (Ch1〜Ch5 / 値は dist×100 の整数)
    for (int i = 0; i < TREND_CH_MAX; i++) {
        lv_chart_set_next_value(chart_trend, trend_series[i],
                                (int32_t)(dist_vals[i] * 100.0f));
    }

    // OK/NG カウント更新
    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d", (int)ok_count);
    lv_label_set_text(lbl_ok_val, buf);
    lv_snprintf(buf, sizeof(buf), "%d", (int)ng_count);
    lv_label_set_text(lbl_ng_val, buf);

    // 楕円パラメータ表示 (Ch1 の固定値)
    lv_label_set_text(lbl_ellipse_a, "a = 1.000 V");
    lv_label_set_text(lbl_ellipse_b, "b = 0.550 V");

    // 総合判定バッジ
    if (ng_count > 0) {
        lv_label_set_text(lbl_judge_badge, "NG");
        lv_obj_set_style_text_color(lbl_judge_badge, COL_ACCENT_NG, 0);
    } else {
        lv_label_set_text(lbl_judge_badge, "ALL OK");
        lv_obj_set_style_text_color(lbl_judge_badge, COL_ACCENT_OK, 0);
    }

    // 円グラフ更新
    draw_pie_chart(ok_count, ng_count);
}

// START ボタン: タイマー開始
static void btn_start_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (timer_dummy != NULL) return;  // 二重起動防止
    ok_count   = 0;
    ng_count   = 0;
    dummy_tick = 0;
    timer_dummy = lv_timer_create(dummy_timer_cb, 800, NULL);
}

// STOP ボタン: タイマー停止・全Ch を INACTIVE に戻す
static void btn_stop_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (timer_dummy != NULL) {
        lv_timer_delete(timer_dummy);
        timer_dummy = NULL;
    }
    for (int ch = 0; ch < 16; ch++) {
        update_ch_cell(ch, CH_INACTIVE, 0.0f, 0.0f);
    }
    lv_label_set_text(lbl_ok_val, "0");
    lv_label_set_text(lbl_ng_val, "0");
    lv_label_set_text(lbl_judge_badge, "--");
    lv_obj_set_style_text_color(lbl_judge_badge, COL_MUTED, 0);
    for (int i = 0; i < TREND_CH_MAX; i++) {
        lv_chart_set_all_value(chart_trend, trend_series[i], LV_CHART_POINT_NONE);
    }
    lv_chart_refresh(chart_trend);
    draw_pie_chart(0, 0);
}

// カウンターリセットボタン: カウントのみクリア (タイマーは継続)
static void btn_counter_reset_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ok_count = 0;
    ng_count = 0;
    lv_label_set_text(lbl_ok_val, "0");
    lv_label_set_text(lbl_ng_val, "0");
    for (int i = 0; i < TREND_CH_MAX; i++) {
        lv_chart_set_all_value(chart_trend, trend_series[i], LV_CHART_POINT_NONE);
    }
    lv_chart_refresh(chart_trend);
    draw_pie_chart(0, 0);
}

// ============================================================
//  pie_area  ドーナツ円グラフ描画
// ============================================================
// ok / ng: 累積カウント。タイマーコールバックおよびリセット時に呼ぶ。
// ※ lv_canvas_fill_bg() は初期化時に呼ばず、ここで毎回背景を上書きする。
static void draw_pie_chart(int32_t ok, int32_t ng)
{
    lv_layer_t layer;
    lv_canvas_init_layer(pie_area, &layer);

    // ─── 1. 背景クリア ───────────────────────────────────────
    lv_draw_rect_dsc_t bg;
    lv_draw_rect_dsc_init(&bg);
    bg.bg_color   = COL_SURFACE;
    bg.bg_opa     = LV_OPA_COVER;
    bg.radius     = 0;
    bg.border_width = 0;
    lv_area_t full = {0, 0, 99, 99};
    lv_draw_rect(&layer, &bg, &full);

    // ─── 2. ドーナツアーク ────────────────────────────────────
    const int32_t  cx      = 50;
    const int32_t  cy      = 50;
    const uint16_t r_outer = 44;
    const int32_t  ring_w  = 16;
    const int32_t  inner_r = r_outer - ring_w;   // = 28

    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.center.x = cx;
    arc.center.y = cy;
    arc.radius   = r_outer;
    arc.width    = ring_w;
    arc.rounded  = 0;
    arc.opa      = LV_OPA_COVER;

    int32_t total = ok + ng;

    if (total == 0) {
        // 未計測: グレー全円
        arc.color       = COL_ACCENT_IDLE;
        arc.start_angle = 0;
        arc.end_angle   = 360;
        lv_draw_arc(&layer, &arc);
    } else if (ng == 0) {
        // 全OK: 緑全円
        arc.color       = COL_ACCENT_OK;
        arc.start_angle = 0;
        arc.end_angle   = 360;
        lv_draw_arc(&layer, &arc);
    } else if (ok == 0) {
        // 全NG: 赤全円
        arc.color       = COL_ACCENT_NG;
        arc.start_angle = 0;
        arc.end_angle   = 360;
        lv_draw_arc(&layer, &arc);
    } else {
        // NG: 全円を赤で塗る → OK分を緑で上書き
        arc.color       = COL_ACCENT_NG;
        arc.start_angle = 0;
        arc.end_angle   = 360;
        lv_draw_arc(&layer, &arc);

        // OK: 12時(270°)から時計回りに ok_angle 分
        int32_t ok_angle = (int32_t)((float)ok / (float)total * 360.0f + 0.5f);
        arc.color       = COL_ACCENT_OK;
        arc.start_angle = 270;
        arc.end_angle   = (lv_value_precise_t)((270 + ok_angle) % 360);
        lv_draw_arc(&layer, &arc);
    }

    // ─── 3. 中心円でドーナツ化 ───────────────────────────────
    lv_draw_rect_dsc_t hole;
    lv_draw_rect_dsc_init(&hole);
    hole.bg_color    = COL_SURFACE;
    hole.bg_opa      = LV_OPA_COVER;
    hole.radius      = LV_RADIUS_CIRCLE;
    hole.border_width = 0;
    lv_area_t hole_area = {
        (lv_coord_t)(cx - inner_r), (lv_coord_t)(cy - inner_r),
        (lv_coord_t)(cx + inner_r), (lv_coord_t)(cy + inner_r)
    };
    lv_draw_rect(&layer, &hole, &hole_area);

    // ─── 4. 中央テキスト (OK%) ───────────────────────────────
    lv_draw_label_dsc_t lbl;
    lv_draw_label_dsc_init(&lbl);
    lbl.font   = &lv_font_montserrat_12;
    lbl.color  = COL_WHITE;
    lbl.opa    = LV_OPA_COVER;
    lbl.align  = LV_TEXT_ALIGN_CENTER;

    char pct[8];
    if (total > 0) {
        lv_snprintf(pct, sizeof(pct), "%d%%", (int)(ok * 100 / total));
    } else {
        lv_snprintf(pct, sizeof(pct), "--");
    }
    lbl.text = pct;
    lv_area_t txt_area = {
        (lv_coord_t)(cx - 22), (lv_coord_t)(cy - 8),
        (lv_coord_t)(cx + 22), (lv_coord_t)(cy + 8)
    };
    lv_draw_label(&layer, &lbl, &txt_area);

    lv_canvas_finish_layer(pie_area, &layer);
}

// ============================================================
//  chart_trend  初期化
// ============================================================
static void init_trend_chart(void)
{
    lv_chart_set_range(chart_trend, LV_CHART_AXIS_PRIMARY_Y, 0, 200);
    lv_chart_set_div_line_count(chart_trend, 4, 0);
    // ドットを非表示
    lv_obj_set_style_size(chart_trend, 0, 0, LV_PART_INDICATOR);

    for (int i = 0; i < TREND_CH_MAX; i++) {
        trend_series[i] = lv_chart_add_series(chart_trend,
                                               lv_color_hex(TREND_COLOR_HEX[i]),
                                               LV_CHART_AXIS_PRIMARY_Y);
        lv_chart_set_all_value(chart_trend, trend_series[i], LV_CHART_POINT_NONE);
    }
}

// ============================================================
//  scr_settings_home  画面遷移コールバック
// ============================================================

// 歯車ボタン → 設定ホームへ (左スライド)
static void btn_gear_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (timer_dummy != NULL) {
        lv_timer_delete(timer_dummy);
        timer_dummy = NULL;
    }
    lv_screen_load_anim(scr_settings_home, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}

// 設定ホームの戻るボタン → メイン画面へ (右スライド)
static void btn_back_settings_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_screen_load_anim(scr_main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, false);
}

// ============================================================
//  設定サブ画面  共通ヘルパー・コールバック
// ============================================================

// 設定サブ画面 → scr_settings_home へ戻る
static void btn_back_to_settings_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_screen_load_anim(scr_settings_home, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, false);
}

// タイル tap → user_data に渡した画面へ左スライド遷移
static void tile_open_cb(lv_event_t *e)
{
    lv_obj_t *dest = (lv_obj_t *)lv_event_get_user_data(e);
    lv_screen_load_anim(dest, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
}

// 設定サブ画面の共通骨格を生成。戻り値 = コンテンツコンテナ (800×436, y=44)
static lv_obj_t *make_settings_subscr(lv_obj_t **out_scr, const char *title)
{
    *out_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(*out_scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(*out_scr, LV_OPA_COVER, 0);
    bare_obj(*out_scr);

    // ヘッダー
    lv_obj_t *hdr = lv_obj_create(*out_scr);
    lv_obj_set_size(hdr, 800, 44);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, COL_BG, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    bare_obj(hdr);

    lv_obj_t *back_btn = lv_btn_create(hdr);
    lv_obj_set_size(back_btn, 36, 28);
    lv_obj_set_pos(back_btn, 4, 8);
    lv_obj_set_style_bg_color(back_btn, COL_BTN_GRAY, 0);
    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(back_icon);
    lv_obj_add_event_cb(back_btn, btn_back_to_settings_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *hdr_title = lv_label_create(hdr);
    lv_label_set_text(hdr_title, title);
    lv_obj_set_pos(hdr_title, 48, 14);
    lv_obj_set_style_text_color(hdr_title, COL_WHITE, 0);
    lv_obj_set_style_text_font(hdr_title, &lv_font_montserrat_16, 0);

    // mem selector pill: 168×30 at hdr(230, 7)
    lv_obj_t *ms = lv_obj_create(hdr);
    lv_obj_set_size(ms, 168, 30);
    lv_obj_set_pos(ms, 230, 7);
    lv_obj_set_style_bg_color(ms, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(ms, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ms, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(ms, 1, 0);
    lv_obj_set_style_radius(ms, 6, 0);
    lv_obj_set_style_pad_all(ms, 0, 0);
    lv_obj_remove_flag(ms, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ms_prev = lv_btn_create(ms);
    lv_obj_set_size(ms_prev, 24, 22);
    lv_obj_set_pos(ms_prev, 4, 4);
    lv_obj_set_style_bg_color(ms_prev, lv_color_hex(0x111827), 0);
    lv_obj_set_style_radius(ms_prev, 4, 0);
    lv_obj_set_style_pad_all(ms_prev, 0, 0);
    lv_obj_t *ms_p_icon = lv_label_create(ms_prev);
    lv_label_set_text(ms_p_icon, LV_SYMBOL_LEFT);
    lv_obj_center(ms_p_icon);
    lv_obj_set_style_text_color(ms_p_icon, lv_color_hex(0x64748b), 0);

    lv_obj_t *ms_cap = lv_label_create(ms);
    lv_label_set_text(ms_cap, "MEM");
    lv_obj_set_pos(ms_cap, 28, 2);
    lv_obj_set_width(ms_cap, 112);
    lv_obj_set_style_text_color(ms_cap, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(ms_cap, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_align(ms_cap, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *ms_no = lv_label_create(ms);
    {
        char buf[8];
        lv_snprintf(buf, sizeof(buf), "%03u", (unsigned)g_mem_no);
        lv_label_set_text(ms_no, buf);
    }
    lv_obj_set_pos(ms_no, 28, 14);
    lv_obj_set_width(ms_no, 112);
    lv_obj_set_style_text_color(ms_no, COL_BLUE, 0);
    lv_obj_set_style_text_font(ms_no, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(ms_no, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(ms_no, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ms_no, show_mem_popup_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *ms_next = lv_btn_create(ms);
    lv_obj_set_size(ms_next, 24, 22);
    lv_obj_set_pos(ms_next, 140, 4);
    lv_obj_set_style_bg_color(ms_next, lv_color_hex(0x111827), 0);
    lv_obj_set_style_radius(ms_next, 4, 0);
    lv_obj_set_style_pad_all(ms_next, 0, 0);
    lv_obj_t *ms_n_icon = lv_label_create(ms_next);
    lv_label_set_text(ms_n_icon, LV_SYMBOL_RIGHT);
    lv_obj_center(ms_n_icon);
    lv_obj_set_style_text_color(ms_n_icon, lv_color_hex(0x64748b), 0);

    // mem name + sub (sibling in hdr)
    lv_obj_t *ms_name = lv_label_create(hdr);
    lv_label_set_text(ms_name, "M42: phi8 M8 tap");
    lv_obj_set_pos(ms_name, 406, 8);
    lv_obj_set_style_text_color(ms_name, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(ms_name, &lv_font_montserrat_10, 0);

    lv_obj_t *ms_sub = lv_label_create(hdr);
    lv_label_set_text(ms_sub, "<  editing this setting");
    lv_obj_set_pos(ms_sub, 406, 22);
    lv_obj_set_style_text_color(ms_sub, lv_color_hex(0x334155), 0);
    lv_obj_set_style_text_font(ms_sub, &lv_font_montserrat_10, 0);

    // amber warning badge at hdr(660, 8, 128, 28)
    lv_obj_t *warn_badge = lv_obj_create(hdr);
    lv_obj_set_pos(warn_badge, 660, 8);
    lv_obj_set_size(warn_badge, 128, 28);
    lv_obj_set_style_bg_color(warn_badge, lv_color_hex(0x422006), 0);
    lv_obj_set_style_bg_opa(warn_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(warn_badge, lv_color_hex(0xf59e0b), 0);
    lv_obj_set_style_border_width(warn_badge, 1, 0);
    lv_obj_set_style_radius(warn_badge, 4, 0);
    lv_obj_set_style_pad_all(warn_badge, 0, 0);
    lv_obj_remove_flag(warn_badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *warn_l1 = lv_label_create(warn_badge);
    lv_label_set_text(warn_l1, "! MEM switch");
    lv_obj_set_pos(warn_l1, 4, 2);
    lv_obj_set_style_text_color(warn_l1, lv_color_hex(0xf59e0b), 0);
    lv_obj_set_style_text_font(warn_l1, &lv_font_montserrat_10, 0);

    lv_obj_t *warn_l2 = lv_label_create(warn_badge);
    lv_label_set_text(warn_l2, "changes settings");
    lv_obj_set_pos(warn_l2, 4, 14);
    lv_obj_set_style_text_color(warn_l2, lv_color_hex(0xd97706), 0);
    lv_obj_set_style_text_font(warn_l2, &lv_font_montserrat_10, 0);

    // コンテンツエリア
    lv_obj_t *content = lv_obj_create(*out_scr);
    lv_obj_set_size(content, 800, 436);
    lv_obj_set_pos(content, 0, 44);
    lv_obj_set_style_bg_color(content, COL_BG, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(content, 0, 0);
    bare_obj(content);

    return content;
}

// ============================================================
//  設定サブ画面  各スケルトン
// ============================================================

static void create_scr_meas_cond(void)
{
    lv_obj_t *c = make_settings_subscr(&scr_meas_cond, "Meas. Conditions");

    // ── Tab bar  y=0 h=40 ─────────────────────────────────────
    lv_obj_t *tab_bg = lv_obj_create(c);
    lv_obj_set_size(tab_bg, 800, 40);
    lv_obj_set_pos(tab_bg, 0, 0);
    lv_obj_set_style_bg_color(tab_bg, COL_BG, 0);
    lv_obj_set_style_bg_opa(tab_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tab_bg, 0, 0);
    bare_obj(tab_bg);

    // Tab "Manual" (inactive)
    {
        lv_obj_t *tab = lv_obj_create(tab_bg);
        lv_obj_set_size(tab, 200, 40);
        lv_obj_set_pos(tab, 0, 0);
        lv_obj_set_style_bg_color(tab, COL_BG, 0);
        lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
        bare_obj(tab);
        lv_obj_t *lbl = lv_label_create(tab);
        lv_label_set_text(lbl, "Manual");
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }
    // Tab "Auto" (active, blue underline)
    {
        lv_obj_t *tab = lv_obj_create(tab_bg);
        lv_obj_set_size(tab, 200, 40);
        lv_obj_set_pos(tab, 200, 0);
        lv_obj_set_style_bg_color(tab, COL_SURFACE, 0);
        lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, 0);
        bare_obj(tab);
        lv_obj_t *lbl = lv_label_create(tab);
        lv_label_set_text(lbl, "Auto");
        lv_obj_set_style_text_color(lbl, COL_WHITE, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
        lv_obj_t *ul = lv_obj_create(tab);
        lv_obj_set_size(ul, 200, 3);
        lv_obj_set_pos(ul, 0, 37);
        lv_obj_set_style_bg_color(ul, COL_BLUE, 0);
        lv_obj_set_style_bg_opa(ul, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ul, 0, 0);
        bare_obj(ul);
    }

    // ── Ch selection bar  y=40 h=48 ───────────────────────────
    lv_obj_t *ch_bar = lv_obj_create(c);
    lv_obj_set_size(ch_bar, 784, 48);
    lv_obj_set_pos(ch_bar, 8, 40);
    lv_obj_set_style_bg_color(ch_bar, COL_BG, 0);
    lv_obj_set_style_bg_opa(ch_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ch_bar, 6, 0);
    lv_obj_set_style_border_color(ch_bar, COL_SURFACE, 0);
    lv_obj_set_style_border_width(ch_bar, 1, 0);
    lv_obj_set_style_pad_all(ch_bar, 0, 0);
    lv_obj_remove_flag(ch_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_ch_hdr = lv_label_create(ch_bar);
    lv_label_set_text(lbl_ch_hdr, "Target Ch");
    lv_obj_set_pos(lbl_ch_hdr, 12, 18);
    lv_obj_set_style_text_color(lbl_ch_hdr, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_ch_hdr, &lv_font_montserrat_10, 0);

    // Ch1,Ch2=ON  Ch3,Ch4=OFF  Ch5=ON  Ch6,Ch7=OFF  Ch8=ON
    static const bool ch_on[8] = {true,true,false,false,true,false,false,true};
    for (int i = 0; i < 8; i++) {
        lv_obj_t *btn = lv_btn_create(ch_bar);
        lv_obj_set_size(btn, 50, 28);
        lv_obj_set_pos(btn, 76 + i * 56, 10);
        lv_obj_set_style_bg_color(btn, ch_on[i] ? lv_color_hex(0x1e3a5f) : COL_SURFACE, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_border_color(btn, ch_on[i] ? COL_BLUE : COL_BTN_GRAY, 0);
        lv_obj_set_style_border_width(btn, ch_on[i] ? 2 : 1, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        char ch_str[8];
        lv_snprintf(ch_str, sizeof(ch_str), "Ch %d", i + 1);
        lv_obj_t *lch = lv_label_create(btn);
        lv_label_set_text(lch, ch_str);
        lv_obj_set_style_text_color(lch, ch_on[i] ? lv_color_hex(0x93c5fd) : lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(lch, &lv_font_montserrat_10, 0);
        lv_obj_center(lch);
    }
    // 76 + 8*56 = 524 → ALL at 528, CLEAR at 584
    lv_obj_t *btn_sel_all = lv_btn_create(ch_bar);
    lv_obj_set_size(btn_sel_all, 50, 24);
    lv_obj_set_pos(btn_sel_all, 528, 12);
    lv_obj_set_style_bg_color(btn_sel_all, COL_SURFACE, 0);
    lv_obj_set_style_border_color(btn_sel_all, COL_BTN_GRAY, 0);
    lv_obj_set_style_border_width(btn_sel_all, 1, 0);
    lv_obj_set_style_radius(btn_sel_all, 3, 0);
    lv_obj_set_style_pad_all(btn_sel_all, 0, 0);
    lv_obj_t *lbl_sel_all = lv_label_create(btn_sel_all);
    lv_label_set_text(lbl_sel_all, "ALL");
    lv_obj_set_style_text_color(lbl_sel_all, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_sel_all, &lv_font_montserrat_10, 0);
    lv_obj_center(lbl_sel_all);

    lv_obj_t *btn_sel_clr = lv_btn_create(ch_bar);
    lv_obj_set_size(btn_sel_clr, 50, 24);
    lv_obj_set_pos(btn_sel_clr, 584, 12);
    lv_obj_set_style_bg_color(btn_sel_clr, COL_SURFACE, 0);
    lv_obj_set_style_border_color(btn_sel_clr, COL_BTN_GRAY, 0);
    lv_obj_set_style_border_width(btn_sel_clr, 1, 0);
    lv_obj_set_style_radius(btn_sel_clr, 3, 0);
    lv_obj_set_style_pad_all(btn_sel_clr, 0, 0);
    lv_obj_t *lbl_sel_clr = lv_label_create(btn_sel_clr);
    lv_label_set_text(lbl_sel_clr, "CLR");
    lv_obj_set_style_text_color(lbl_sel_clr, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_sel_clr, &lv_font_montserrat_10, 0);
    lv_obj_center(lbl_sel_clr);

    // ── Step indicator  circles cy=122 ────────────────────────
    static const char *step_lbl[3] = {"OK Sampling", "NG Sampling", "Optimize"};
    static const int   step_cx[3]  = {200, 400, 600};
    const int step_cy = 122;

    for (int s = 0; s < 2; s++) {
        lv_obj_t *ln = lv_obj_create(c);
        lv_obj_set_size(ln, step_cx[s+1] - step_cx[s] - 28, 1);
        lv_obj_set_pos(ln, step_cx[s] + 14, step_cy);
        lv_obj_set_style_bg_color(ln, COL_SURFACE, 0);
        lv_obj_set_style_bg_opa(ln, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ln, 0, 0);
        bare_obj(ln);
    }
    for (int s = 0; s < 3; s++) {
        lv_obj_t *circ = lv_obj_create(c);
        lv_obj_set_size(circ, 28, 28);
        lv_obj_set_pos(circ, step_cx[s] - 14, step_cy - 14);
        lv_obj_set_style_radius(circ, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(circ, s == 0 ? lv_color_hex(0x1e3a5f) : COL_SURFACE, 0);
        lv_obj_set_style_bg_opa(circ, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(circ, s == 0 ? COL_BLUE : COL_BTN_GRAY, 0);
        lv_obj_set_style_border_width(circ, 2, 0);
        lv_obj_set_style_pad_all(circ, 0, 0);
        lv_obj_remove_flag(circ, LV_OBJ_FLAG_SCROLLABLE);
        char num[4];
        lv_snprintf(num, sizeof(num), "%d", s + 1);
        lv_obj_t *lnum = lv_label_create(circ);
        lv_label_set_text(lnum, num);
        lv_obj_set_style_text_color(lnum, s == 0 ? COL_BLUE : lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(lnum, &lv_font_montserrat_14, 0);
        lv_obj_center(lnum);

        lv_obj_t *lstep = lv_label_create(c);
        lv_label_set_text(lstep, step_lbl[s]);
        lv_obj_set_style_text_color(lstep, s == 0 ? COL_BLUE : lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(lstep, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(lstep, step_cx[s] - 40, step_cy + 16);
        lv_obj_set_width(lstep, 80);
        lv_obj_set_style_text_align(lstep, LV_TEXT_ALIGN_CENTER, 0);
    }

    // ── Step 1: left instruction panel  x=8 y=158 w=330 h=210 ─
    lv_obj_t *inst = lv_obj_create(c);
    lv_obj_set_size(inst, 330, 210);
    lv_obj_set_pos(inst, 8, 158);
    lv_obj_set_style_bg_color(inst, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(inst, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(inst, 8, 0);
    lv_obj_set_style_border_color(inst, COL_BLUE, 0);
    lv_obj_set_style_border_width(inst, 2, 0);
    lv_obj_set_style_pad_all(inst, 0, 0);
    lv_obj_remove_flag(inst, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *acc = lv_obj_create(inst);
    lv_obj_set_size(acc, 330, 4);
    lv_obj_set_pos(acc, 0, 0);
    lv_obj_set_style_bg_color(acc, COL_BLUE, 0);
    lv_obj_set_style_bg_opa(acc, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(acc, 0, 0);
    bare_obj(acc);

    lv_obj_t *lbl_p_title = lv_label_create(inst);
    lv_label_set_text(lbl_p_title, "Step 1 — OK Sampling");
    lv_obj_set_pos(lbl_p_title, 16, 12);
    lv_obj_set_style_text_color(lbl_p_title, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(lbl_p_title, &lv_font_montserrat_12, 0);

    lv_obj_t *lbl_inst = lv_label_create(inst);
    lv_label_set_text(lbl_inst, "Set a conforming product and press\nthe button. All selected Ch measured.");
    lv_obj_set_pos(lbl_inst, 16, 32);
    lv_obj_set_style_text_color(lbl_inst, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_inst, &lv_font_montserrat_10, 0);
    lv_label_set_long_mode(lbl_inst, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_inst, 298);

    lv_obj_t *lbl_rec = lv_label_create(inst);
    lv_label_set_text(lbl_rec, "Recommended: 5+  /  Current: 0 times");
    lv_obj_set_pos(lbl_rec, 16, 66);
    lv_obj_set_style_text_color(lbl_rec, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(lbl_rec, &lv_font_montserrat_10, 0);

    // sampling button
    lv_obj_t *btn_samp = lv_btn_create(inst);
    lv_obj_set_size(btn_samp, 198, 52);
    lv_obj_set_pos(btn_samp, 16, 82);
    lv_obj_set_style_bg_color(btn_samp, COL_CELL_OK, 0);
    lv_obj_set_style_bg_opa(btn_samp, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_samp, 8, 0);
    lv_obj_set_style_border_color(btn_samp, COL_ACCENT_OK, 0);
    lv_obj_set_style_border_width(btn_samp, 2, 0);
    lv_obj_set_style_pad_all(btn_samp, 0, 0);
    lv_obj_t *lbl_samp1 = lv_label_create(btn_samp);
    lv_label_set_text(lbl_samp1, LV_SYMBOL_PLAY "  OK Sampling");
    lv_obj_set_style_text_color(lbl_samp1, lv_color_hex(0x86efac), 0);
    lv_obj_set_style_text_font(lbl_samp1, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_samp1, LV_ALIGN_CENTER, 0, -8);
    lv_obj_t *lbl_samp2 = lv_label_create(btn_samp);
    lv_label_set_text(lbl_samp2, "Measure all selected Ch");
    lv_obj_set_style_text_color(lbl_samp2, lv_color_hex(0x4ade80), 0);
    lv_obj_set_style_text_font(lbl_samp2, &lv_font_montserrat_10, 0);
    lv_obj_align(lbl_samp2, LV_ALIGN_CENTER, 0, 12);

    // count badge
    lv_obj_t *badge = lv_obj_create(inst);
    lv_obj_set_size(badge, 86, 52);
    lv_obj_set_pos(badge, 226, 82);
    lv_obj_set_style_bg_color(badge, COL_BG, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 6, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_pad_all(badge, 4, 0);
    lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_reg = lv_label_create(badge);
    lv_label_set_text(lbl_reg, "Registered");
    lv_obj_set_pos(lbl_reg, 0, 0);
    lv_obj_set_style_text_color(lbl_reg, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_reg, &lv_font_montserrat_10, 0);
    lv_obj_set_width(lbl_reg, 78);
    lv_obj_set_style_text_align(lbl_reg, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *lbl_cnt = lv_label_create(badge);
    lv_label_set_text(lbl_cnt, "0");
    lv_obj_set_pos(lbl_cnt, 0, 16);
    lv_obj_set_style_text_color(lbl_cnt, lv_color_hex(0x4ade80), 0);
    lv_obj_set_style_text_font(lbl_cnt, &lv_font_montserrat_24, 0);
    lv_obj_set_width(lbl_cnt, 62);
    lv_obj_set_style_text_align(lbl_cnt, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *lbl_times = lv_label_create(badge);
    lv_label_set_text(lbl_times, "times");
    lv_obj_set_pos(lbl_times, 64, 34);
    lv_obj_set_style_text_color(lbl_times, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(lbl_times, &lv_font_montserrat_10, 0);

    // Clear / Next step buttons
    lv_obj_t *btn_data_clr = lv_btn_create(inst);
    lv_obj_set_size(btn_data_clr, 72, 24);
    lv_obj_set_pos(btn_data_clr, 16, 150);
    lv_obj_set_style_bg_color(btn_data_clr, COL_SURFACE, 0);
    lv_obj_set_style_border_color(btn_data_clr, COL_BTN_GRAY, 0);
    lv_obj_set_style_border_width(btn_data_clr, 1, 0);
    lv_obj_set_style_radius(btn_data_clr, 4, 0);
    lv_obj_set_style_pad_all(btn_data_clr, 0, 0);
    lv_obj_t *lbl_data_clr = lv_label_create(btn_data_clr);
    lv_label_set_text(lbl_data_clr, "Clear");
    lv_obj_set_style_text_color(lbl_data_clr, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_data_clr, &lv_font_montserrat_10, 0);
    lv_obj_center(lbl_data_clr);

    lv_obj_t *btn_step_next = lv_btn_create(inst);
    lv_obj_set_size(btn_step_next, 76, 24);
    lv_obj_set_pos(btn_step_next, 238, 150);
    lv_obj_set_style_bg_color(btn_step_next, lv_color_hex(0x1e3a5f), 0);
    lv_obj_set_style_border_color(btn_step_next, lv_color_hex(0x1d4ed8), 0);
    lv_obj_set_style_border_width(btn_step_next, 1, 0);
    lv_obj_set_style_radius(btn_step_next, 4, 0);
    lv_obj_set_style_pad_all(btn_step_next, 0, 0);
    lv_obj_t *lbl_step_next = lv_label_create(btn_step_next);
    lv_label_set_text(lbl_step_next, "Next " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(lbl_step_next, lv_color_hex(0x93c5fd), 0);
    lv_obj_set_style_text_font(lbl_step_next, &lv_font_montserrat_10, 0);
    lv_obj_center(lbl_step_next);

    // ── Right: sample result table  x=346 y=158 w=446 h=210 ───
    lv_obj_t *tbl = lv_obj_create(c);
    lv_obj_set_size(tbl, 446, 210);
    lv_obj_set_pos(tbl, 346, 158);
    lv_obj_set_style_bg_color(tbl, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(tbl, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tbl, 8, 0);
    lv_obj_set_style_border_color(tbl, COL_SURFACE, 0);
    lv_obj_set_style_border_width(tbl, 1, 0);
    lv_obj_set_style_pad_all(tbl, 0, 0);
    lv_obj_remove_flag(tbl, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_tbl_title = lv_label_create(tbl);
    lv_label_set_text(lbl_tbl_title, "Sampling results (last 3 measurements)");
    lv_obj_set_pos(lbl_tbl_title, 16, 8);
    lv_obj_set_style_text_color(lbl_tbl_title, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(lbl_tbl_title, &lv_font_montserrat_10, 0);

    // column header row
    lv_obj_t *col_hdr = lv_obj_create(tbl);
    lv_obj_set_size(col_hdr, 446, 20);
    lv_obj_set_pos(col_hdr, 0, 24);
    lv_obj_set_style_bg_color(col_hdr, COL_BG, 0);
    lv_obj_set_style_bg_opa(col_hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(col_hdr, 0, 0);
    bare_obj(col_hdr);

    // col centers relative to tbl: Ch=32 #1=114 #2=184 #3=254 Avg=334 Var=408
    static const char *col_hdrs[] = {"Ch", "#1", "#2", "#3", "Avg", "Var"};
    static const int   col_cx[]   = {32, 114, 184, 254, 334, 408};
    for (int i = 0; i < 6; i++) {
        lv_obj_t *lhdr = lv_label_create(col_hdr);
        lv_label_set_text(lhdr, col_hdrs[i]);
        lv_obj_set_style_text_color(lhdr, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(lhdr, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(lhdr, col_cx[i] - 16, 4);
        lv_obj_set_width(lhdr, 32);
        lv_obj_set_style_text_align(lhdr, LV_TEXT_ALIGN_CENTER, 0);
    }

    // data rows (dummy: Ch1, Ch2, Ch5, Ch8)
    static const char *row_ch[]  = {"Ch1", "Ch2", "Ch5", "Ch8"};
    static const char *row_d1[]  = {"1.24/-0.87", "0.99/+1.10", "0.51/+0.33", "-0.12/+0.79"};
    static const char *row_d2[]  = {"1.20/-0.91", "0.95/+1.08", "0.50/+0.35", "-0.11/+0.81"};
    static const char *row_d3[]  = {"1.22/-0.88", "1.01/+1.12", "0.53/+0.32", "-0.13/+0.78"};
    static const char *row_avg[] = {"0.57", "0.62", "0.30", "0.46"};
    static const char *row_var[] = {"±0.01", "±0.01", "±0.01", "±0.03"};
    static const bool  row_warn[]= {false, false, false, true};

    for (int r = 0; r < 4; r++) {
        lv_obj_t *row = lv_obj_create(tbl);
        lv_obj_set_size(row, 446, 38);
        lv_obj_set_pos(row, 0, 44 + r * 40);
        lv_obj_set_style_bg_color(row, r % 2 == 0 ? lv_color_hex(0x111827) : COL_BG, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 0, 0);
        bare_obj(row);

        lv_obj_t *lch = lv_label_create(row);
        lv_label_set_text(lch, row_ch[r]);
        lv_obj_set_pos(lch, col_cx[0] - 14, 12);
        lv_obj_set_style_text_color(lch, lv_color_hex(0x93c5fd), 0);
        lv_obj_set_style_text_font(lch, &lv_font_montserrat_10, 0);

        const char *samples[3] = {row_d1[r], row_d2[r], row_d3[r]};
        for (int sc = 0; sc < 3; sc++) {
            lv_obj_t *lv = lv_label_create(row);
            lv_label_set_text(lv, samples[sc]);
            lv_obj_set_pos(lv, col_cx[sc + 1] - 28, 12);
            lv_obj_set_style_text_color(lv, lv_color_hex(0x6ee7b7), 0);
            lv_obj_set_style_text_font(lv, &lv_font_montserrat_10, 0);
            lv_obj_set_width(lv, 56);
            lv_obj_set_style_text_align(lv, LV_TEXT_ALIGN_CENTER, 0);
        }
        lv_obj_t *lavg = lv_label_create(row);
        lv_label_set_text(lavg, row_avg[r]);
        lv_obj_set_pos(lavg, col_cx[4] - 16, 12);
        lv_obj_set_style_text_color(lavg, lv_color_hex(0x4ade80), 0);
        lv_obj_set_style_text_font(lavg, &lv_font_montserrat_10, 0);
        lv_obj_set_width(lavg, 32);
        lv_obj_set_style_text_align(lavg, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t *lvar = lv_label_create(row);
        lv_label_set_text(lvar, row_var[r]);
        lv_obj_set_pos(lvar, col_cx[5] - 20, 12);
        lv_obj_set_style_text_color(lvar, row_warn[r] ? lv_color_hex(0xf59e0b) : lv_color_hex(0x4ade80), 0);
        lv_obj_set_style_text_font(lvar, &lv_font_montserrat_10, 0);
        lv_obj_set_width(lvar, 40);
        lv_obj_set_style_text_align(lvar, LV_TEXT_ALIGN_CENTER, 0);
    }

    // ── Bottom buttons  y=378 h=40 ────────────────────────────
    lv_obj_t *btn_cancel = lv_btn_create(c);
    lv_obj_set_size(btn_cancel, 148, 40);
    lv_obj_set_pos(btn_cancel, 8, 378);
    lv_obj_set_style_bg_color(btn_cancel, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_cancel, 6, 0);
    lv_obj_set_style_border_color(btn_cancel, COL_BTN_GRAY, 0);
    lv_obj_set_style_border_width(btn_cancel, 1, 0);
    lv_obj_set_style_pad_all(btn_cancel, 0, 0);
    lv_obj_add_event_cb(btn_cancel, btn_back_to_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_color(lbl_cancel, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_cancel);

    lv_obj_t *btn_next = lv_btn_create(c);
    lv_obj_set_size(btn_next, 148, 40);
    lv_obj_set_pos(btn_next, 644, 378);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0x166534), 0);
    lv_obj_set_style_bg_opa(btn_next, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_next, 6, 0);
    lv_obj_set_style_border_color(btn_next, COL_ACCENT_OK, 0);
    lv_obj_set_style_border_width(btn_next, 1, 0);
    lv_obj_set_style_pad_all(btn_next, 0, 0);
    lv_obj_t *lbl_next = lv_label_create(btn_next);
    lv_label_set_text(lbl_next, "Next " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(lbl_next, lv_color_hex(0xbbf7d0), 0);
    lv_obj_set_style_text_font(lbl_next, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl_next);
}

// ============================================================
//  threshold XY plot  canvas描画 (one-shot timer から呼ぶ)
//  Canvas 256x256, ARGB8888, center=(128,128), scale=32px/V
//  OK cluster center: canvas (168,94)
//  Fitted ellipse: rx=26 ry=20 (dashed)
//  Applied ellipse: rx=31 ry=24 (solid, k=1.20)
// ============================================================
static void draw_threshold_plot(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!threshold_plot_canvas) return;

    lv_layer_t layer;
    lv_canvas_init_layer(threshold_plot_canvas, &layer);

    /* ── 1. Background ─────────────────────────────────────── */
    {
        lv_draw_rect_dsc_t r;
        lv_draw_rect_dsc_init(&r);
        r.bg_color    = lv_color_hex(0x111827);
        r.bg_opa      = LV_OPA_COVER;
        r.radius      = 0;
        r.border_width = 0;
        lv_area_t a   = {0, 0, 255, 255};
        lv_draw_rect(&layer, &r, &a);
    }

    /* ── 2. Grid lines at ±2V (64px from center 128,128) ─── */
    {
        lv_draw_line_dsc_t g;
        lv_draw_line_dsc_init(&g);
        g.color = lv_color_hex(0x1e293b);
        g.width = 1;
        g.opa   = LV_OPA_COVER;
        static const int32_t HY[] = {64, 192};
        static const int32_t VX[] = {64, 192};
        for (int i = 0; i < 2; i++) {
            g.p1.x = 0;   g.p1.y = HY[i];
            g.p2.x = 255; g.p2.y = HY[i];
            lv_draw_line(&layer, &g);
        }
        for (int i = 0; i < 2; i++) {
            g.p1.x = VX[i]; g.p1.y = 0;
            g.p2.x = VX[i]; g.p2.y = 255;
            lv_draw_line(&layer, &g);
        }
    }

    /* ── 3. X / Y axes ─────────────────────────────────────── */
    {
        lv_draw_line_dsc_t ax;
        lv_draw_line_dsc_init(&ax);
        ax.color = lv_color_hex(0x2d3748);
        ax.width = 1;
        ax.opa   = LV_OPA_COVER;
        ax.p1.x = 0;   ax.p1.y = 128; ax.p2.x = 255; ax.p2.y = 128;
        lv_draw_line(&layer, &ax);
        ax.p1.x = 128; ax.p1.y = 0;   ax.p2.x = 128; ax.p2.y = 255;
        lv_draw_line(&layer, &ax);
    }

    /* ── 4. Axis labels ─────────────────────────────────────── */
    {
        lv_draw_label_dsc_t lbl;
        lv_draw_label_dsc_init(&lbl);
        lbl.font = &lv_font_montserrat_10;
        lbl.opa  = LV_OPA_COVER;

        lbl.color = lv_color_hex(0x475569);
        lbl.text  = "X";
        lv_area_t a1 = {240, 120, 255, 133};
        lv_draw_label(&layer, &lbl, &a1);

        lbl.text  = "Y";
        lv_area_t a2 = {130, 0, 145, 13};
        lv_draw_label(&layer, &lbl, &a2);

        lbl.color = lv_color_hex(0x374151);
        lbl.text  = "+2";
        lv_area_t a3 = {183, 130, 207, 143};
        lv_draw_label(&layer, &lbl, &a3);

        lbl.text  = "-2";
        lv_area_t a4 = {50, 130, 74, 143};
        lv_draw_label(&layer, &lbl, &a4);

        lbl.text  = "+2";
        lv_area_t a5 = {130, 57, 154, 70};
        lv_draw_label(&layer, &lbl, &a5);

        lbl.text  = "-2";
        lv_area_t a6 = {130, 185, 154, 198};
        lv_draw_label(&layer, &lbl, &a6);
    }

    /* ── 5. OK sample dots (green, 7 pts) ──────────────────── */
    {
        static const int32_t OX[] = {170, 166, 172, 168, 164, 171, 167};
        static const int32_t OY[] = { 94,  98,  91,  96,  92,  99,  89};
        lv_draw_rect_dsc_t dot;
        lv_draw_rect_dsc_init(&dot);
        dot.bg_color    = lv_color_hex(0x22c55e);
        dot.bg_opa      = (lv_opa_t)204;
        dot.radius      = LV_RADIUS_CIRCLE;
        dot.border_width = 0;
        for (int i = 0; i < 7; i++) {
            lv_area_t a = {OX[i]-3, OY[i]-3, OX[i]+3, OY[i]+3};
            lv_draw_rect(&layer, &dot, &a);
        }
    }

    /* ── 6. NG sample dots (red, 5 pts) ────────────────────── */
    {
        static const int32_t NX[] = {222, 227, 217,  32,  37};
        static const int32_t NY[] = { 54,  48,  58, 206, 212};
        lv_draw_rect_dsc_t dot;
        lv_draw_rect_dsc_init(&dot);
        dot.bg_color    = lv_color_hex(0xef4444);
        dot.bg_opa      = (lv_opa_t)204;
        dot.radius      = LV_RADIUS_CIRCLE;
        dot.border_width = 0;
        for (int i = 0; i < 5; i++) {
            lv_area_t a = {NX[i]-3, NY[i]-3, NX[i]+3, NY[i]+3};
            lv_draw_rect(&layer, &dot, &a);
        }
    }

    /* ── 7. Fitted ellipse (dashed, 50% opacity, rx=26 ry=20) */
    {
        lv_draw_line_dsc_t el;
        lv_draw_line_dsc_init(&el);
        el.color      = COL_BLUE;
        el.width      = 1;
        el.opa        = LV_OPA_50;
        el.dash_width = 4;
        el.dash_gap   = 3;
        const double STEP = 3.14159265358979 / 30.0;
        for (int i = 0; i < 60; i++) {
            double a1 = i * STEP, a2 = (i + 1) * STEP;
            el.p1.x = (int32_t)(168.0 + 26.0 * cos(a1));
            el.p1.y = (int32_t)( 94.0 + 20.0 * sin(a1));
            el.p2.x = (int32_t)(168.0 + 26.0 * cos(a2));
            el.p2.y = (int32_t)( 94.0 + 20.0 * sin(a2));
            lv_draw_line(&layer, &el);
        }
    }

    /* ── 8. Applied ellipse (solid, 90% opacity, rx=31 ry=24) */
    {
        lv_draw_line_dsc_t el;
        lv_draw_line_dsc_init(&el);
        el.color = COL_BLUE;
        el.width = 2;
        el.opa   = (lv_opa_t)230;
        const double STEP = 3.14159265358979 / 30.0;
        for (int i = 0; i < 60; i++) {
            double a1 = i * STEP, a2 = (i + 1) * STEP;
            el.p1.x = (int32_t)(168.0 + 31.0 * cos(a1));
            el.p1.y = (int32_t)( 94.0 + 24.0 * sin(a1));
            el.p2.x = (int32_t)(168.0 + 31.0 * cos(a2));
            el.p2.y = (int32_t)( 94.0 + 24.0 * sin(a2));
            lv_draw_line(&layer, &el);
        }
    }

    /* ── 9. a / b axis guide lines (dashed) ────────────────── */
    {
        lv_draw_line_dsc_t g;
        lv_draw_line_dsc_init(&g);
        g.color      = COL_BLUE;
        g.width      = 1;
        g.opa        = LV_OPA_50;
        g.dash_width = 3;
        g.dash_gap   = 2;
        g.p1.x = 168; g.p1.y = 94; g.p2.x = 199; g.p2.y = 94;
        lv_draw_line(&layer, &g);
        g.p1.x = 168; g.p1.y = 94; g.p2.x = 168; g.p2.y = 70;
        lv_draw_line(&layer, &g);
    }

    /* ── 10. "a" / "b" axis labels ─────────────────────────── */
    {
        lv_draw_label_dsc_t lbl;
        lv_draw_label_dsc_init(&lbl);
        lbl.font  = &lv_font_montserrat_10;
        lbl.opa   = LV_OPA_COVER;
        lbl.color = COL_BLUE;
        lbl.text  = "a";
        lv_area_t a1 = {201, 87, 214, 100};
        lv_draw_label(&layer, &lbl, &a1);
        lbl.text  = "b";
        lv_area_t a2 = {170, 59, 183, 72};
        lv_draw_label(&layer, &lbl, &a2);
    }

    lv_canvas_finish_layer(threshold_plot_canvas, &layer);
}

// ============================================================
//  scr_threshold  閾値設定画面 (3ペイン)
// ============================================================
static void create_scr_threshold(void)
{
    lv_obj_t *c = make_settings_subscr(&scr_threshold, "Threshold");
    // c = content container 800×436, y=44 on screen
    // Coordinate mapping: content_(x,y) = SVG_(x, y-44)

    // ── LEFT: Channel list  x=8 y=8 w=150 h=410 ─────────────
    lv_obj_t *ch_panel = lv_obj_create(c);
    lv_obj_set_pos(ch_panel, 8, 8);
    lv_obj_set_size(ch_panel, 150, 410);
    lv_obj_set_style_bg_color(ch_panel, COL_BG, 0);
    lv_obj_set_style_bg_opa(ch_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ch_panel, 8, 0);
    lv_obj_set_style_border_color(ch_panel, COL_SURFACE, 0);
    lv_obj_set_style_border_width(ch_panel, 1, 0);
    lv_obj_set_style_pad_all(ch_panel, 0, 0);
    lv_obj_remove_flag(ch_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_ch_hdr = lv_label_create(ch_panel);
    lv_label_set_text(lbl_ch_hdr, "Channel");
    lv_obj_set_pos(lbl_ch_hdr, 12, 8);
    lv_obj_set_style_text_color(lbl_ch_hdr, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(lbl_ch_hdr, &lv_font_montserrat_10, 0);

    // 5 channel items (Ch1=selected, Ch3=warning/no-sample)
    static const char *const CH_NAMES[]  = { "Ch 1", "Ch 2", "Ch 3", "Ch 5", "Ch 8" };
    static const char *const CH_AB[]     = { "a=1.80  b=1.35", "a=1.52  b=1.10",
                                              "Not sampled", "a=0.92  b=0.68", "a=1.24  b=0.94" };
    static const char *const CH_KS[]     = { "k=1.20 [set]", "k=1.20 [set]",
                                              "", "k=1.30 [set]", "k=1.20 [set]" };
    static const int32_t ITEM_Y[]        = { 24, 80, 136, 192, 248 };
    static const bool    CH_SEL[]        = { true, false, false, false, false };
    static const bool    CH_WARN[]       = { false, false, true, false, false };

    for (int i = 0; i < 5; i++) {
        lv_obj_t *item = lv_obj_create(ch_panel);
        lv_obj_set_pos(item, 0, ITEM_Y[i]);
        lv_obj_set_size(item, 150, 56);
        lv_obj_set_style_bg_color(item, CH_SEL[i] ? lv_color_hex(0x1e3a5f) : lv_color_hex(0x111827), 0);
        lv_obj_set_style_bg_opa(item, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(item, 0, 0);
        bare_obj(item);

        // 4px accent bar on left edge
        lv_obj_t *bar = lv_obj_create(item);
        lv_obj_set_pos(bar, 0, 0);
        lv_obj_set_size(bar, 4, 56);
        lv_obj_set_style_bg_color(bar, CH_SEL[i] ? COL_BLUE : COL_SURFACE, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar, 0, 0);
        bare_obj(bar);

        lv_obj_t *lbl_name = lv_label_create(item);
        lv_label_set_text(lbl_name, CH_NAMES[i]);
        lv_obj_set_pos(lbl_name, 16, 8);
        lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_name,
            CH_SEL[i] ? lv_color_hex(0x93c5fd) : lv_color_hex(0x94a3b8), 0);

        if (CH_AB[i][0]) {
            lv_obj_t *lbl_ab = lv_label_create(item);
            lv_label_set_text(lbl_ab, CH_AB[i]);
            lv_obj_set_pos(lbl_ab, 16, 28);
            lv_obj_set_style_text_font(lbl_ab, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_ab,
                CH_SEL[i] ? lv_color_hex(0x475569) : lv_color_hex(0x374151), 0);
        }
        if (CH_KS[i][0]) {
            lv_obj_t *lbl_ks = lv_label_create(item);
            lv_label_set_text(lbl_ks, CH_KS[i]);
            lv_obj_set_pos(lbl_ks, 16, 40);
            lv_obj_set_style_text_font(lbl_ks, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_ks,
                CH_SEL[i] ? lv_color_hex(0x475569) : lv_color_hex(0x374151), 0);
        }
        if (CH_WARN[i]) {
            lv_obj_t *warn = lv_obj_create(item);
            lv_obj_set_pos(warn, 130, 8);
            lv_obj_set_size(warn, 10, 10);
            lv_obj_set_style_bg_color(warn, lv_color_hex(0xf59e0b), 0);
            lv_obj_set_style_bg_opa(warn, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(warn, LV_RADIUS_CIRCLE, 0);
            bare_obj(warn);
        }
    }

    // ── CENTER: XY plot panel  x=166 y=8 w=280 h=380 ─────────
    lv_obj_t *plot_panel = lv_obj_create(c);
    lv_obj_set_pos(plot_panel, 166, 8);
    lv_obj_set_size(plot_panel, 280, 380);
    lv_obj_set_style_bg_color(plot_panel, COL_BG, 0);
    lv_obj_set_style_bg_opa(plot_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(plot_panel, 8, 0);
    lv_obj_set_style_border_color(plot_panel, COL_SURFACE, 0);
    lv_obj_set_style_border_width(plot_panel, 1, 0);
    lv_obj_set_style_pad_all(plot_panel, 0, 0);
    lv_obj_remove_flag(plot_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Canvas 256x256 at panel inner (12,12)
    threshold_plot_canvas = lv_canvas_create(plot_panel);
    lv_canvas_set_buffer(threshold_plot_canvas,
                         threshold_plot_buf, 256, 256, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(threshold_plot_canvas, 12, 12);

    // Legend: OK dot + label at panel inner y=280
    lv_obj_t *ok_dot = lv_obj_create(plot_panel);
    lv_obj_set_pos(ok_dot, 20, 284);
    lv_obj_set_size(ok_dot, 8, 8);
    lv_obj_set_style_bg_color(ok_dot, COL_ACCENT_OK, 0);
    lv_obj_set_style_bg_opa(ok_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ok_dot, LV_RADIUS_CIRCLE, 0);
    bare_obj(ok_dot);

    lv_obj_t *lbl_ok_leg = lv_label_create(plot_panel);
    lv_label_set_text(lbl_ok_leg, "OK samples (7)");
    lv_obj_set_pos(lbl_ok_leg, 32, 280);
    lv_obj_set_style_text_font(lbl_ok_leg, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_ok_leg, lv_color_hex(0x86efac), 0);

    // NG dot + label at y=296
    lv_obj_t *ng_dot = lv_obj_create(plot_panel);
    lv_obj_set_pos(ng_dot, 20, 300);
    lv_obj_set_size(ng_dot, 8, 8);
    lv_obj_set_style_bg_color(ng_dot, COL_ACCENT_NG, 0);
    lv_obj_set_style_bg_opa(ng_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ng_dot, LV_RADIUS_CIRCLE, 0);
    bare_obj(ng_dot);

    lv_obj_t *lbl_ng_leg = lv_label_create(plot_panel);
    lv_label_set_text(lbl_ng_leg, "NG samples (5)");
    lv_obj_set_pos(lbl_ng_leg, 32, 296);
    lv_obj_set_style_text_font(lbl_ng_leg, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_ng_leg, lv_color_hex(0xfca5a5), 0);

    // Applied ellipse legend line + label at y=312
    lv_obj_t *lbl_applied_line = lv_label_create(plot_panel);
    lv_label_set_text(lbl_applied_line, "---");
    lv_obj_set_pos(lbl_applied_line, 18, 312);
    lv_obj_set_style_text_font(lbl_applied_line, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_applied_line, COL_BLUE, 0);

    lv_obj_t *lbl_applied_leg = lv_label_create(plot_panel);
    lv_label_set_text(lbl_applied_leg, "Threshold (k=1.20)");
    lv_obj_set_pos(lbl_applied_leg, 38, 312);
    lv_obj_set_style_text_font(lbl_applied_leg, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_applied_leg, lv_color_hex(0x7dd3fc), 0);

    // Fitted ellipse legend at y=326
    lv_obj_t *lbl_fitted_line = lv_label_create(plot_panel);
    lv_label_set_text(lbl_fitted_line, "- -");
    lv_obj_set_pos(lbl_fitted_line, 18, 326);
    lv_obj_set_style_text_font(lbl_fitted_line, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_fitted_line, lv_color_hex(0x475569), 0);

    lv_obj_t *lbl_fitted_leg = lv_label_create(plot_panel);
    lv_label_set_text(lbl_fitted_leg, "Fitted ellipse (raw)");
    lv_obj_set_pos(lbl_fitted_leg, 38, 326);
    lv_obj_set_style_text_font(lbl_fitted_leg, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_fitted_leg, lv_color_hex(0x475569), 0);

    // Plot title (bottom-center of panel)
    lv_obj_t *lbl_plot_title = lv_label_create(plot_panel);
    lv_label_set_text(lbl_plot_title, "Ch 1  X-Y Plot");
    lv_obj_set_style_text_font(lbl_plot_title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_plot_title, COL_MUTED, 0);
    lv_obj_align(lbl_plot_title, LV_ALIGN_BOTTOM_MID, 0, -10);

    // ── RIGHT: Controls panel  x=454 y=8 w=338 h=410 ─────────
    lv_obj_t *ctrl = lv_obj_create(c);
    lv_obj_set_pos(ctrl, 454, 8);
    lv_obj_set_size(ctrl, 338, 410);
    lv_obj_set_style_bg_color(ctrl, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(ctrl, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ctrl, 8, 0);
    lv_obj_set_style_border_color(ctrl, COL_BTN_GRAY, 0);
    lv_obj_set_style_border_width(ctrl, 1, 0);
    lv_obj_set_style_pad_all(ctrl, 0, 0);
    lv_obj_remove_flag(ctrl, LV_OBJ_FLAG_SCROLLABLE);

    // Title "Ch 1 - Threshold Params"
    lv_obj_t *lbl_ctrl_ttl = lv_label_create(ctrl);
    lv_label_set_text(lbl_ctrl_ttl, "Ch 1  -  Threshold Params");
    lv_obj_set_pos(lbl_ctrl_ttl, 16, 22);
    lv_obj_set_style_text_font(lbl_ctrl_ttl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_ctrl_ttl, lv_color_hex(0x94a3b8), 0);

    // Sampling section label
    lv_obj_t *lbl_samp_hdr = lv_label_create(ctrl);
    lv_label_set_text(lbl_samp_hdr, "Sampling");
    lv_obj_set_pos(lbl_samp_hdr, 16, 44);
    lv_obj_set_style_text_font(lbl_samp_hdr, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_samp_hdr, lv_color_hex(0x64748b), 0);

    // OK Sampling button (16, 50, 148x40)
    lv_obj_t *btn_ok_samp = lv_obj_create(ctrl);
    lv_obj_set_pos(btn_ok_samp, 16, 50);
    lv_obj_set_size(btn_ok_samp, 148, 40);
    lv_obj_set_style_bg_color(btn_ok_samp, lv_color_hex(0x14532d), 0);
    lv_obj_set_style_bg_opa(btn_ok_samp, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_ok_samp, 6, 0);
    lv_obj_set_style_border_color(btn_ok_samp, COL_ACCENT_OK, 0);
    lv_obj_set_style_border_width(btn_ok_samp, 2, 0);
    lv_obj_set_style_pad_all(btn_ok_samp, 0, 0);
    lv_obj_remove_flag(btn_ok_samp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *lbl_ok_top = lv_label_create(btn_ok_samp);
    lv_label_set_text(lbl_ok_top, LV_SYMBOL_PLAY "  OK Sampling");
    lv_obj_set_style_text_font(lbl_ok_top, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_ok_top, lv_color_hex(0x86efac), 0);
    lv_obj_set_pos(lbl_ok_top, 8, 6);
    lv_obj_t *lbl_ok_cnt = lv_label_create(btn_ok_samp);
    lv_label_set_text(lbl_ok_cnt, "7 registered");
    lv_obj_set_style_text_font(lbl_ok_cnt, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_ok_cnt, lv_color_hex(0x4ade80), 0);
    lv_obj_set_pos(lbl_ok_cnt, 8, 22);

    // NG Sampling button (170, 50, 148x40)
    lv_obj_t *btn_ng_samp = lv_obj_create(ctrl);
    lv_obj_set_pos(btn_ng_samp, 170, 50);
    lv_obj_set_size(btn_ng_samp, 148, 40);
    lv_obj_set_style_bg_color(btn_ng_samp, lv_color_hex(0x7f1d1d), 0);
    lv_obj_set_style_bg_opa(btn_ng_samp, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_ng_samp, 6, 0);
    lv_obj_set_style_border_color(btn_ng_samp, COL_ACCENT_NG, 0);
    lv_obj_set_style_border_width(btn_ng_samp, 2, 0);
    lv_obj_set_style_pad_all(btn_ng_samp, 0, 0);
    lv_obj_remove_flag(btn_ng_samp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *lbl_ng_top = lv_label_create(btn_ng_samp);
    lv_label_set_text(lbl_ng_top, LV_SYMBOL_PLAY "  NG Sampling");
    lv_obj_set_style_text_font(lbl_ng_top, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_ng_top, lv_color_hex(0xfca5a5), 0);
    lv_obj_set_pos(lbl_ng_top, 8, 6);
    lv_obj_t *lbl_ng_cnt = lv_label_create(btn_ng_samp);
    lv_label_set_text(lbl_ng_cnt, "5 registered");
    lv_obj_set_style_text_font(lbl_ng_cnt, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_ng_cnt, lv_color_hex(0xfca5a5), 0);
    lv_obj_set_pos(lbl_ng_cnt, 8, 22);

    // Divider 1 at y=100
    {
        lv_obj_t *d = lv_obj_create(ctrl);
        lv_obj_set_pos(d, 16, 100); lv_obj_set_size(d, 312, 1);
        lv_obj_set_style_bg_color(d, COL_BTN_GRAY, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        bare_obj(d);
    }

    // Fit Result section label at (16, 118)
    lv_obj_t *lbl_fit_hdr = lv_label_create(ctrl);
    lv_label_set_text(lbl_fit_hdr, "Fit Result  (auto from samples)");
    lv_obj_set_pos(lbl_fit_hdr, 16, 118);
    lv_obj_set_style_text_font(lbl_fit_hdr, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_fit_hdr, lv_color_hex(0x64748b), 0);

    // a box (16, 126, 148x44)
    lv_obj_t *box_a = lv_obj_create(ctrl);
    lv_obj_set_pos(box_a, 16, 126); lv_obj_set_size(box_a, 148, 44);
    lv_obj_set_style_bg_color(box_a, COL_BG, 0);
    lv_obj_set_style_bg_opa(box_a, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box_a, 6, 0);
    bare_obj(box_a);
    lv_obj_t *lbl_a_sub = lv_label_create(box_a);
    lv_label_set_text(lbl_a_sub, "a-axis (major)");
    lv_obj_set_style_text_font(lbl_a_sub, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_a_sub, lv_color_hex(0x475569), 0);
    lv_obj_align(lbl_a_sub, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_t *lbl_a_val = lv_label_create(box_a);
    lv_label_set_text(lbl_a_val, "1.500 V");
    lv_obj_set_style_text_font(lbl_a_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_a_val, lv_color_hex(0x94a3b8), 0);
    lv_obj_align(lbl_a_val, LV_ALIGN_BOTTOM_MID, 0, -4);

    // b box (170, 126, 148x44)
    lv_obj_t *box_b = lv_obj_create(ctrl);
    lv_obj_set_pos(box_b, 170, 126); lv_obj_set_size(box_b, 148, 44);
    lv_obj_set_style_bg_color(box_b, COL_BG, 0);
    lv_obj_set_style_bg_opa(box_b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box_b, 6, 0);
    bare_obj(box_b);
    lv_obj_t *lbl_b_sub = lv_label_create(box_b);
    lv_label_set_text(lbl_b_sub, "b-axis (minor)");
    lv_obj_set_style_text_font(lbl_b_sub, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_b_sub, lv_color_hex(0x475569), 0);
    lv_obj_align(lbl_b_sub, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_t *lbl_b_val = lv_label_create(box_b);
    lv_label_set_text(lbl_b_val, "1.125 V");
    lv_obj_set_style_text_font(lbl_b_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_b_val, lv_color_hex(0x94a3b8), 0);
    lv_obj_align(lbl_b_val, LV_ALIGN_BOTTOM_MID, 0, -4);

    // Divider 2 at y=180
    {
        lv_obj_t *d = lv_obj_create(ctrl);
        lv_obj_set_pos(d, 16, 180); lv_obj_set_size(d, 312, 1);
        lv_obj_set_style_bg_color(d, COL_BTN_GRAY, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        bare_obj(d);
    }

    // Scale factor k section label at (16, 198)
    lv_obj_t *lbl_k_hdr = lv_label_create(ctrl);
    lv_label_set_text(lbl_k_hdr, "Scale factor  k");
    lv_obj_set_pos(lbl_k_hdr, 16, 198);
    lv_obj_set_style_text_font(lbl_k_hdr, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_k_hdr, lv_color_hex(0x64748b), 0);

    // k value container (16, 206, 312x56)
    lv_obj_t *k_box = lv_obj_create(ctrl);
    lv_obj_set_pos(k_box, 16, 206); lv_obj_set_size(k_box, 312, 56);
    lv_obj_set_style_bg_color(k_box, COL_BG, 0);
    lv_obj_set_style_bg_opa(k_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(k_box, 6, 0);
    bare_obj(k_box);

    lv_obj_t *btn_k_minus = lv_btn_create(k_box);
    lv_obj_set_pos(btn_k_minus, 8, 8); lv_obj_set_size(btn_k_minus, 40, 40);
    lv_obj_set_style_bg_color(btn_k_minus, lv_color_hex(0x1e3a5f), 0);
    lv_obj_set_style_border_color(btn_k_minus, lv_color_hex(0x1d4ed8), 0);
    lv_obj_set_style_border_width(btn_k_minus, 1, 0);
    lv_obj_set_style_radius(btn_k_minus, 6, 0);
    lv_obj_set_style_pad_all(btn_k_minus, 0, 0);
    lv_obj_t *lbl_k_minus = lv_label_create(btn_k_minus);
    lv_label_set_text(lbl_k_minus, "-");
    lv_obj_set_style_text_font(lbl_k_minus, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_k_minus, lv_color_hex(0x93c5fd), 0);
    lv_obj_center(lbl_k_minus);

    lv_obj_t *lbl_k_val = lv_label_create(k_box);
    lv_label_set_text(lbl_k_val, "1.20");
    lv_obj_set_style_text_font(lbl_k_val, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(lbl_k_val, COL_BLUE, 0);
    lv_obj_align(lbl_k_val, LV_ALIGN_CENTER, 0, -4);

    lv_obj_t *lbl_k_sub = lv_label_create(k_box);
    lv_label_set_text(lbl_k_sub, "x fit ellipse");
    lv_obj_set_style_text_font(lbl_k_sub, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_k_sub, lv_color_hex(0x475569), 0);
    lv_obj_align(lbl_k_sub, LV_ALIGN_BOTTOM_MID, 0, -2);

    lv_obj_t *btn_k_plus = lv_btn_create(k_box);
    lv_obj_set_pos(btn_k_plus, 264, 8); lv_obj_set_size(btn_k_plus, 40, 40);
    lv_obj_set_style_bg_color(btn_k_plus, lv_color_hex(0x1e3a5f), 0);
    lv_obj_set_style_border_color(btn_k_plus, lv_color_hex(0x1d4ed8), 0);
    lv_obj_set_style_border_width(btn_k_plus, 1, 0);
    lv_obj_set_style_radius(btn_k_plus, 6, 0);
    lv_obj_set_style_pad_all(btn_k_plus, 0, 0);
    lv_obj_t *lbl_k_plus = lv_label_create(btn_k_plus);
    lv_label_set_text(lbl_k_plus, "+");
    lv_obj_set_style_text_font(lbl_k_plus, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_k_plus, lv_color_hex(0x93c5fd), 0);
    lv_obj_center(lbl_k_plus);

    // k slider track at (16, 274, 312x6)
    lv_obj_t *sl_track = lv_obj_create(ctrl);
    lv_obj_set_pos(sl_track, 16, 274); lv_obj_set_size(sl_track, 312, 6);
    lv_obj_set_style_bg_color(sl_track, COL_BG, 0);
    lv_obj_set_style_bg_opa(sl_track, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sl_track, 3, 0);
    bare_obj(sl_track);

    // k filled portion: k=1.20 → (1.20-0.5)/(3.0-0.5)*312 = 87px
    lv_obj_t *sl_fill = lv_obj_create(ctrl);
    lv_obj_set_pos(sl_fill, 16, 274); lv_obj_set_size(sl_fill, 87, 6);
    lv_obj_set_style_bg_color(sl_fill, COL_BLUE, 0);
    lv_obj_set_style_bg_opa(sl_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sl_fill, 3, 0);
    bare_obj(sl_fill);

    // Slider knob: center at (16+87=103, 277), r=9
    lv_obj_t *sl_knob = lv_obj_create(ctrl);
    lv_obj_set_pos(sl_knob, 94, 268); lv_obj_set_size(sl_knob, 18, 18);
    lv_obj_set_style_bg_color(sl_knob, COL_BLUE, 0);
    lv_obj_set_style_bg_opa(sl_knob, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sl_knob, LV_RADIUS_CIRCLE, 0);
    bare_obj(sl_knob);

    // Slider min/max labels at y=284
    lv_obj_t *lbl_k_min = lv_label_create(ctrl);
    lv_label_set_text(lbl_k_min, "0.5");
    lv_obj_set_pos(lbl_k_min, 16, 284);
    lv_obj_set_style_text_font(lbl_k_min, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_k_min, lv_color_hex(0x475569), 0);

    lv_obj_t *lbl_k_max = lv_label_create(ctrl);
    lv_label_set_text(lbl_k_max, "3.0");
    lv_obj_set_pos(lbl_k_max, 312, 284);
    lv_obj_set_style_text_font(lbl_k_max, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_k_max, lv_color_hex(0x475569), 0);

    // Step selector label at (16, 314)
    lv_obj_t *lbl_step_hdr = lv_label_create(ctrl);
    lv_label_set_text(lbl_step_hdr, "Step");
    lv_obj_set_pos(lbl_step_hdr, 16, 314);
    lv_obj_set_style_text_font(lbl_step_hdr, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_step_hdr, lv_color_hex(0x64748b), 0);

    // Step buttons at y=320: 0.01 / 0.05(selected) / 0.10
    static const char *const STEP_VALS[] = { "0.01", "0.05", "0.10" };
    static const bool STEP_SEL[] = { false, true, false };
    static const int32_t STEP_X[] = { 16, 69, 122 };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *sb = lv_obj_create(ctrl);
        lv_obj_set_pos(sb, STEP_X[i], 320); lv_obj_set_size(sb, 48, 22);
        lv_obj_set_style_bg_color(sb, STEP_SEL[i] ? lv_color_hex(0x1e3a5f) : COL_BG, 0);
        lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(sb, 4, 0);
        lv_obj_set_style_border_color(sb, STEP_SEL[i] ? lv_color_hex(0x1d4ed8) : COL_BTN_GRAY, 0);
        lv_obj_set_style_border_width(sb, 1, 0);
        lv_obj_set_style_pad_all(sb, 0, 0);
        lv_obj_remove_flag(sb, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *lbl_s = lv_label_create(sb);
        lv_label_set_text(lbl_s, STEP_VALS[i]);
        lv_obj_set_style_text_font(lbl_s, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lbl_s,
            STEP_SEL[i] ? lv_color_hex(0x93c5fd) : lv_color_hex(0x94a3b8), 0);
        lv_obj_center(lbl_s);
    }

    // Divider 3 at y=352
    {
        lv_obj_t *d = lv_obj_create(ctrl);
        lv_obj_set_pos(d, 16, 352); lv_obj_set_size(d, 312, 1);
        lv_obj_set_style_bg_color(d, COL_BTN_GRAY, 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        bare_obj(d);
    }

    // Applied threshold label at (16, 368)
    lv_obj_t *lbl_app_hdr = lv_label_create(ctrl);
    lv_label_set_text(lbl_app_hdr, "Applied threshold");
    lv_obj_set_pos(lbl_app_hdr, 16, 368);
    lv_obj_set_style_text_font(lbl_app_hdr, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_app_hdr, lv_color_hex(0x64748b), 0);

    // a applied value (left half, center-text)
    lv_obj_t *lbl_a_app = lv_label_create(ctrl);
    lv_label_set_text(lbl_a_app, "a = 1.800 V");
    lv_obj_set_pos(lbl_a_app, 16, 384);
    lv_obj_set_size(lbl_a_app, 148, 20);
    lv_obj_set_style_text_font(lbl_a_app, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_a_app, COL_BLUE, 0);
    lv_obj_set_style_text_align(lbl_a_app, LV_TEXT_ALIGN_CENTER, 0);

    // b applied value (right half, center-text)
    lv_obj_t *lbl_b_app = lv_label_create(ctrl);
    lv_label_set_text(lbl_b_app, "b = 1.350 V");
    lv_obj_set_pos(lbl_b_app, 170, 384);
    lv_obj_set_size(lbl_b_app, 148, 20);
    lv_obj_set_style_text_font(lbl_b_app, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_b_app, COL_BLUE, 0);
    lv_obj_set_style_text_align(lbl_b_app, LV_TEXT_ALIGN_CENTER, 0);

    // ── BOTTOM BUTTONS  y=388 h=30 ────────────────────────────
    // Cancel (8, 388, 148x30)
    lv_obj_t *btn_cancel = lv_btn_create(c);
    lv_obj_set_size(btn_cancel, 148, 30);
    lv_obj_set_pos(btn_cancel, 8, 388);
    lv_obj_set_style_bg_color(btn_cancel, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_cancel, 6, 0);
    lv_obj_set_style_border_color(btn_cancel, COL_BTN_GRAY, 0);
    lv_obj_set_style_border_width(btn_cancel, 1, 0);
    lv_obj_set_style_pad_all(btn_cancel, 0, 0);
    lv_obj_add_event_cb(btn_cancel, btn_back_to_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_color(lbl_cancel, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_cancel);

    // Copy to all Ch (166, 388, 126x30)
    lv_obj_t *btn_copy = lv_btn_create(c);
    lv_obj_set_size(btn_copy, 126, 30);
    lv_obj_set_pos(btn_copy, 166, 388);
    lv_obj_set_style_bg_color(btn_copy, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(btn_copy, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_copy, 6, 0);
    lv_obj_set_style_border_color(btn_copy, COL_BTN_GRAY, 0);
    lv_obj_set_style_border_width(btn_copy, 1, 0);
    lv_obj_set_style_pad_all(btn_copy, 0, 0);
    lv_obj_t *lbl_copy = lv_label_create(btn_copy);
    lv_label_set_text(lbl_copy, "Copy to all Ch");
    lv_obj_set_style_text_color(lbl_copy, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_text_font(lbl_copy, &lv_font_montserrat_10, 0);
    lv_obj_center(lbl_copy);

    // Apply (644, 388, 148x30) — green
    lv_obj_t *btn_apply = lv_btn_create(c);
    lv_obj_set_size(btn_apply, 148, 30);
    lv_obj_set_pos(btn_apply, 644, 388);
    lv_obj_set_style_bg_color(btn_apply, lv_color_hex(0x0f6e56), 0);
    lv_obj_set_style_bg_opa(btn_apply, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_apply, 6, 0);
    lv_obj_set_style_border_width(btn_apply, 0, 0);
    lv_obj_set_style_pad_all(btn_apply, 0, 0);
    lv_obj_t *lbl_apply = lv_label_create(btn_apply);
    lv_label_set_text(lbl_apply, "Apply");
    lv_obj_set_style_text_color(lbl_apply, COL_WHITE, 0);
    lv_obj_set_style_text_font(lbl_apply, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_apply);

    // One-shot timer to draw canvas after init completes
    lv_timer_t *plot_timer = lv_timer_create(draw_threshold_plot, 50, NULL);
    lv_timer_set_repeat_count(plot_timer, 1);
}

// ============================================================
//  scr_history  XY plot canvas draw (one-shot timer)
// ============================================================
static void draw_history_plot(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!history_plot_canvas) return;

    lv_layer_t layer;
    lv_canvas_init_layer(history_plot_canvas, &layer);

    /* ── 1. Background ──────────────────────────────────────── */
    {
        lv_draw_rect_dsc_t r;
        lv_draw_rect_dsc_init(&r);
        r.bg_color     = lv_color_hex(0x111827);
        r.bg_opa       = LV_OPA_COVER;
        r.radius       = 0;
        r.border_width = 0;
        lv_area_t a    = {0, 0, 399, 289};
        lv_draw_rect(&layer, &r, &a);
    }

    /* ── 2. Grid lines ──────────────────────────────────────── */
    /* canvas coords: SVG_x - 40, SVG_y - 118
       horizontal grid: SVG y=191→73, 263→145, 335→217
       vertical grid:   SVG x=140→100, 240→200, 340→300       */
    {
        lv_draw_line_dsc_t g;
        lv_draw_line_dsc_init(&g);
        g.color = lv_color_hex(0x1e293b);
        g.width = 1;
        g.opa   = LV_OPA_COVER;
        static const int32_t HY[] = {73, 145, 217};
        for (int i = 0; i < 3; i++) {
            g.p1.x = 0;   g.p1.y = HY[i];
            g.p2.x = 399; g.p2.y = HY[i];
            lv_draw_line(&layer, &g);
        }
        static const int32_t VX[] = {100, 200, 300};
        for (int i = 0; i < 3; i++) {
            g.p1.x = VX[i]; g.p1.y = 0;
            g.p2.x = VX[i]; g.p2.y = 289;
            lv_draw_line(&layer, &g);
        }
    }

    /* ── 3. Axis lines ──────────────────────────────────────── */
    {
        lv_draw_line_dsc_t ax;
        lv_draw_line_dsc_init(&ax);
        ax.color = lv_color_hex(0x2d3748);
        ax.width = 1;
        ax.opa   = LV_OPA_COVER;
        ax.p1.x = 0;   ax.p1.y = 145; ax.p2.x = 399; ax.p2.y = 145;
        lv_draw_line(&layer, &ax);
        ax.p1.x = 200; ax.p1.y = 0;   ax.p2.x = 200; ax.p2.y = 289;
        lv_draw_line(&layer, &ax);
    }

    /* ── 4. Axis labels ─────────────────────────────────────── */
    {
        lv_draw_label_dsc_t lbl;
        lv_draw_label_dsc_init(&lbl);
        lbl.font = &lv_font_montserrat_10;
        lbl.opa  = LV_OPA_COVER;

        lbl.color = lv_color_hex(0x475569);
        lbl.text  = "X";
        lv_area_t ax1 = {385, 148, 399, 161};
        lv_draw_label(&layer, &lbl, &ax1);
        lbl.text  = "Y";
        lv_area_t ax2 = {202, 1, 215, 14};
        lv_draw_label(&layer, &lbl, &ax2);

        lbl.color = lv_color_hex(0x374151);
        lbl.text  = "+2";
        lv_area_t y1 = {1, 66, 20, 79};
        lv_draw_label(&layer, &lbl, &y1);
        lbl.text  = "-2";
        lv_area_t y2 = {1, 210, 20, 223};
        lv_draw_label(&layer, &lbl, &y2);
        lbl.text  = "-2";
        lv_area_t x1 = {90, 150, 110, 163};
        lv_draw_label(&layer, &lbl, &x1);
        lbl.text  = "+2";
        lv_area_t x2 = {290, 150, 310, 163};
        lv_draw_label(&layer, &lbl, &x2);
    }

    /* ── 5. Threshold ellipse (dashed, blue, θ=−25°) ────────── */
    /* cx=200, cy=145, rx=133, ry=100 in canvas coords          */
    {
        lv_draw_line_dsc_t el;
        lv_draw_line_dsc_init(&el);
        el.color      = COL_BLUE;
        el.width      = 2;
        el.opa        = (lv_opa_t)153; /* 0.6×255 */
        el.dash_width = 6;
        el.dash_gap   = 3;
        const double cx = 200.0, cy = 145.0;
        const double rx = 133.0, ry = 100.0;
        const double TH  = -25.0 * 3.14159265358979 / 180.0;
        const double CTH = cos(TH), STH = sin(TH);
        const double STEP = 3.14159265358979 / 30.0;
        for (int i = 0; i < 60; i++) {
            double t0 = i * STEP, t1 = (i + 1) * STEP;
            el.p1.x = (int32_t)(cx + rx * cos(t0) * CTH - ry * sin(t0) * STH);
            el.p1.y = (int32_t)(cy + rx * cos(t0) * STH + ry * sin(t0) * CTH);
            el.p2.x = (int32_t)(cx + rx * cos(t1) * CTH - ry * sin(t1) * STH);
            el.p2.y = (int32_t)(cy + rx * cos(t1) * STH + ry * sin(t1) * CTH);
            lv_draw_line(&layer, &el);
        }
    }

    /* ── 6. OK dots (20 green pts) ─────────────────────────── */
    {
        static const int32_t OKX[] = {228,215,234,221,230,218,238,224,232,210,
                                       225,220,236,214,229,241,206,233,218,244};
        static const int32_t OKY[] = {115,123,108,120,126,111,118,130,103,127,
                                       117,134,112,119,107,124,133,138,102,116};
        lv_draw_rect_dsc_t dot;
        lv_draw_rect_dsc_init(&dot);
        dot.bg_color     = lv_color_hex(0x22c55e);
        dot.bg_opa       = (lv_opa_t)179; /* 0.7×255 */
        dot.radius       = LV_RADIUS_CIRCLE;
        dot.border_width = 0;
        for (int i = 0; i < 20; i++) {
            lv_area_t a = {OKX[i]-3, OKY[i]-3, OKX[i]+3, OKY[i]+3};
            lv_draw_rect(&layer, &dot, &a);
        }
    }

    /* ── 7. NG dots (6 red pts) ─────────────────────────────── */
    {
        static const int32_t NGX[] = {302, 316, 290,  96, 108,  84};
        static const int32_t NGY[] = { 64,  52,  56, 228, 220, 234};
        lv_draw_rect_dsc_t dot;
        lv_draw_rect_dsc_init(&dot);
        dot.bg_color     = lv_color_hex(0xef4444);
        dot.bg_opa       = (lv_opa_t)230; /* 0.9×255 */
        dot.radius       = LV_RADIUS_CIRCLE;
        dot.border_width = 0;
        for (int i = 0; i < 6; i++) {
            lv_area_t a = {NGX[i]-4, NGY[i]-4, NGX[i]+4, NGY[i]+4};
            lv_draw_rect(&layer, &dot, &a);
        }
    }

    /* ── 8. Highlighted point ring (amber) at canvas(302,64) ── */
    {
        lv_draw_rect_dsc_t ring;
        lv_draw_rect_dsc_init(&ring);
        ring.radius       = LV_RADIUS_CIRCLE;
        ring.bg_opa       = LV_OPA_TRANSP;
        ring.border_color = lv_color_hex(0xfbbf24);
        ring.border_width = 2;
        ring.border_opa   = LV_OPA_COVER;
        lv_area_t a = {295, 57, 309, 71};
        lv_draw_rect(&layer, &ring, &a);
    }

    /* ── 9. Tooltip for highlighted point ───────────────────── */
    {
        lv_draw_rect_dsc_t tip;
        lv_draw_rect_dsc_init(&tip);
        tip.bg_color     = lv_color_hex(0x422006);
        tip.bg_opa       = LV_OPA_COVER;
        tip.border_color = lv_color_hex(0xf59e0b);
        tip.border_width = 1;
        tip.border_opa   = LV_OPA_COVER;
        tip.radius       = 3;
        lv_area_t ta = {278, 28, 378, 55};
        lv_draw_rect(&layer, &tip, &ta);

        lv_draw_label_dsc_t lbl;
        lv_draw_label_dsc_init(&lbl);
        lbl.font  = &lv_font_montserrat_10;
        lbl.opa   = LV_OPA_COVER;
        lbl.color = lv_color_hex(0xfbbf24);
        lbl.text  = "2026-04-18  09:14";
        lv_area_t tl1 = {281, 31, 376, 41};
        lv_draw_label(&layer, &lbl, &tl1);
        lbl.color = lv_color_hex(0xfca5a5);
        lbl.text  = "X+2.89  Y-2.34  NG";
        lv_area_t tl2 = {281, 42, 376, 54};
        lv_draw_label(&layer, &lbl, &tl2);
    }

    lv_canvas_finish_layer(history_plot_canvas, &layer);
}

// ============================================================
//  scr_history  履歴画面 (Filter bar + XY plot + Log list)
// ============================================================
static void create_scr_history(void)
{
    lv_obj_t *c = make_settings_subscr(&scr_history, "History");
    /* content_y = SVG_y - 44 */

    /* ── Filter bar (content y=0..44) ────────────────────────── */

    /* "From" label */
    {
        lv_obj_t *lbl = lv_label_create(c);
        lv_label_set_text(lbl, "From");
        lv_obj_set_pos(lbl, 16, 5);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    }
    /* Date from field content(16,22,110,20) */
    {
        lv_obj_t *box = lv_obj_create(c);
        lv_obj_set_pos(box, 16, 22);
        lv_obj_set_size(box, 110, 20);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(box, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_radius(box, 4, 0);
        lv_obj_set_style_pad_all(box, 0, 0);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *txt = lv_label_create(box);
        lv_label_set_text(txt, "2026-04-01");
        lv_obj_center(txt);
        lv_obj_set_style_text_color(txt, lv_color_hex(0x94a3b8), 0);
        lv_obj_set_style_text_font(txt, &lv_font_montserrat_10, 0);
    }

    /* "~" separator */
    {
        lv_obj_t *sep = lv_label_create(c);
        lv_label_set_text(sep, "~");
        lv_obj_set_pos(sep, 130, 27);
        lv_obj_set_style_text_color(sep, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(sep, &lv_font_montserrat_10, 0);
    }

    /* "To" label */
    {
        lv_obj_t *lbl = lv_label_create(c);
        lv_label_set_text(lbl, "To");
        lv_obj_set_pos(lbl, 144, 5);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    }
    /* Date to field content(144,22,110,20) */
    {
        lv_obj_t *box = lv_obj_create(c);
        lv_obj_set_pos(box, 144, 22);
        lv_obj_set_size(box, 110, 20);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(box, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_radius(box, 4, 0);
        lv_obj_set_style_pad_all(box, 0, 0);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *txt = lv_label_create(box);
        lv_label_set_text(txt, "2026-04-20");
        lv_obj_center(txt);
        lv_obj_set_style_text_color(txt, lv_color_hex(0x94a3b8), 0);
        lv_obj_set_style_text_font(txt, &lv_font_montserrat_10, 0);
    }

    /* Quick range buttons content(264/313/362, 22, 44, 20) */
    {
        static const char *const RNG_LBL[] = {"Today", "7 days", "30 days"};
        static const int32_t RNG_X[] = {264, 313, 362};
        static const bool    RNG_SEL[] = {false, false, true};
        for (int i = 0; i < 3; i++) {
            lv_obj_t *btn = lv_obj_create(c);
            lv_obj_set_pos(btn, RNG_X[i], 22);
            lv_obj_set_size(btn, 44, 20);
            lv_obj_set_style_bg_color(btn, RNG_SEL[i] ? lv_color_hex(0x1e3a5f)
                                                       : lv_color_hex(0x1e293b), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(btn, RNG_SEL[i] ? lv_color_hex(0x1d4ed8)
                                                           : lv_color_hex(0x334155), 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_radius(btn, 4, 0);
            lv_obj_set_style_pad_all(btn, 0, 0);
            lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, RNG_LBL[i]);
            lv_obj_center(lbl);
            lv_obj_set_style_text_color(lbl, RNG_SEL[i] ? lv_color_hex(0x93c5fd)
                                                         : lv_color_hex(0x64748b), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
        }
    }

    /* Ch filter label */
    {
        lv_obj_t *lbl = lv_label_create(c);
        lv_label_set_text(lbl, "Ch filter");
        lv_obj_set_pos(lbl, 422, 5);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    }
    /* Ch filter buttons content(422/471/520/569, 22, 44, 20) */
    {
        static const char *const CH_LBL[] = {"Ch 1", "Ch 2", "Ch 3", "All"};
        static const int32_t CH_X[] = {422, 471, 520, 569};
        static const bool    CH_SEL[] = {true, false, false, false};
        for (int i = 0; i < 4; i++) {
            lv_obj_t *btn = lv_obj_create(c);
            lv_obj_set_pos(btn, CH_X[i], 22);
            lv_obj_set_size(btn, 44, 20);
            lv_obj_set_style_bg_color(btn, CH_SEL[i] ? lv_color_hex(0x1e3a5f)
                                                      : lv_color_hex(0x1e293b), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(btn, CH_SEL[i] ? COL_BLUE
                                                          : lv_color_hex(0x334155), 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_radius(btn, 4, 0);
            lv_obj_set_style_pad_all(btn, 0, 0);
            lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, CH_LBL[i]);
            lv_obj_center(lbl);
            lv_obj_set_style_text_color(lbl, CH_SEL[i] ? lv_color_hex(0x93c5fd)
                                                        : lv_color_hex(0x475569), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
        }
    }

    /* Count label content(628,5) */
    {
        lv_obj_t *lbl = lv_label_create(c);
        lv_label_set_text(lbl, "248 pts");
        lv_obj_set_pos(lbl, 628, 5);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    }

    /* CSV Export button content(628,22,74,20) */
    {
        lv_obj_t *btn = lv_obj_create(c);
        lv_obj_set_pos(btn, 628, 22);
        lv_obj_set_size(btn, 74, 20);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, "CSV Export");
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x64748b), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    }

    /* Filter divider at content y=43 */
    {
        lv_obj_t *div = lv_obj_create(c);
        lv_obj_set_pos(div, 0, 43);
        lv_obj_set_size(div, 800, 1);
        lv_obj_set_style_bg_color(div, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(div, 0, 0);
        lv_obj_set_style_radius(div, 0, 0);
        lv_obj_set_style_pad_all(div, 0, 0);
        lv_obj_remove_flag(div, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* ── XY Plot panel content(8,52,452,340) ─────────────────── */
    lv_obj_t *xy_panel = lv_obj_create(c);
    lv_obj_set_pos(xy_panel, 8, 52);
    lv_obj_set_size(xy_panel, 452, 340);
    lv_obj_set_style_bg_color(xy_panel, COL_BG, 0);
    lv_obj_set_style_bg_opa(xy_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(xy_panel, COL_SURFACE, 0);
    lv_obj_set_style_border_width(xy_panel, 1, 0);
    lv_obj_set_style_radius(xy_panel, 8, 0);
    lv_obj_set_style_pad_all(xy_panel, 0, 0);
    lv_obj_remove_flag(xy_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Plot title */
    {
        lv_obj_t *ttl = lv_label_create(xy_panel);
        lv_label_set_text(ttl, "X-Y Plot  Ch 1  /  30 days  (248)");
        lv_obj_set_pos(ttl, 12, 8);
        lv_obj_set_width(ttl, 428);
        lv_obj_set_style_text_align(ttl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(ttl, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(ttl, &lv_font_montserrat_10, 0);
    }

    /* Canvas 400×290 at panel(32,22) */
    history_plot_canvas = lv_canvas_create(xy_panel);
    lv_canvas_set_buffer(history_plot_canvas,
                         history_plot_buf, 400, 290, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(history_plot_canvas, 32, 22);

    /* Legend dots below canvas (panel y≈308..328) */
    {
        lv_obj_t *ok_dot = lv_obj_create(xy_panel);
        lv_obj_set_pos(ok_dot, 14, 308);
        lv_obj_set_size(ok_dot, 8, 8);
        lv_obj_set_style_bg_color(ok_dot, lv_color_hex(0x22c55e), 0);
        lv_obj_set_style_bg_opa(ok_dot, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ok_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ok_dot, 0, 0);
        lv_obj_set_style_pad_all(ok_dot, 0, 0);
        lv_obj_remove_flag(ok_dot, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *ok_lbl = lv_label_create(xy_panel);
        lv_label_set_text(ok_lbl, "OK (242)");
        lv_obj_set_pos(ok_lbl, 25, 305);
        lv_obj_set_style_text_color(ok_lbl, lv_color_hex(0x86efac), 0);
        lv_obj_set_style_text_font(ok_lbl, &lv_font_montserrat_10, 0);

        lv_obj_t *ng_dot = lv_obj_create(xy_panel);
        lv_obj_set_pos(ng_dot, 14, 320);
        lv_obj_set_size(ng_dot, 8, 8);
        lv_obj_set_style_bg_color(ng_dot, lv_color_hex(0xef4444), 0);
        lv_obj_set_style_bg_opa(ng_dot, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ng_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(ng_dot, 0, 0);
        lv_obj_set_style_pad_all(ng_dot, 0, 0);
        lv_obj_remove_flag(ng_dot, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *ng_lbl = lv_label_create(xy_panel);
        lv_label_set_text(ng_lbl, "NG (6)");
        lv_obj_set_pos(ng_lbl, 25, 317);
        lv_obj_set_style_text_color(ng_lbl, lv_color_hex(0xfca5a5), 0);
        lv_obj_set_style_text_font(ng_lbl, &lv_font_montserrat_10, 0);

        lv_obj_t *ell_lbl = lv_label_create(xy_panel);
        lv_label_set_text(ell_lbl, "--- Ellipse");
        lv_obj_set_pos(ell_lbl, 100, 311);
        lv_obj_set_style_text_color(ell_lbl, lv_color_hex(0x7dd3fc), 0);
        lv_obj_set_style_text_font(ell_lbl, &lv_font_montserrat_10, 0);
    }

    /* ── Right log panel content(468,52,324,340) ─────────────── */
    lv_obj_t *log_panel = lv_obj_create(c);
    lv_obj_set_pos(log_panel, 468, 52);
    lv_obj_set_size(log_panel, 324, 340);
    lv_obj_set_style_bg_color(log_panel, COL_BG, 0);
    lv_obj_set_style_bg_opa(log_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(log_panel, COL_SURFACE, 0);
    lv_obj_set_style_border_width(log_panel, 1, 0);
    lv_obj_set_style_radius(log_panel, 8, 0);
    lv_obj_set_style_pad_all(log_panel, 0, 0);
    lv_obj_remove_flag(log_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* Panel title at panel(12,6) */
    {
        lv_obj_t *ttl = lv_label_create(log_panel);
        lv_label_set_text(ttl, "Log  (newest first)");
        lv_obj_set_pos(ttl, 12, 6);
        lv_obj_set_style_text_color(ttl, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(ttl, &lv_font_montserrat_10, 0);
    }

    /* Header row at panel(0,22,324,22) */
    {
        lv_obj_t *hdr = lv_obj_create(log_panel);
        lv_obj_set_pos(hdr, 0, 22);
        lv_obj_set_size(hdr, 324, 22);
        lv_obj_set_style_bg_color(hdr, COL_BG, 0);
        lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(hdr, 0, 0);
        lv_obj_set_style_border_width(hdr, 0, 0);
        lv_obj_set_style_pad_all(hdr, 0, 0);
        lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
        /* col centers (panel-inner x): date=38, X=126, Y=182, dist=238, judge=294 */
        static const char *const HDR_LBL[] = {"Date/Time", "X [V]", "Y [V]", "Dist.", "Judge"};
        static const int32_t HDR_CX[] = {38, 126, 182, 238, 294};
        for (int i = 0; i < 5; i++) {
            lv_obj_t *lbl = lv_label_create(hdr);
            lv_label_set_text(lbl, HDR_LBL[i]);
            lv_obj_set_pos(lbl, HDR_CX[i] - 22, 5);
            lv_obj_set_width(lbl, 44);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0x475569), 0);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
        }
    }

    /* 7 data rows */
    {
        struct RowData {
            int32_t     panel_y;
            bool        highlight;
            uint32_t    bg;
            uint32_t    bar_col;
            const char *date1;
            const char *date2;
            const char *xv;
            const char *yv;
            const char *dist;
            uint32_t    badge_bg;
            const char *judge;
            uint32_t    val_col;
            uint32_t    badge_txt;
        };
        static const RowData ROWS[] = {
            {44,  true,  0x2a1a0a, 0xfbbf24, "04-18","09:14:22","+2.891","-2.340","1.25", 0x7f1d1d,"NG",0xfca5a5,0xfca5a5},
            {76,  false, 0x111827, 0x1e293b,  "04-18","09:13:05","+1.243","-0.872","0.58", 0x14532d,"OK",0x6ee7b7,0x4ade80},
            {108, false, 0x0f172a, 0x1e293b,  "04-18","09:11:48","+1.198","-0.791","0.56", 0x14532d,"OK",0x6ee7b7,0x4ade80},
            {140, false, 0x111827, 0x1e293b,  "04-17","14:52:11","+2.654","-2.180","1.18", 0x7f1d1d,"NG",0xfca5a5,0xfca5a5},
            {172, false, 0x0f172a, 0x1e293b,  "04-17","14:50:33","+1.054","-0.912","0.57", 0x14532d,"OK",0x6ee7b7,0x4ade80},
            {204, false, 0x111827, 0x1e293b,  "04-17","11:23:07","+1.187","-0.843","0.60", 0x14532d,"OK",0x6ee7b7,0x4ade80},
            {236, false, 0x0f172a, 0x1e293b,  "04-16","16:08:44","+1.302","-0.804","0.62", 0x14532d,"OK",0x6ee7b7,0x4ade80},
        };
        /* value col centers (row-local x): X=126, Y=182, Dist=238 */
        static const int32_t VCOL_CX[] = {126, 182, 238};

        for (int i = 0; i < 7; i++) {
            const RowData &rd = ROWS[i];
            lv_obj_t *row = lv_obj_create(log_panel);
            lv_obj_set_pos(row, 0, rd.panel_y);
            lv_obj_set_size(row, 324, 32);
            lv_obj_set_style_bg_color(row, lv_color_hex(rd.bg), 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(row, 0, 0);
            lv_obj_set_style_pad_all(row, 0, 0);
            lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
            if (rd.highlight) {
                lv_obj_set_style_border_color(row, lv_color_hex(0xf59e0b), 0);
                lv_obj_set_style_border_width(row, 1, 0);
            } else {
                lv_obj_set_style_border_width(row, 0, 0);
            }

            /* Accent bar 3px at left */
            lv_obj_t *bar = lv_obj_create(row);
            lv_obj_set_pos(bar, 0, 0);
            lv_obj_set_size(bar, 3, 32);
            lv_obj_set_style_bg_color(bar, lv_color_hex(rd.bar_col), 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(bar, 0, 0);
            lv_obj_set_style_border_width(bar, 0, 0);
            lv_obj_set_style_pad_all(bar, 0, 0);
            lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

            /* Date line 1 */
            lv_obj_t *d1 = lv_label_create(row);
            lv_label_set_text(d1, rd.date1);
            lv_obj_set_pos(d1, 8, 5);
            lv_obj_set_style_text_color(d1, rd.highlight ? lv_color_hex(0xfbbf24)
                                                          : lv_color_hex(0x475569), 0);
            lv_obj_set_style_text_font(d1, &lv_font_montserrat_10, 0);

            /* Date line 2 */
            lv_obj_t *d2 = lv_label_create(row);
            lv_label_set_text(d2, rd.date2);
            lv_obj_set_pos(d2, 8, 18);
            lv_obj_set_style_text_color(d2, rd.highlight ? lv_color_hex(0xfbbf24)
                                                          : lv_color_hex(0x475569), 0);
            lv_obj_set_style_text_font(d2, &lv_font_montserrat_10, 0);

            /* X, Y, Dist value labels centered at each column */
            const char *vcols[] = {rd.xv, rd.yv, rd.dist};
            for (int j = 0; j < 3; j++) {
                lv_obj_t *vl = lv_label_create(row);
                lv_label_set_text(vl, vcols[j]);
                lv_obj_set_pos(vl, VCOL_CX[j] - 22, 10);
                lv_obj_set_width(vl, 44);
                lv_obj_set_style_text_align(vl, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_style_text_color(vl, lv_color_hex(rd.val_col), 0);
                lv_obj_set_style_text_font(vl, &lv_font_montserrat_10, 0);
            }

            /* Judge badge at row(278,7,36,18) */
            lv_obj_t *badge = lv_obj_create(row);
            lv_obj_set_pos(badge, 278, 7);
            lv_obj_set_size(badge, 36, 18);
            lv_obj_set_style_bg_color(badge, lv_color_hex(rd.badge_bg), 0);
            lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(badge, 3, 0);
            lv_obj_set_style_border_width(badge, 0, 0);
            lv_obj_set_style_pad_all(badge, 0, 0);
            lv_obj_remove_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t *jlbl = lv_label_create(badge);
            lv_label_set_text(jlbl, rd.judge);
            lv_obj_center(jlbl);
            lv_obj_set_style_text_color(jlbl, lv_color_hex(rd.badge_txt), 0);
            lv_obj_set_style_text_font(jlbl, &lv_font_montserrat_10, 0);
        }
    }

    /* Scroll hint at panel(0,268,324,32) */
    {
        lv_obj_t *hint = lv_obj_create(log_panel);
        lv_obj_set_pos(hint, 0, 268);
        lv_obj_set_size(hint, 324, 32);
        lv_obj_set_style_bg_color(hint, lv_color_hex(0x0a0f1a), 0);
        lv_obj_set_style_bg_opa(hint, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(hint, 0, 0);
        lv_obj_set_style_border_width(hint, 0, 0);
        lv_obj_set_style_pad_all(hint, 0, 0);
        lv_obj_remove_flag(hint, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *lbl = lv_label_create(hint);
        lv_label_set_text(lbl, LV_SYMBOL_DOWN "  241 more...");
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x334155), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    }

    /* Summary bar at panel(0,310,324,30) */
    {
        lv_obj_t *sumbar = lv_obj_create(log_panel);
        lv_obj_set_pos(sumbar, 0, 310);
        lv_obj_set_size(sumbar, 324, 30);
        lv_obj_set_style_bg_color(sumbar, COL_BG, 0);
        lv_obj_set_style_bg_opa(sumbar, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(sumbar, COL_SURFACE, 0);
        lv_obj_set_style_border_width(sumbar, 1, 0);
        lv_obj_set_style_radius(sumbar, 0, 0);
        lv_obj_set_style_pad_all(sumbar, 0, 0);
        lv_obj_remove_flag(sumbar, LV_OBJ_FLAG_SCROLLABLE);

        /* OK badge at sumbar(12,8,56,14) */
        lv_obj_t *ok_b = lv_obj_create(sumbar);
        lv_obj_set_pos(ok_b, 12, 8);
        lv_obj_set_size(ok_b, 56, 14);
        lv_obj_set_style_bg_color(ok_b, lv_color_hex(0x14532d), 0);
        lv_obj_set_style_bg_opa(ok_b, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ok_b, 3, 0);
        lv_obj_set_style_border_width(ok_b, 0, 0);
        lv_obj_set_style_pad_all(ok_b, 0, 0);
        lv_obj_remove_flag(ok_b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *ok_l = lv_label_create(ok_b);
        lv_label_set_text(ok_l, "OK  242");
        lv_obj_center(ok_l);
        lv_obj_set_style_text_color(ok_l, lv_color_hex(0x4ade80), 0);
        lv_obj_set_style_text_font(ok_l, &lv_font_montserrat_10, 0);

        /* NG badge at sumbar(78,8,48,14) */
        lv_obj_t *ng_b = lv_obj_create(sumbar);
        lv_obj_set_pos(ng_b, 78, 8);
        lv_obj_set_size(ng_b, 48, 14);
        lv_obj_set_style_bg_color(ng_b, lv_color_hex(0x7f1d1d), 0);
        lv_obj_set_style_bg_opa(ng_b, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ng_b, 3, 0);
        lv_obj_set_style_border_width(ng_b, 0, 0);
        lv_obj_set_style_pad_all(ng_b, 0, 0);
        lv_obj_remove_flag(ng_b, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *ng_l = lv_label_create(ng_b);
        lv_label_set_text(ng_l, "NG  6");
        lv_obj_center(ng_l);
        lv_obj_set_style_text_color(ng_l, lv_color_hex(0xfca5a5), 0);
        lv_obj_set_style_text_font(ng_l, &lv_font_montserrat_10, 0);

        /* Pass rate label at sumbar(140,10) */
        lv_obj_t *rate = lv_label_create(sumbar);
        lv_label_set_text(rate, "Pass rate  97.6%");
        lv_obj_set_pos(rate, 140, 9);
        lv_obj_set_style_text_color(rate, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(rate, &lv_font_montserrat_10, 0);
    }

    /* One-shot timer to draw XY plot canvas */
    lv_timer_t *plot_timer = lv_timer_create(draw_history_plot, 50, NULL);
    lv_timer_set_repeat_count(plot_timer, 1);
}

// ============================================================
//  scr_correction  補正値管理
// ============================================================
static void create_scr_correction(void)
{
    lv_obj_t *c = make_settings_subscr(&scr_correction, "Correction");
    /* content_y = SVG_y - 44 */

    /* ── Recommendation banner  content(8,8,784,64) ─────────── */
    lv_obj_t *banner = lv_obj_create(c);
    lv_obj_set_pos(banner, 8, 8);
    lv_obj_set_size(banner, 784, 64);
    lv_obj_set_style_bg_color(banner, lv_color_hex(0x1c1400), 0);
    lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(banner, lv_color_hex(0xf59e0b), 0);
    lv_obj_set_style_border_width(banner, 1, 0);
    lv_obj_set_style_radius(banner, 6, 0);
    lv_obj_set_style_pad_all(banner, 0, 0);
    lv_obj_remove_flag(banner, LV_OBJ_FLAG_SCROLLABLE);

    /* left amber accent bar 5px */
    lv_obj_t *banner_bar = lv_obj_create(banner);
    lv_obj_set_pos(banner_bar, 0, 0);
    lv_obj_set_size(banner_bar, 5, 64);
    lv_obj_set_style_bg_color(banner_bar, lv_color_hex(0xf59e0b), 0);
    lv_obj_set_style_bg_opa(banner_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(banner_bar, 0, 0);
    lv_obj_set_style_border_width(banner_bar, 0, 0);
    lv_obj_set_style_pad_all(banner_bar, 0, 0);
    lv_obj_remove_flag(banner_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* ML icon area (circle symbol) */
    lv_obj_t *ml_icon = lv_label_create(banner);
    lv_label_set_text(ml_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_pos(ml_icon, 18, 10);
    lv_obj_set_style_text_color(ml_icon, lv_color_hex(0xf59e0b), 0);
    lv_obj_set_style_text_font(ml_icon, &lv_font_montserrat_20, 0);

    /* Banner title */
    lv_obj_t *banner_title = lv_label_create(banner);
    lv_label_set_text(banner_title, "ML Correction Proposal");
    lv_obj_set_pos(banner_title, 44, 10);
    lv_obj_set_style_text_color(banner_title, lv_color_hex(0xfbbf24), 0);
    lv_obj_set_style_text_font(banner_title, &lv_font_montserrat_14, 0);

    /* Banner description */
    lv_obj_t *banner_desc = lv_label_create(banner);
    lv_label_set_text(banner_desc, "3 channels have updated correction proposals based on recent measurement data.");
    lv_obj_set_pos(banner_desc, 44, 30);
    lv_obj_set_style_text_color(banner_desc, lv_color_hex(0x92400e), 0);
    lv_obj_set_style_text_font(banner_desc, &lv_font_montserrat_10, 0);

    /* Approve button  content(620,18,80,30) */
    lv_obj_t *btn_approve = lv_btn_create(banner);
    lv_obj_set_pos(btn_approve, 612, 17);
    lv_obj_set_size(btn_approve, 80, 30);
    lv_obj_set_style_bg_color(btn_approve, lv_color_hex(0x166534), 0);
    lv_obj_set_style_bg_opa(btn_approve, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn_approve, lv_color_hex(0x22c55e), 0);
    lv_obj_set_style_border_width(btn_approve, 1, 0);
    lv_obj_set_style_radius(btn_approve, 4, 0);
    lv_obj_set_style_pad_all(btn_approve, 0, 0);
    lv_obj_t *lbl_approve = lv_label_create(btn_approve);
    lv_label_set_text(lbl_approve, "Approve");
    lv_obj_center(lbl_approve);
    lv_obj_set_style_text_color(lbl_approve, lv_color_hex(0x86efac), 0);
    lv_obj_set_style_text_font(lbl_approve, &lv_font_montserrat_12, 0);

    /* Reject button  content(700,18,76,30) */
    lv_obj_t *btn_reject = lv_btn_create(banner);
    lv_obj_set_pos(btn_reject, 700, 17);
    lv_obj_set_size(btn_reject, 76, 30);
    lv_obj_set_style_bg_color(btn_reject, lv_color_hex(0x1c0505), 0);
    lv_obj_set_style_bg_opa(btn_reject, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn_reject, lv_color_hex(0xef4444), 0);
    lv_obj_set_style_border_width(btn_reject, 1, 0);
    lv_obj_set_style_radius(btn_reject, 4, 0);
    lv_obj_set_style_pad_all(btn_reject, 0, 0);
    lv_obj_t *lbl_reject = lv_label_create(btn_reject);
    lv_label_set_text(lbl_reject, "Reject");
    lv_obj_center(lbl_reject);
    lv_obj_set_style_text_color(lbl_reject, lv_color_hex(0xfca5a5), 0);
    lv_obj_set_style_text_font(lbl_reject, &lv_font_montserrat_12, 0);

    /* ── Table container  content(0,80,800,356) ──────────────── */
    /* content_y=80 = SVG_y(124) - 44 */

    /* Table header row  content(0,80,800,24) */
    lv_obj_t *tbl_hdr = lv_obj_create(c);
    lv_obj_set_pos(tbl_hdr, 0, 80);
    lv_obj_set_size(tbl_hdr, 800, 24);
    lv_obj_set_style_bg_color(tbl_hdr, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_bg_opa(tbl_hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(tbl_hdr, 0, 0);
    lv_obj_set_style_border_width(tbl_hdr, 0, 0);
    lv_obj_set_style_border_side(tbl_hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(tbl_hdr, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(tbl_hdr, 1, 0);
    lv_obj_set_style_pad_all(tbl_hdr, 0, 0);
    lv_obj_remove_flag(tbl_hdr, LV_OBJ_FLAG_SCROLLABLE);

    /* Header labels at column centers (x relative to content/tbl_hdr):
       Ch=36, Enabled=96, Coeff=184, Proposed=316, Last Date=430, Reason=560, Count=716 */
    static const char *hdr_texts[] = { "Ch", "Enabled", "Current Coeff.", "Proposed", "Last Applied", "Reason", "Apply Count" };
    static const int32_t hdr_cx[]  = { 36, 96, 184, 316, 430, 560, 716 };
    for (int i = 0; i < 7; i++) {
        lv_obj_t *h = lv_label_create(tbl_hdr);
        lv_label_set_text(h, hdr_texts[i]);
        lv_obj_set_style_text_color(h, lv_color_hex(0x94a3b8), 0);
        lv_obj_set_style_text_font(h, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(h, 80);
        lv_obj_set_pos(h, hdr_cx[i] - 40, 7);
    }

    /* Row data */
    typedef struct {
        int     ch;
        bool    active;
        bool    proposed;
        bool    neg_delta;  /* true = red delta */
        const char *coeff;
        const char *prop_val;
        const char *delta;
        const char *date;
        const char *reason;
        int     count;
    } CorrRow;

    static const CorrRow rows[16] = {
        {  1, true,  false, false, "1.0240", "—",      "",         "2026-03-15", "自動学習", 4 },
        {  2, true,  false, false, "0.9870", "—",      "",         "2026-02-28", "自動学習", 3 },
        {  3, true,  true,  false, "1.1520", "1.1840", "▲+0.032", "2025-11-10", "自動学習", 7 },
        {  4, false, false, false, "—",      "—",      "",         "未設定",     "未使用Ch", 0 },
        {  5, true,  false, false, "1.0050", "—",      "",         "2026-04-01", "自動学習", 5 },
        {  6, false, false, false, "—",      "—",      "",         "未設定",     "未使用Ch", 0 },
        {  7, false, false, false, "—",      "—",      "",         "未設定",     "未使用Ch", 0 },
        {  8, true,  true,  true,  "0.9640", "0.9520", "▼-0.012", "2026-01-22", "手動入力", 2 },
        {  9, false, false, false, "—",      "—",      "",         "未設定",     "未使用Ch", 0 },
        { 10, false, false, false, "—",      "—",      "",         "未設定",     "未使用Ch", 0 },
        { 11, false, false, false, "—",      "—",      "",         "未設定",     "未使用Ch", 0 },
        { 12, true,  true,  false, "1.0780", "1.0960", "▲+0.018", "2026-02-10", "自動学習", 6 },
        { 13, false, false, false, "—",      "—",      "",         "未設定",     "未使用Ch", 0 },
        { 14, false, false, false, "—",      "—",      "",         "未設定",     "未使用Ch", 0 },
        { 15, false, false, false, "—",      "—",      "",         "未設定",     "未使用Ch", 0 },
        { 16, false, false, false, "—",      "—",      "",         "未設定",     "未使用Ch", 0 },
    };

    /* Col x positions for data cells (center) */
    static const int32_t col_cx[] = { 36, 96, 184, 316, 430, 560, 716 };

    for (int i = 0; i < 16; i++) {
        const CorrRow *r = &rows[i];
        int32_t ry = 80 + 24 + i * 20;  /* content_y for this row */
        int32_t rh = 20;

        lv_obj_t *row = lv_obj_create(c);
        lv_obj_set_pos(row, 0, ry);
        lv_obj_set_size(row, 800, rh);

        lv_color_t row_bg;
        if (r->proposed) {
            row_bg = lv_color_hex(0x1c1917);
        } else if (i % 2 == 0) {
            row_bg = lv_color_hex(0x0f172a);
        } else {
            row_bg = lv_color_hex(0x111827);
        }
        lv_obj_set_style_bg_color(row, row_bg, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        /* Amber left accent bar for proposed rows */
        if (r->proposed) {
            lv_obj_t *acc = lv_obj_create(row);
            lv_obj_set_pos(acc, 0, 0);
            lv_obj_set_size(acc, 3, rh);
            lv_obj_set_style_bg_color(acc, lv_color_hex(0xf59e0b), 0);
            lv_obj_set_style_bg_opa(acc, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(acc, 0, 0);
            lv_obj_set_style_border_width(acc, 0, 0);
            lv_obj_set_style_pad_all(acc, 0, 0);
            lv_obj_remove_flag(acc, LV_OBJ_FLAG_SCROLLABLE);
        }

        /* Col 0: Ch number */
        {
            char ch_str[4];
            lv_snprintf(ch_str, sizeof(ch_str), "%d", r->ch);
            lv_obj_t *lbl = lv_label_create(row);
            lv_label_set_text(lbl, ch_str);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
            lv_color_t ch_col = r->proposed ? lv_color_hex(0xf59e0b) :
                                (r->active  ? COL_WHITE : COL_MUTED);
            lv_obj_set_style_text_color(lbl, ch_col, 0);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(lbl, 50);
            lv_obj_set_pos(lbl, col_cx[0] - 25, 5);
        }

        /* Col 1: Status dot + Enabled/Disabled */
        {
            lv_obj_t *dot = lv_obj_create(row);
            lv_obj_set_size(dot, 6, 6);
            lv_obj_set_pos(dot, col_cx[1] - 20, 7);
            lv_obj_set_style_bg_color(dot, r->active ? lv_color_hex(0x22c55e) : lv_color_hex(0x374151), 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(dot, 0, 0);
            lv_obj_set_style_pad_all(dot, 0, 0);
            lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_t *lbl = lv_label_create(row);
            lv_label_set_text(lbl, r->active ? "ON" : "OFF");
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl, r->active ? lv_color_hex(0x22c55e) : lv_color_hex(0x374151), 0);
            lv_obj_set_pos(lbl, col_cx[1] - 10, 5);
        }

        /* Col 2: Current coeff */
        {
            lv_obj_t *lbl = lv_label_create(row);
            lv_label_set_text(lbl, r->coeff);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl, r->active ? COL_WHITE : COL_MUTED, 0);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(lbl, 80);
            lv_obj_set_pos(lbl, col_cx[2] - 40, 5);
        }

        /* Col 3: Proposed value + delta */
        {
            lv_obj_t *lbl = lv_label_create(row);
            if (r->proposed) {
                char buf[32];
                lv_snprintf(buf, sizeof(buf), "%s  %s", r->prop_val, r->delta);
                lv_label_set_text(lbl, buf);
                lv_obj_set_style_text_color(lbl, r->neg_delta ? lv_color_hex(0xef4444) : lv_color_hex(0x86efac), 0);
            } else {
                lv_label_set_text(lbl, r->prop_val);
                lv_obj_set_style_text_color(lbl, COL_MUTED, 0);
            }
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(lbl, 120);
            lv_obj_set_pos(lbl, col_cx[3] - 60, 5);
        }

        /* Col 4: Last applied date */
        {
            lv_obj_t *lbl = lv_label_create(row);
            lv_label_set_text(lbl, r->date);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl, r->active ? lv_color_hex(0x94a3b8) : COL_MUTED, 0);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(lbl, 120);
            lv_obj_set_pos(lbl, col_cx[4] - 60, 5);
        }

        /* Col 5: Reason */
        {
            lv_obj_t *lbl = lv_label_create(row);
            lv_label_set_text(lbl, r->reason);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl, r->active ? lv_color_hex(0x94a3b8) : COL_MUTED, 0);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(lbl, 160);
            lv_obj_set_pos(lbl, col_cx[5] - 80, 5);
        }

        /* Col 6: Apply count */
        {
            char cnt_str[8];
            lv_snprintf(cnt_str, sizeof(cnt_str), "%d", r->count);
            lv_obj_t *lbl = lv_label_create(row);
            lv_label_set_text(lbl, cnt_str);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl, r->active ? COL_WHITE : COL_MUTED, 0);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_width(lbl, 80);
            lv_obj_set_pos(lbl, col_cx[6] - 40, 5);
        }
    }

    /* Column separator lines */
    static const int32_t sep_xs[] = { 64, 128, 248, 376, 492, 632 };
    for (int i = 0; i < 6; i++) {
        lv_obj_t *sep = lv_obj_create(c);
        lv_obj_set_pos(sep, sep_xs[i], 80);
        lv_obj_set_size(sep, 1, 24 + 16 * 20);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(sep, 0, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_set_style_pad_all(sep, 0, 0);
        lv_obj_remove_flag(sep, LV_OBJ_FLAG_SCROLLABLE);
    }
}

// ============================================================
//  scr_detail  詳細設定 (Language / Date+Time / I/O Test)
// ============================================================
static void create_scr_detail(void)
{
    lv_obj_t *c = make_settings_subscr(&scr_detail, "Detail Settings");
    /* content_y = SVG_y - 44  /  right panel inner_x = SVG_x - 196 */

    /* ── Left nav panel  content(8,8,180,410) ───────────────── */
    lv_obj_t *nav = lv_obj_create(c);
    lv_obj_set_pos(nav, 8, 8);
    lv_obj_set_size(nav, 180, 410);
    lv_obj_set_style_bg_color(nav, COL_BG, 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(nav, COL_SURFACE, 0);
    lv_obj_set_style_border_width(nav, 1, 0);
    lv_obj_set_style_radius(nav, 6, 0);
    lv_obj_set_style_pad_all(nav, 0, 0);
    lv_obj_remove_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    /* -- Language item (active, blue)  nav(0,0,180,52) -- */
    lv_obj_t *nav_lang = lv_obj_create(nav);
    lv_obj_set_pos(nav_lang, 0, 0);
    lv_obj_set_size(nav_lang, 180, 52);
    lv_obj_set_style_bg_color(nav_lang, lv_color_hex(0x1e3a5f), 0);
    lv_obj_set_style_bg_opa(nav_lang, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(nav_lang, 0, 0);
    lv_obj_set_style_border_width(nav_lang, 0, 0);
    lv_obj_set_style_pad_all(nav_lang, 0, 0);
    lv_obj_remove_flag(nav_lang, LV_OBJ_FLAG_SCROLLABLE);

    /* 4px blue accent bar */
    lv_obj_t *lang_bar = lv_obj_create(nav_lang);
    lv_obj_set_pos(lang_bar, 0, 0);
    lv_obj_set_size(lang_bar, 4, 52);
    lv_obj_set_style_bg_color(lang_bar, COL_BLUE, 0);
    lv_obj_set_style_bg_opa(lang_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lang_bar, 0, 0);
    lv_obj_set_style_border_width(lang_bar, 0, 0);
    lv_obj_set_style_pad_all(lang_bar, 0, 0);
    lv_obj_remove_flag(lang_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Globe icon (LV_SYMBOL_AUDIO as placeholder) */
    lv_obj_t *lang_icon = lv_label_create(nav_lang);
    lv_label_set_text(lang_icon, LV_SYMBOL_AUDIO);
    lv_obj_set_pos(lang_icon, 14, 12);
    lv_obj_set_style_text_color(lang_icon, COL_BLUE, 0);
    lv_obj_set_style_text_font(lang_icon, &lv_font_montserrat_16, 0);

    lv_obj_t *lang_title = lv_label_create(nav_lang);
    lv_label_set_text(lang_title, "Language");
    lv_obj_set_pos(lang_title, 40, 10);
    lv_obj_set_style_text_color(lang_title, lv_color_hex(0x93c5fd), 0);
    lv_obj_set_style_text_font(lang_title, &lv_font_montserrat_12, 0);

    lv_obj_t *lang_sub = lv_label_create(nav_lang);
    lv_label_set_text(lang_sub, "Japanese");
    lv_obj_set_pos(lang_sub, 40, 28);
    lv_obj_set_style_text_color(lang_sub, COL_MUTED, 0);
    lv_obj_set_style_text_font(lang_sub, &lv_font_montserrat_10, 0);

    /* -- Date/Time item  nav(0,56,180,52) -- */
    lv_obj_t *nav_dt = lv_obj_create(nav);
    lv_obj_set_pos(nav_dt, 0, 56);
    lv_obj_set_size(nav_dt, 180, 52);
    lv_obj_set_style_bg_color(nav_dt, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(nav_dt, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(nav_dt, 0, 0);
    lv_obj_set_style_border_width(nav_dt, 0, 0);
    lv_obj_set_style_pad_all(nav_dt, 0, 0);
    lv_obj_remove_flag(nav_dt, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dt_icon = lv_label_create(nav_dt);
    lv_label_set_text(dt_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_pos(dt_icon, 14, 12);
    lv_obj_set_style_text_color(dt_icon, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(dt_icon, &lv_font_montserrat_16, 0);

    lv_obj_t *dt_title = lv_label_create(nav_dt);
    lv_label_set_text(dt_title, "Date/Time");
    lv_obj_set_pos(dt_title, 40, 10);
    lv_obj_set_style_text_color(dt_title, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(dt_title, &lv_font_montserrat_12, 0);

    lv_obj_t *dt_sub = lv_label_create(nav_dt);
    lv_label_set_text(dt_sub, "2026-04-20 14:32");
    lv_obj_set_pos(dt_sub, 40, 28);
    lv_obj_set_style_text_color(dt_sub, lv_color_hex(0x374151), 0);
    lv_obj_set_style_text_font(dt_sub, &lv_font_montserrat_10, 0);

    /* -- I/O Test item  nav(0,112,180,52) -- */
    lv_obj_t *nav_io = lv_obj_create(nav);
    lv_obj_set_pos(nav_io, 0, 112);
    lv_obj_set_size(nav_io, 180, 52);
    lv_obj_set_style_bg_color(nav_io, COL_BG, 0);
    lv_obj_set_style_bg_opa(nav_io, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(nav_io, 0, 0);
    lv_obj_set_style_border_width(nav_io, 0, 0);
    lv_obj_set_style_pad_all(nav_io, 0, 0);
    lv_obj_remove_flag(nav_io, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *io_icon = lv_label_create(nav_io);
    lv_label_set_text(io_icon, LV_SYMBOL_USB);
    lv_obj_set_pos(io_icon, 14, 12);
    lv_obj_set_style_text_color(io_icon, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(io_icon, &lv_font_montserrat_16, 0);

    lv_obj_t *io_title = lv_label_create(nav_io);
    lv_label_set_text(io_title, "I/O Test");
    lv_obj_set_pos(io_title, 40, 10);
    lv_obj_set_style_text_color(io_title, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(io_title, &lv_font_montserrat_12, 0);

    lv_obj_t *io_sub = lv_label_create(nav_io);
    lv_label_set_text(io_sub, "I/O test");
    lv_obj_set_pos(io_sub, 40, 28);
    lv_obj_set_style_text_color(io_sub, COL_MUTED, 0);
    lv_obj_set_style_text_font(io_sub, &lv_font_montserrat_10, 0);

    /* -- Divider + System Info  nav(4,182,172,1) -- */
    lv_obj_t *nav_div = lv_obj_create(nav);
    lv_obj_set_pos(nav_div, 4, 182);
    lv_obj_set_size(nav_div, 172, 1);
    lv_obj_set_style_bg_color(nav_div, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_bg_opa(nav_div, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(nav_div, 0, 0);
    lv_obj_set_style_border_width(nav_div, 0, 0);
    lv_obj_set_style_pad_all(nav_div, 0, 0);
    lv_obj_remove_flag(nav_div, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sys_info_lbl = lv_label_create(nav);
    lv_label_set_text(sys_info_lbl, "System Info");
    lv_obj_set_pos(sys_info_lbl, 8, 192);
    lv_obj_set_style_text_color(sys_info_lbl, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(sys_info_lbl, &lv_font_montserrat_10, 0);

    static const char *sys_keys[]  = { "Firmware", "Serial", "Uptime" };
    static const char *sys_vals[]  = { "v2.4.1",   "ITGC-00142", "14d 06:22" };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *k = lv_label_create(nav);
        lv_label_set_text(k, sys_keys[i]);
        lv_obj_set_pos(k, 8, 210 + i * 18);
        lv_obj_set_style_text_color(k, lv_color_hex(0x475569), 0);
        lv_obj_set_style_text_font(k, &lv_font_montserrat_10, 0);

        lv_obj_t *v = lv_label_create(nav);
        lv_label_set_text(v, sys_vals[i]);
        lv_obj_set_pos(v, 8, 222 + i * 18);
        lv_obj_set_style_text_color(v, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_text_font(v, &lv_font_montserrat_10, 0);
    }

    /* ── Right panel  content(196,8,596,410) ─────────────────── */
    lv_obj_t *rpanel = lv_obj_create(c);
    lv_obj_set_pos(rpanel, 196, 8);
    lv_obj_set_size(rpanel, 596, 410);
    lv_obj_set_style_bg_color(rpanel, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_bg_opa(rpanel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(rpanel, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(rpanel, 1, 0);
    lv_obj_set_style_radius(rpanel, 6, 0);
    lv_obj_set_style_pad_all(rpanel, 0, 0);
    lv_obj_remove_flag(rpanel, LV_OBJ_FLAG_SCROLLABLE);
    /* panel inner coords: panel_x = SVG_x - 196, panel_y = SVG_y - 52 */

    /* ── Section: Language  panel(20,26) ──────────────────────── */
    lv_obj_t *sec_lang_title = lv_label_create(rpanel);
    lv_label_set_text(sec_lang_title, "Language / Language");
    lv_obj_set_pos(sec_lang_title, 20, 26);
    lv_obj_set_style_text_color(sec_lang_title, COL_WHITE, 0);
    lv_obj_set_style_text_font(sec_lang_title, &lv_font_montserrat_14, 0);

    lv_obj_t *lang_div = lv_obj_create(rpanel);
    lv_obj_set_pos(lang_div, 20, 34);
    lv_obj_set_size(lang_div, 556, 1);
    lv_obj_set_style_bg_color(lang_div, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_opa(lang_div, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(lang_div, 0, 0);
    lv_obj_set_style_border_width(lang_div, 0, 0);
    lv_obj_set_style_pad_all(lang_div, 0, 0);
    lv_obj_remove_flag(lang_div, LV_OBJ_FLAG_SCROLLABLE);

    /* Japanese card (selected)  panel(20,44,172,80) */
    lv_obj_t *card_jp = lv_obj_create(rpanel);
    lv_obj_set_pos(card_jp, 20, 44);
    lv_obj_set_size(card_jp, 172, 80);
    lv_obj_set_style_bg_color(card_jp, lv_color_hex(0x1e3a5f), 0);
    lv_obj_set_style_bg_opa(card_jp, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card_jp, COL_BLUE, 0);
    lv_obj_set_style_border_width(card_jp, 2, 0);
    lv_obj_set_style_radius(card_jp, 6, 0);
    lv_obj_set_style_pad_all(card_jp, 0, 0);
    lv_obj_remove_flag(card_jp, LV_OBJ_FLAG_SCROLLABLE);

    /* Check circle at top-right */
    lv_obj_t *check_dot = lv_obj_create(card_jp);
    lv_obj_set_pos(check_dot, 148, 6);
    lv_obj_set_size(check_dot, 16, 16);
    lv_obj_set_style_bg_color(check_dot, COL_BLUE, 0);
    lv_obj_set_style_bg_opa(check_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(check_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(check_dot, 0, 0);
    lv_obj_set_style_pad_all(check_dot, 0, 0);
    lv_obj_remove_flag(check_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *check_lbl = lv_label_create(check_dot);
    lv_label_set_text(check_lbl, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(check_lbl, COL_BG, 0);
    lv_obj_set_style_text_font(check_lbl, &lv_font_montserrat_10, 0);
    lv_obj_center(check_lbl);

    lv_obj_t *jp_lbl1 = lv_label_create(card_jp);
    lv_label_set_text(jp_lbl1, "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");  /* 日本語 UTF-8 */
    lv_obj_set_pos(jp_lbl1, 12, 16);
    lv_obj_set_style_text_color(jp_lbl1, COL_BLUE, 0);
    lv_obj_set_style_text_font(jp_lbl1, &lv_font_montserrat_14, 0);

    lv_obj_t *jp_lbl2 = lv_label_create(card_jp);
    lv_label_set_text(jp_lbl2, "Japanese");
    lv_obj_set_pos(jp_lbl2, 12, 36);
    lv_obj_set_style_text_color(jp_lbl2, lv_color_hex(0x93c5fd), 0);
    lv_obj_set_style_text_font(jp_lbl2, &lv_font_montserrat_10, 0);

    lv_obj_t *jp_lbl3 = lv_label_create(card_jp);
    lv_label_set_text(jp_lbl3, "\xe7\x8f\xbe\xe5\x9c\xa8\xe9\x81\xb8\xe6\x8a\x9e\xe4\xb8\xad");  /* 現在選択中 */
    lv_obj_set_pos(jp_lbl3, 12, 54);
    lv_obj_set_style_text_color(jp_lbl3, lv_color_hex(0x60a5fa), 0);
    lv_obj_set_style_text_font(jp_lbl3, &lv_font_montserrat_10, 0);

    /* English card  panel(208,44,172,80) */
    lv_obj_t *card_en = lv_obj_create(rpanel);
    lv_obj_set_pos(card_en, 208, 44);
    lv_obj_set_size(card_en, 172, 80);
    lv_obj_set_style_bg_color(card_en, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(card_en, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card_en, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(card_en, 1, 0);
    lv_obj_set_style_radius(card_en, 6, 0);
    lv_obj_set_style_pad_all(card_en, 0, 0);
    lv_obj_remove_flag(card_en, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *en_lbl1 = lv_label_create(card_en);
    lv_label_set_text(en_lbl1, "English");
    lv_obj_set_pos(en_lbl1, 12, 16);
    lv_obj_set_style_text_color(en_lbl1, COL_MUTED, 0);
    lv_obj_set_style_text_font(en_lbl1, &lv_font_montserrat_14, 0);

    lv_obj_t *en_lbl2 = lv_label_create(card_en);
    lv_label_set_text(en_lbl2, "\xe8\x8b\xb1\xe8\xaa\x9e");  /* 英語 */
    lv_obj_set_pos(en_lbl2, 12, 36);
    lv_obj_set_style_text_color(en_lbl2, lv_color_hex(0x374151), 0);
    lv_obj_set_style_text_font(en_lbl2, &lv_font_montserrat_10, 0);

    lv_obj_t *en_lbl3 = lv_label_create(card_en);
    lv_label_set_text(en_lbl3, "\xe3\x82\xbf\xe3\x83\x83\xe3\x83\x97\xe3\x81\x97\xe3\x81\xa6\xe5\x88\x87\xe6\x9b\xbf");  /* タップして切替 */
    lv_obj_set_pos(en_lbl3, 12, 54);
    lv_obj_set_style_text_color(en_lbl3, lv_color_hex(0x374151), 0);
    lv_obj_set_style_text_font(en_lbl3, &lv_font_montserrat_10, 0);

    /* ── Section: Date/Time  panel y=150 ──────────────────────── */
    lv_obj_t *sec_dt_title = lv_label_create(rpanel);
    lv_label_set_text(sec_dt_title, "Date/Time");
    lv_obj_set_pos(sec_dt_title, 20, 150);
    lv_obj_set_style_text_color(sec_dt_title, COL_WHITE, 0);
    lv_obj_set_style_text_font(sec_dt_title, &lv_font_montserrat_14, 0);

    lv_obj_t *dt_div = lv_obj_create(rpanel);
    lv_obj_set_pos(dt_div, 20, 158);
    lv_obj_set_size(dt_div, 556, 1);
    lv_obj_set_style_bg_color(dt_div, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_opa(dt_div, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dt_div, 0, 0);
    lv_obj_set_style_border_width(dt_div, 0, 0);
    lv_obj_set_style_pad_all(dt_div, 0, 0);
    lv_obj_remove_flag(dt_div, LV_OBJ_FLAG_SCROLLABLE);

    /* "Date" label  panel(20,176) */
    lv_obj_t *date_lbl = lv_label_create(rpanel);
    lv_label_set_text(date_lbl, "Date");
    lv_obj_set_pos(date_lbl, 20, 176);
    lv_obj_set_style_text_color(date_lbl, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(date_lbl, &lv_font_montserrat_10, 0);

    /* Spinbox helper macro — creates a −/value/+ triplet */
    /* Year spinbox  panel(20,182,90,36) */
    typedef struct { int32_t x; int32_t y; int32_t w; const char *val; const char *unit; } SpinboxDef;
    static const SpinboxDef date_spins[] = {
        { 20, 182, 90, "2026", "\xe5\xb9\xb4" },   /* 年 */
        { 128, 182, 74, "04",  "\xe6\x9c\x88" },    /* 月 */
        { 224, 182, 74, "20",  "\xe6\x97\xa5" },    /* 日 */
    };
    static const SpinboxDef time_spins[] = {
        { 20, 242, 74, "14", "\xe6\x99\x82" },   /* 時 */
        { 116, 242, 74, "32", "\xe5\x88\x86" },  /* 分 */
    };

    auto make_spinbox = [&](lv_obj_t *parent, const SpinboxDef *d) {
        lv_obj_t *box = lv_obj_create(parent);
        lv_obj_set_pos(box, d->x, d->y);
        lv_obj_set_size(box, d->w, 36);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x0f172a), 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(box, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_radius(box, 4, 0);
        lv_obj_set_style_pad_all(box, 0, 0);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *btn_m = lv_btn_create(box);
        lv_obj_set_pos(btn_m, 0, 0);
        lv_obj_set_size(btn_m, 24, 36);
        lv_obj_set_style_bg_color(btn_m, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_bg_opa(btn_m, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn_m, 0, 0);
        lv_obj_set_style_pad_all(btn_m, 0, 0);
        lv_obj_t *lbl_m = lv_label_create(btn_m);
        lv_label_set_text(lbl_m, LV_SYMBOL_MINUS);
        lv_obj_center(lbl_m);
        lv_obj_set_style_text_color(lbl_m, lv_color_hex(0x94a3b8), 0);
        lv_obj_set_style_text_font(lbl_m, &lv_font_montserrat_10, 0);

        lv_obj_t *val_lbl = lv_label_create(box);
        lv_label_set_text(val_lbl, d->val);
        lv_obj_set_style_text_color(val_lbl, COL_WHITE, 0);
        lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_align(val_lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(val_lbl, d->w - 48);
        lv_obj_set_pos(val_lbl, 24, 11);

        lv_obj_t *btn_p = lv_btn_create(box);
        lv_obj_set_pos(btn_p, d->w - 24, 0);
        lv_obj_set_size(btn_p, 24, 36);
        lv_obj_set_style_bg_color(btn_p, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_bg_opa(btn_p, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn_p, 0, 0);
        lv_obj_set_style_pad_all(btn_p, 0, 0);
        lv_obj_t *lbl_p = lv_label_create(btn_p);
        lv_label_set_text(lbl_p, LV_SYMBOL_PLUS);
        lv_obj_center(lbl_p);
        lv_obj_set_style_text_color(lbl_p, lv_color_hex(0x94a3b8), 0);
        lv_obj_set_style_text_font(lbl_p, &lv_font_montserrat_10, 0);

        lv_obj_t *unit_lbl = lv_label_create(parent);
        lv_label_set_text(unit_lbl, d->unit);
        lv_obj_set_pos(unit_lbl, d->x + d->w + 2, d->y + 11);
        lv_obj_set_style_text_color(unit_lbl, lv_color_hex(0x94a3b8), 0);
        lv_obj_set_style_text_font(unit_lbl, &lv_font_montserrat_10, 0);
    };

    for (int i = 0; i < 3; i++) make_spinbox(rpanel, &date_spins[i]);

    /* "Time" label  panel(20,236) */
    lv_obj_t *time_lbl = lv_label_create(rpanel);
    lv_label_set_text(time_lbl, "Time");
    lv_obj_set_pos(time_lbl, 20, 236);
    lv_obj_set_style_text_color(time_lbl, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_10, 0);

    for (int i = 0; i < 2; i++) make_spinbox(rpanel, &time_spins[i]);

    /* Apply / Update Time button  panel(224,242,100,36) */
    lv_obj_t *btn_apply = lv_btn_create(rpanel);
    lv_obj_set_pos(btn_apply, 224, 242);
    lv_obj_set_size(btn_apply, 100, 36);
    lv_obj_set_style_bg_color(btn_apply, lv_color_hex(0x0f6e56), 0);
    lv_obj_set_style_bg_opa(btn_apply, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn_apply, 0, 0);
    lv_obj_set_style_radius(btn_apply, 4, 0);
    lv_obj_set_style_pad_all(btn_apply, 0, 0);
    lv_obj_t *lbl_apply = lv_label_create(btn_apply);
    lv_label_set_text(lbl_apply, "Update Time");
    lv_obj_center(lbl_apply);
    lv_obj_set_style_text_color(lbl_apply, COL_WHITE, 0);
    lv_obj_set_style_text_font(lbl_apply, &lv_font_montserrat_12, 0);

    /* ── Section: I/O Test  panel y=300 ───────────────────────── */
    lv_obj_t *sec_io_title = lv_label_create(rpanel);
    lv_label_set_text(sec_io_title, "I/O Test");
    lv_obj_set_pos(sec_io_title, 20, 300);
    lv_obj_set_style_text_color(sec_io_title, COL_WHITE, 0);
    lv_obj_set_style_text_font(sec_io_title, &lv_font_montserrat_14, 0);

    lv_obj_t *io_div = lv_obj_create(rpanel);
    lv_obj_set_pos(io_div, 20, 308);
    lv_obj_set_size(io_div, 556, 1);
    lv_obj_set_style_bg_color(io_div, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_opa(io_div, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(io_div, 0, 0);
    lv_obj_set_style_border_width(io_div, 0, 0);
    lv_obj_set_style_pad_all(io_div, 0, 0);
    lv_obj_remove_flag(io_div, LV_OBJ_FLAG_SCROLLABLE);

    /* Digital Input label  panel(20,326) */
    lv_obj_t *di_lbl = lv_label_create(rpanel);
    lv_label_set_text(di_lbl, "Digital Input");
    lv_obj_set_pos(di_lbl, 20, 326);
    lv_obj_set_style_text_color(di_lbl, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(di_lbl, &lv_font_montserrat_10, 0);

    /* IN1..IN4  panel(20,332), (82,332), (144,332), (206,332)  each 56x28 */
    typedef struct { int32_t x; bool on; const char *name; } InBtn;
    static const InBtn in_btns[] = {
        { 20,  true,  "IN1" },
        { 82,  false, "IN2" },
        { 144, false, "IN3" },
        { 206, true,  "IN4" },
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_obj_create(rpanel);
        lv_obj_set_pos(btn, in_btns[i].x, 332);
        lv_obj_set_size(btn, 56, 28);
        lv_obj_set_style_bg_color(btn, in_btns[i].on ? lv_color_hex(0x14532d) : lv_color_hex(0x1f2937), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, in_btns[i].on ? lv_color_hex(0x22c55e) : lv_color_hex(0x374151), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *dot = lv_obj_create(btn);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_pos(dot, 6, 11);
        lv_obj_set_style_bg_color(dot, in_btns[i].on ? lv_color_hex(0x22c55e) : lv_color_hex(0x4b5563), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(btn);
        char buf[16];
        lv_snprintf(buf, sizeof(buf), "%s %s", in_btns[i].name, in_btns[i].on ? "ON" : "OFF");
        lv_label_set_text(lbl, buf);
        lv_obj_set_pos(lbl, 16, 8);
        lv_obj_set_style_text_color(lbl, in_btns[i].on ? lv_color_hex(0x22c55e) : lv_color_hex(0x6b7280), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    }

    /* Digital Output label  panel(352,326) */
    lv_obj_t *do_lbl = lv_label_create(rpanel);
    lv_label_set_text(do_lbl, "Digital Output");
    lv_obj_set_pos(do_lbl, 352, 326);
    lv_obj_set_style_text_color(do_lbl, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(do_lbl, &lv_font_montserrat_10, 0);

    /* OUT1..OUT3  panel(352,332), (426,332), (500,332)  each 68x28 */
    typedef struct { int32_t x; bool forced; const char *name; const char *state; } OutBtn;
    static const OutBtn out_btns[] = {
        { 352, true,  "OUT1", "Force ON" },
        { 426, false, "OUT2", "OFF" },
        { 500, false, "OUT3", "OFF" },
    };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_obj_create(rpanel);
        lv_obj_set_pos(btn, out_btns[i].x, 332);
        lv_obj_set_size(btn, 68, 28);
        lv_obj_set_style_bg_color(btn, out_btns[i].forced ? lv_color_hex(0x1e3a5f) : lv_color_hex(0x1f2937), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(btn, out_btns[i].forced ? COL_BLUE : lv_color_hex(0x374151), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, out_btns[i].state);
        lv_obj_center(lbl);
        lv_obj_set_style_text_color(lbl, out_btns[i].forced ? lv_color_hex(0x93c5fd) : lv_color_hex(0x6b7280), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    }

    /* Note text  panel(20,380) */
    lv_obj_t *note_lbl = lv_label_create(rpanel);
    lv_label_set_text(note_lbl, "* Force ON overrides automatic output control.");
    lv_obj_set_pos(note_lbl, 20, 380);
    lv_obj_set_style_text_color(note_lbl, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_font(note_lbl, &lv_font_montserrat_10, 0);
}

// ============================================================
//  scr_threshold_method  方式選択 (3 method tiles)
//  content_y = SVG_y - 44
//  Tiles: x=57,295,533  y=72  w=210  h=270
// ============================================================
static void create_scr_threshold_method(void)
{
    lv_obj_t *c = make_settings_subscr(&scr_threshold_method, "Threshold \xe2\x80\x94 Method");

    /* subtitle area */
    lv_obj_t *lbl_sub = lv_label_create(c);
    lv_label_set_text(lbl_sub, "Select the threshold setting method");
    lv_obj_set_pos(lbl_sub, 0, 36);
    lv_obj_set_size(lbl_sub, 800, 18);
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_sub, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_align(lbl_sub, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *lbl_sub2 = lv_label_create(c);
    lv_label_set_text(lbl_sub2, "Ellipse threshold is configured per channel using the selected method");
    lv_obj_set_pos(lbl_sub2, 0, 54);
    lv_obj_set_size(lbl_sub2, 800, 16);
    lv_obj_set_style_text_font(lbl_sub2, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_sub2, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_align(lbl_sub2, LV_TEXT_ALIGN_CENTER, 0);

    /* 3 method tiles ─────────────────────────────────────────── */
    struct TileDef {
        int32_t     x;
        const char *name;
        const char *icon_sym;
        uint32_t    accent, icon_bg, icon_bd, icon_fg;
        const char *desc1, *desc2;
        const char *t1_txt; uint32_t t1_bg, t1_bd, t1_fg;
        const char *t2_txt;
        const char *note;
        uint32_t    btn_bg, btn_bd, btn_fg;
    };
    static const TileDef TILES[3] = {
        /* Sampling — green */
        { 57, "Sampling",    LV_SYMBOL_AUDIO,
          0x22c55e, 0x0d2b1e, 0x166534, 0x22c55e,
          "Measure OK/NG parts on device",
          "for automatic ellipse fit",
          "High accuracy",  0x14532d, 0x14532d, 0x4ade80,
          "Requires hardware",
          "Recommended: 5+ samples each",
          0x166534, 0x22c55e, 0xbbf7d0 },
        /* File Import — sky-blue */
        { 295, "File Import", LV_SYMBOL_SD_CARD,
          0x38bdf8, 0x0c2a3e, 0x1e4d6b, 0x38bdf8,
          "Read CSV of OK part data",
          "for automatic ellipse fit",
          "For production", 0x0c2a3e, 0x1e4d6b, 0x38bdf8,
          "SD card",
          "CSV with X, Y voltage columns",
          0x1e3a5f, 0x1d4ed8, 0x93c5fd },
        /* Manual Input — purple */
        { 533, "Manual Input", LV_SYMBOL_EDIT,
          0xa78bfa, 0x1e1b4b, 0x3730a3, 0xa78bfa,
          "Enter a / b axis values and",
          "rotation angle directly",
          "Instant setup",  0x1e1b4b, 0x3730a3, 0xa78bfa,
          "Expert",
          "Specify known parameters directly",
          0x2d1b69, 0x7c3aed, 0xc4b5fd },
    };

    for (int ti = 0; ti < 3; ti++) {
        const TileDef &td = TILES[ti];

        /* tile card content(td.x, 72, 210, 270) */
        lv_obj_t *card = lv_obj_create(c);
        lv_obj_set_pos(card, td.x, 72);
        lv_obj_set_size(card, 210, 270);
        lv_obj_set_style_bg_color(card, COL_SURFACE, 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        /* top accent bar h=5 — clipped to card's rounded top corners */
        lv_obj_t *acc = lv_obj_create(card);
        lv_obj_set_pos(acc, 0, 0); lv_obj_set_size(acc, 210, 5);
        lv_obj_set_style_bg_color(acc, lv_color_hex(td.accent), 0);
        lv_obj_set_style_bg_opa(acc, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(acc, 0, 0);
        bare_obj(acc);

        /* icon circle: r=28 → 56×56, centered at card(105,80) */
        lv_obj_t *ic = lv_obj_create(card);
        lv_obj_set_pos(ic, 77, 52); lv_obj_set_size(ic, 56, 56);
        lv_obj_set_style_bg_color(ic, lv_color_hex(td.icon_bg), 0);
        lv_obj_set_style_bg_opa(ic, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(ic, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(ic, lv_color_hex(td.icon_bd), 0);
        lv_obj_set_style_border_width(ic, 2, 0);
        lv_obj_set_style_pad_all(ic, 0, 0);
        lv_obj_remove_flag(ic, LV_OBJ_FLAG_SCROLLABLE);
        {
            lv_obj_t *l = lv_label_create(ic);
            lv_label_set_text(l, td.icon_sym);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(td.icon_fg), 0);
            lv_obj_center(l);
        }

        /* Sampling tile: add OK (green) / NG (red) accent dots on icon */
        if (ti == 0) {
            lv_obj_t *ok_dot = lv_obj_create(card);
            lv_obj_set_pos(ok_dot, 94, 71); lv_obj_set_size(ok_dot, 6, 6);
            lv_obj_set_style_bg_color(ok_dot, lv_color_hex(0x4ade80), 0);
            lv_obj_set_style_bg_opa(ok_dot, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(ok_dot, LV_RADIUS_CIRCLE, 0);
            bare_obj(ok_dot);

            lv_obj_t *ng_dot = lv_obj_create(card);
            lv_obj_set_pos(ng_dot, 112, 85); lv_obj_set_size(ng_dot, 6, 6);
            lv_obj_set_style_bg_color(ng_dot, lv_color_hex(0xef4444), 0);
            lv_obj_set_style_bg_opa(ng_dot, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(ng_dot, LV_RADIUS_CIRCLE, 0);
            bare_obj(ng_dot);
        }

        /* method name at card-inner y=132 */
        lv_obj_t *lbl_name = lv_label_create(card);
        lv_label_set_text(lbl_name, td.name);
        lv_obj_set_pos(lbl_name, 0, 132); lv_obj_set_size(lbl_name, 210, 20);
        lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_name, lv_color_hex(0xe2e8f0), 0);
        lv_obj_set_style_text_align(lbl_name, LV_TEXT_ALIGN_CENTER, 0);

        /* description lines at card-inner y=154,168 */
        {
            lv_obj_t *l = lv_label_create(card);
            lv_label_set_text(l, td.desc1);
            lv_obj_set_pos(l, 0, 154); lv_obj_set_size(l, 210, 14);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(0x64748b), 0);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        }
        {
            lv_obj_t *l = lv_label_create(card);
            lv_label_set_text(l, td.desc2);
            lv_obj_set_pos(l, 0, 168); lv_obj_set_size(l, 210, 14);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(0x64748b), 0);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        }

        /* tag pills at card-inner y=184 */
        /* tag 1 (colored): (12,184,88,18) */
        lv_obj_t *tag1 = lv_obj_create(card);
        lv_obj_set_pos(tag1, 12, 184); lv_obj_set_size(tag1, 88, 18);
        lv_obj_set_style_bg_color(tag1, lv_color_hex(td.t1_bg), 0);
        lv_obj_set_style_bg_opa(tag1, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(tag1, 9, 0);
        lv_obj_set_style_border_color(tag1, lv_color_hex(td.t1_bd), 0);
        lv_obj_set_style_border_width(tag1, 1, 0);
        lv_obj_set_style_pad_all(tag1, 0, 0);
        lv_obj_remove_flag(tag1, LV_OBJ_FLAG_SCROLLABLE);
        {
            lv_obj_t *l = lv_label_create(tag1);
            lv_label_set_text(l, td.t1_txt);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(td.t1_fg), 0);
            lv_obj_center(l);
        }

        /* tag 2 (gray): (108,184,90,18) */
        lv_obj_t *tag2 = lv_obj_create(card);
        lv_obj_set_pos(tag2, 108, 184); lv_obj_set_size(tag2, 90, 18);
        lv_obj_set_style_bg_color(tag2, COL_SURFACE, 0);
        lv_obj_set_style_bg_opa(tag2, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(tag2, 9, 0);
        lv_obj_set_style_border_color(tag2, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(tag2, 1, 0);
        lv_obj_set_style_pad_all(tag2, 0, 0);
        lv_obj_remove_flag(tag2, LV_OBJ_FLAG_SCROLLABLE);
        {
            lv_obj_t *l = lv_label_create(tag2);
            lv_label_set_text(l, td.t2_txt);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(0x64748b), 0);
            lv_obj_center(l);
        }

        /* hint note at card-inner y=228 */
        {
            lv_obj_t *l = lv_label_create(card);
            lv_label_set_text(l, td.note);
            lv_obj_set_pos(l, 0, 228); lv_obj_set_size(l, 210, 14);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(0x475569), 0);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        }

        /* select button at card-inner(20,242,170,20) */
        lv_obj_t *btn = lv_btn_create(card);
        lv_obj_set_pos(btn, 20, 242); lv_obj_set_size(btn, 170, 20);
        lv_obj_set_style_bg_color(btn, lv_color_hex(td.btn_bg), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(td.btn_bd), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        {
            lv_obj_t *l = lv_label_create(btn);
            lv_label_set_text(l, "Select  " LV_SYMBOL_RIGHT);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(td.btn_fg), 0);
            lv_obj_center(l);
        }
        /* navigation: Sampling→scr_threshold, Manual→scr_threshold_manual, File→back */
        if (ti == 0)
            lv_obj_add_event_cb(btn, tile_open_cb, LV_EVENT_CLICKED, scr_threshold);
        else if (ti == 2)
            lv_obj_add_event_cb(btn, tile_open_cb, LV_EVENT_CLICKED, scr_threshold_manual);
        else
            lv_obj_add_event_cb(btn, btn_back_to_settings_cb, LV_EVENT_CLICKED, NULL);
    }

    /* current method status note at content(400,374) */
    lv_obj_t *lbl_cur = lv_label_create(c);
    lv_label_set_text(lbl_cur,
        "Current method:  Sampling  /  "
        "Ch1 done \xc2\xb7 Ch2 done \xc2\xb7 Ch3 unset \xc2\xb7 Ch5 done \xc2\xb7 Ch8 done");
    lv_obj_set_pos(lbl_cur, 0, 374);
    lv_obj_set_size(lbl_cur, 800, 16);
    lv_obj_set_style_text_font(lbl_cur, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_cur, lv_color_hex(0x334155), 0);
    lv_obj_set_style_text_align(lbl_cur, LV_TEXT_ALIGN_CENTER, 0);
}

// ============================================================
//  Threshold-Manual preview canvas draw (one-shot timer)
//  Canvas 244x244 ARGB8888
//  Rotated ellipse: cx=122,cy=122, rx=54,ry=40, θ=-25°
// ============================================================
static void draw_thresh_manual_preview(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!thresh_manual_canvas) return;

    lv_layer_t layer;
    lv_canvas_init_layer(thresh_manual_canvas, &layer);

    /* 1. Background */
    {
        lv_draw_rect_dsc_t r;
        lv_draw_rect_dsc_init(&r);
        r.bg_color     = lv_color_hex(0x111827);
        r.bg_opa       = LV_OPA_COVER;
        r.radius       = 0;
        r.border_width = 0;
        lv_area_t a    = {0, 0, 243, 243};
        lv_draw_rect(&layer, &r, &a);
    }

    /* 2. Grid lines #1e293b: h at 62,122,182; v at 60,122,184 */
    {
        lv_draw_line_dsc_t g;
        lv_draw_line_dsc_init(&g);
        g.color = lv_color_hex(0x1e293b);
        g.width = 1;
        g.opa   = LV_OPA_COVER;
        static const int32_t HY[] = {62, 122, 182};
        static const int32_t VX[] = {60, 122, 184};
        for (int i = 0; i < 3; i++) {
            g.p1.x = 0;   g.p1.y = HY[i];
            g.p2.x = 243; g.p2.y = HY[i];
            lv_draw_line(&layer, &g);
        }
        for (int i = 0; i < 3; i++) {
            g.p1.x = VX[i]; g.p1.y = 0;
            g.p2.x = VX[i]; g.p2.y = 243;
            lv_draw_line(&layer, &g);
        }
    }

    /* 3. X/Y axes #2d3748 */
    {
        lv_draw_line_dsc_t ax;
        lv_draw_line_dsc_init(&ax);
        ax.color = lv_color_hex(0x2d3748);
        ax.width = 1;
        ax.opa   = LV_OPA_COVER;
        ax.p1.x = 0;   ax.p1.y = 122; ax.p2.x = 243; ax.p2.y = 122;
        lv_draw_line(&layer, &ax);
        ax.p1.x = 122; ax.p1.y = 0;   ax.p2.x = 122; ax.p2.y = 243;
        lv_draw_line(&layer, &ax);
    }

    /* 4. Rotated ellipse: θ=-25° → cos=-25°=0.90631, sin=-25°=-0.42262
       x(t)=122+54cos(t)·cosθ - 40sin(t)·sinθ
       y(t)=122+54cos(t)·sinθ + 40sin(t)·cosθ   */
    {
        lv_draw_line_dsc_t el;
        lv_draw_line_dsc_init(&el);
        el.color = lv_color_hex(0xa78bfa);
        el.width = 2;
        el.opa   = LV_OPA_COVER;
        const double COSTH =  0.906308;
        const double SINTH = -0.422618;
        const double STEP  =  3.14159265358979 / 30.0;
        for (int i = 0; i < 60; i++) {
            double a1 = i * STEP, a2 = (i + 1) * STEP;
            el.p1.x = (int32_t)(122.0 + 54.0*cos(a1)*COSTH - 40.0*sin(a1)*SINTH);
            el.p1.y = (int32_t)(122.0 + 54.0*cos(a1)*SINTH + 40.0*sin(a1)*COSTH);
            el.p2.x = (int32_t)(122.0 + 54.0*cos(a2)*COSTH - 40.0*sin(a2)*SINTH);
            el.p2.y = (int32_t)(122.0 + 54.0*cos(a2)*SINTH + 40.0*sin(a2)*COSTH);
            lv_draw_line(&layer, &el);
        }
    }

    /* 5. a-arm: center → major-axis tip (171,99), dashed purple */
    {
        lv_draw_line_dsc_t ar;
        lv_draw_line_dsc_init(&ar);
        ar.color      = lv_color_hex(0xa78bfa);
        ar.width      = 1;
        ar.opa        = LV_OPA_60;
        ar.dash_width = 4;
        ar.dash_gap   = 3;
        ar.p1.x = 122; ar.p1.y = 122;
        ar.p2.x = 171; ar.p2.y = 99;
        lv_draw_line(&layer, &ar);
    }

    /* 6. b-arm: center → minor-axis tip (139,158), dashed purple */
    {
        lv_draw_line_dsc_t br;
        lv_draw_line_dsc_init(&br);
        br.color      = lv_color_hex(0xa78bfa);
        br.width      = 1;
        br.opa        = LV_OPA_60;
        br.dash_width = 4;
        br.dash_gap   = 3;
        br.p1.x = 122; br.p1.y = 122;
        br.p2.x = 139; br.p2.y = 158;
        lv_draw_line(&layer, &br);
    }

    /* 7. θ arc: 0° → -25° at r=20px from center, amber */
    {
        lv_draw_line_dsc_t th;
        lv_draw_line_dsc_init(&th);
        th.color = lv_color_hex(0xfbbf24);
        th.width = 1;
        th.opa   = LV_OPA_COVER;
        const double END  = -0.43633;  /* -25° in radians */
        const double STEP = END / 6.0;
        for (int i = 0; i < 6; i++) {
            double a1 = i * STEP, a2 = (i + 1) * STEP;
            th.p1.x = (int32_t)(122.0 + 20.0 * cos(a1));
            th.p1.y = (int32_t)(122.0 + 20.0 * sin(a1));
            th.p2.x = (int32_t)(122.0 + 20.0 * cos(a2));
            th.p2.y = (int32_t)(122.0 + 20.0 * sin(a2));
            lv_draw_line(&layer, &th);
        }
    }

    /* 8. Center cross */
    {
        lv_draw_line_dsc_t cr;
        lv_draw_line_dsc_init(&cr);
        cr.color = lv_color_hex(0x475569);
        cr.width = 1;
        cr.opa   = LV_OPA_COVER;
        cr.p1.x = 118; cr.p1.y = 122; cr.p2.x = 126; cr.p2.y = 122;
        lv_draw_line(&layer, &cr);
        cr.p1.x = 122; cr.p1.y = 118; cr.p2.x = 122; cr.p2.y = 126;
        lv_draw_line(&layer, &cr);
    }

    /* 9. X/Y axis labels */
    {
        lv_draw_label_dsc_t lbl;
        lv_draw_label_dsc_init(&lbl);
        lbl.font  = &lv_font_montserrat_10;
        lbl.opa   = LV_OPA_COVER;
        lbl.color = lv_color_hex(0x475569);
        lbl.text  = "X";
        lv_area_t a1 = {228, 110, 243, 123};
        lv_draw_label(&layer, &lbl, &a1);
        lbl.text  = "Y";
        lv_area_t a2 = {124, 0, 139, 13};
        lv_draw_label(&layer, &lbl, &a2);
    }

    /* 10. a / b / θ arm labels */
    {
        lv_draw_label_dsc_t lbl;
        lv_draw_label_dsc_init(&lbl);
        lbl.font  = &lv_font_montserrat_10;
        lbl.opa   = LV_OPA_COVER;
        lbl.color = lv_color_hex(0xa78bfa);
        lbl.text  = "a";
        lv_area_t a1 = {174, 88, 187, 101};
        lv_draw_label(&layer, &lbl, &a1);
        lbl.text  = "b";
        lv_area_t a2 = {142, 161, 155, 174};
        lv_draw_label(&layer, &lbl, &a2);
        lbl.color = lv_color_hex(0xfbbf24);
        lbl.text  = "\xce\xb8";   /* θ UTF-8 */
        lv_area_t a3 = {144, 106, 157, 119};
        lv_draw_label(&layer, &lbl, &a3);
    }

    lv_canvas_finish_layer(thresh_manual_canvas, &layer);
}

// ============================================================
//  scr_threshold_manual  手動閾値入力 (左Ch一覧/中央プレビュー/右パラメータ)
// ============================================================
static void create_scr_threshold_manual(void)
{
    lv_obj_t *c = make_settings_subscr(&scr_threshold_manual, "Threshold \xe2\x80\x94 Manual");
    /* content_y = SVG_y - 44
       right panel inner_x = content_x - 442, inner_y = content_y - 8  */

    // ── LEFT PANEL: channel list  content(8,8,150,392) ─────────
    lv_obj_t *left = lv_obj_create(c);
    lv_obj_set_pos(left, 8, 8);
    lv_obj_set_size(left, 150, 392);
    lv_obj_set_style_bg_color(left, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(left, 6, 0);
    lv_obj_set_style_border_color(left, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_border_width(left, 1, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_remove_flag(left, LV_OBJ_FLAG_SCROLLABLE);

    /* 5 channel rows, each 70px */
    struct ChRow { int ch; bool sel; bool warn; const char *ab; const char *theta; };
    static const ChRow CH_ROWS[5] = {
        {1, true,  false, "a=1.800  b=1.350", "\xce\xb8=+25\xc2\xb0"},
        {2, false, false, "a=----  b=----",   ""},
        {3, false, true,  "a=----  b=----",   ""},
        {4, false, false, "a=----  b=----",   ""},
        {5, false, false, "a=----  b=----",   ""},
    };
    for (int i = 0; i < 5; i++) {
        const ChRow &cr = CH_ROWS[i];
        int32_t row_y = i * 70;

        lv_obj_t *row = lv_obj_create(left);
        lv_obj_set_pos(row, 0, row_y);
        lv_obj_set_size(row, 150, 70);
        lv_obj_set_style_bg_color(row, cr.sel ? lv_color_hex(0x1e3a5f) : lv_color_hex(0x111827), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        bare_obj(row);

        /* left accent bar (4px purple) for selected */
        if (cr.sel) {
            lv_obj_t *bar = lv_obj_create(row);
            lv_obj_set_pos(bar, 0, 0);
            lv_obj_set_size(bar, 4, 70);
            lv_obj_set_style_bg_color(bar, lv_color_hex(0xa78bfa), 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
            bare_obj(bar);
        }

        char ch_buf[8];
        lv_snprintf(ch_buf, sizeof(ch_buf), "Ch %d", cr.ch);
        lv_obj_t *lbl_ch = lv_label_create(row);
        lv_label_set_text(lbl_ch, ch_buf);
        lv_obj_set_pos(lbl_ch, 12, 10);
        lv_obj_set_style_text_font(lbl_ch, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_ch, cr.sel ? lv_color_hex(0xc4b5fd) : lv_color_hex(0x94a3b8), 0);

        lv_obj_t *lbl_ab = lv_label_create(row);
        lv_label_set_text(lbl_ab, cr.ab);
        lv_obj_set_pos(lbl_ab, 12, 32);
        lv_obj_set_style_text_font(lbl_ab, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lbl_ab, cr.sel ? lv_color_hex(0xa78bfa) : lv_color_hex(0x334155), 0);

        if (cr.sel && cr.theta[0]) {
            lv_obj_t *lbl_th = lv_label_create(row);
            lv_label_set_text(lbl_th, cr.theta);
            lv_obj_set_pos(lbl_th, 12, 50);
            lv_obj_set_style_text_font(lbl_th, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_th, lv_color_hex(0xfbbf24), 0);
        }

        /* warning dot for Ch3 at left-inner(136, row_y+16) */
        if (cr.warn) {
            lv_obj_t *dot = lv_obj_create(left);
            lv_obj_set_pos(dot, 136, row_y + 16);
            lv_obj_set_size(dot, 8, 8);
            lv_obj_set_style_bg_color(dot, lv_color_hex(0xf59e0b), 0);
            lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            bare_obj(dot);
        }

        /* row separator */
        if (i < 4) {
            lv_obj_t *sep = lv_obj_create(left);
            lv_obj_set_pos(sep, 0, row_y + 69);
            lv_obj_set_size(sep, 150, 1);
            lv_obj_set_style_bg_color(sep, lv_color_hex(0x1e293b), 0);
            lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
            bare_obj(sep);
        }
    }

    /* scroll hint at bottom of left panel */
    lv_obj_t *lbl_more = lv_label_create(left);
    lv_label_set_text(lbl_more, "\xe2\x96\xbe  6 ~ 16");
    lv_obj_set_pos(lbl_more, 0, 360);
    lv_obj_set_size(lbl_more, 150, 20);
    lv_obj_set_style_text_font(lbl_more, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_more, lv_color_hex(0x334155), 0);
    lv_obj_set_style_text_align(lbl_more, LV_TEXT_ALIGN_CENTER, 0);

    // ── CENTER CANVAS CONTAINER  content(178,34,244,244) ───────
    lv_obj_t *canvas_cont = lv_obj_create(c);
    lv_obj_set_pos(canvas_cont, 178, 34);
    lv_obj_set_size(canvas_cont, 244, 244);
    lv_obj_set_style_bg_color(canvas_cont, lv_color_hex(0x111827), 0);
    lv_obj_set_style_bg_opa(canvas_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(canvas_cont, 4, 0);
    lv_obj_set_style_border_color(canvas_cont, lv_color_hex(0x2d1b69), 0);
    lv_obj_set_style_border_width(canvas_cont, 1, 0);
    lv_obj_set_style_pad_all(canvas_cont, 0, 0);
    lv_obj_remove_flag(canvas_cont, LV_OBJ_FLAG_SCROLLABLE);

    thresh_manual_canvas = lv_canvas_create(canvas_cont);
    lv_canvas_set_buffer(thresh_manual_canvas, thresh_manual_plot_buf, 244, 244, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_set_pos(thresh_manual_canvas, 0, 0);
    lv_obj_set_size(thresh_manual_canvas, 244, 244);

    /* canvas subtitle */
    lv_obj_t *lbl_cv_ttl = lv_label_create(c);
    lv_label_set_text(lbl_cv_ttl, "Ch 1  Ellipse Preview");
    lv_obj_set_pos(lbl_cv_ttl, 178, 284);
    lv_obj_set_size(lbl_cv_ttl, 244, 16);
    lv_obj_set_style_text_font(lbl_cv_ttl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_cv_ttl, lv_color_hex(0x475569), 0);
    lv_obj_set_style_text_align(lbl_cv_ttl, LV_TEXT_ALIGN_CENTER, 0);

    // ── RIGHT PANEL  content(442,8,350,392) ────────────────────
    lv_obj_t *rp = lv_obj_create(c);
    lv_obj_set_pos(rp, 442, 8);
    lv_obj_set_size(rp, 350, 392);
    lv_obj_set_style_bg_color(rp, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(rp, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rp, 8, 0);
    lv_obj_set_style_border_color(rp, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(rp, 1, 0);
    lv_obj_set_style_pad_all(rp, 0, 0);
    lv_obj_remove_flag(rp, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_rp_ttl = lv_label_create(rp);
    lv_label_set_text(lbl_rp_ttl, "Ch 1  -  Manual Threshold");
    lv_obj_set_pos(lbl_rp_ttl, 16, 12);
    lv_obj_set_style_text_font(lbl_rp_ttl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_rp_ttl, lv_color_hex(0x94a3b8), 0);

    {
        lv_obj_t *d = lv_obj_create(rp);
        lv_obj_set_pos(d, 16, 34); lv_obj_set_size(d, 318, 1);
        lv_obj_set_style_bg_color(d, lv_color_hex(0x334155), 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        bare_obj(d);
    }

    /* a and b axis spinbox sections
       y_base: a=40, b=178
       fill_px: a=113 (1.800/5.0 * 314), b=85 (1.350/5.0 * 314)  */
    struct AxisParam { const char *label; const char *val; int32_t fill_px; int32_t y_base; };
    const AxisParam AXES[2] = {
        {"a  (semi-axis)", "1.800", 113, 40},
        {"b  (semi-axis)", "1.350",  85, 178},
    };
    for (int ai = 0; ai < 2; ai++) {
        const AxisParam &ap = AXES[ai];

        lv_obj_t *lbl_ax = lv_label_create(rp);
        lv_label_set_text(lbl_ax, ap.label);
        lv_obj_set_pos(lbl_ax, 16, ap.y_base);
        lv_obj_set_style_text_font(lbl_ax, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(lbl_ax, lv_color_hex(0x64748b), 0);

        /* input box inner(16, y_base+10, 314, 52) */
        lv_obj_t *box = lv_obj_create(rp);
        lv_obj_set_pos(box, 16, ap.y_base + 10);
        lv_obj_set_size(box, 314, 52);
        lv_obj_set_style_bg_color(box, lv_color_hex(0x0f172a), 0);
        lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(box, 6, 0);
        lv_obj_set_style_border_color(box, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(box, 1, 0);
        lv_obj_set_style_pad_all(box, 0, 0);
        lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

        /* minus button at box(6,6,40,40) */
        lv_obj_t *btn_m = lv_obj_create(box);
        lv_obj_set_pos(btn_m, 6, 6); lv_obj_set_size(btn_m, 40, 40);
        lv_obj_set_style_bg_color(btn_m, lv_color_hex(0x1e3a5f), 0);
        lv_obj_set_style_bg_opa(btn_m, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn_m, 6, 0);
        lv_obj_set_style_border_color(btn_m, lv_color_hex(0x1d4ed8), 0);
        lv_obj_set_style_border_width(btn_m, 1, 0);
        lv_obj_set_style_pad_all(btn_m, 0, 0);
        lv_obj_remove_flag(btn_m, LV_OBJ_FLAG_SCROLLABLE);
        {
            lv_obj_t *l = lv_label_create(btn_m);
            lv_label_set_text(l, LV_SYMBOL_MINUS);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(0x93c5fd), 0);
            lv_obj_center(l);
        }

        /* value label centered in box */
        lv_obj_t *lbl_val = lv_label_create(box);
        lv_label_set_text(lbl_val, ap.val);
        lv_obj_set_pos(lbl_val, 52, 0);
        lv_obj_set_size(lbl_val, 198, 52);
        lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl_val, lv_color_hex(0xa78bfa), 0);
        lv_obj_set_style_text_align(lbl_val, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(lbl_val, 13, 0);

        /* unit label */
        lv_obj_t *lbl_unit = lv_label_create(box);
        lv_label_set_text(lbl_unit, "V");
        lv_obj_set_pos(lbl_unit, 256, 0);
        lv_obj_set_size(lbl_unit, 32, 52);
        lv_obj_set_style_text_font(lbl_unit, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_unit, lv_color_hex(0x475569), 0);
        lv_obj_set_style_pad_top(lbl_unit, 18, 0);

        /* plus button at box(268,6,40,40) */
        lv_obj_t *btn_p = lv_obj_create(box);
        lv_obj_set_pos(btn_p, 268, 6); lv_obj_set_size(btn_p, 40, 40);
        lv_obj_set_style_bg_color(btn_p, lv_color_hex(0x1e3a5f), 0);
        lv_obj_set_style_bg_opa(btn_p, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(btn_p, 6, 0);
        lv_obj_set_style_border_color(btn_p, lv_color_hex(0x1d4ed8), 0);
        lv_obj_set_style_border_width(btn_p, 1, 0);
        lv_obj_set_style_pad_all(btn_p, 0, 0);
        lv_obj_remove_flag(btn_p, LV_OBJ_FLAG_SCROLLABLE);
        {
            lv_obj_t *l = lv_label_create(btn_p);
            lv_label_set_text(l, LV_SYMBOL_PLUS);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(0x93c5fd), 0);
            lv_obj_center(l);
        }

        /* slider track at rp-inner(16, y_base+70, 314, 6) */
        lv_obj_t *sl_track = lv_obj_create(rp);
        lv_obj_set_pos(sl_track, 16, ap.y_base + 70);
        lv_obj_set_size(sl_track, 314, 6);
        lv_obj_set_style_bg_color(sl_track, lv_color_hex(0x0f172a), 0);
        lv_obj_set_style_bg_opa(sl_track, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(sl_track, 3, 0);
        bare_obj(sl_track);

        lv_obj_t *sl_fill = lv_obj_create(rp);
        lv_obj_set_pos(sl_fill, 16, ap.y_base + 70);
        lv_obj_set_size(sl_fill, ap.fill_px, 6);
        lv_obj_set_style_bg_color(sl_fill, lv_color_hex(0xa78bfa), 0);
        lv_obj_set_style_bg_opa(sl_fill, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(sl_fill, 3, 0);
        bare_obj(sl_fill);

        lv_obj_t *sl_knob = lv_obj_create(rp);
        lv_obj_set_pos(sl_knob, 16 + ap.fill_px - 9, ap.y_base + 67);
        lv_obj_set_size(sl_knob, 18, 18);
        lv_obj_set_style_bg_color(sl_knob, lv_color_hex(0xa78bfa), 0);
        lv_obj_set_style_bg_opa(sl_knob, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(sl_knob, LV_RADIUS_CIRCLE, 0);
        bare_obj(sl_knob);

        /* slider range labels */
        {
            lv_obj_t *l = lv_label_create(rp);
            lv_label_set_text(l, "0.0");
            lv_obj_set_pos(l, 16, ap.y_base + 80);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(0x334155), 0);
        }
        {
            lv_obj_t *l = lv_label_create(rp);
            lv_label_set_text(l, "5.0 V");
            lv_obj_set_pos(l, 295, ap.y_base + 80);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(0x334155), 0);
        }

        /* step selector label */
        {
            lv_obj_t *l = lv_label_create(rp);
            lv_label_set_text(l, "Step");
            lv_obj_set_pos(l, 16, ap.y_base + 96);
            lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(l, lv_color_hex(0x64748b), 0);
        }

        /* step buttons: 0.01 / 0.05✓ / 0.10 */
        static const char *const SV[] = {"0.01", "0.05", "0.10"};
        static const bool        SS[] = {false, true, false};
        static const int32_t     SX[] = {50, 108, 166};
        for (int si = 0; si < 3; si++) {
            lv_obj_t *sb = lv_obj_create(rp);
            lv_obj_set_pos(sb, SX[si], ap.y_base + 93);
            lv_obj_set_size(sb, 52, 26);
            lv_obj_set_style_bg_color(sb, SS[si] ? lv_color_hex(0x2d1b69) : COL_BG, 0);
            lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(sb, 4, 0);
            lv_obj_set_style_border_color(sb, SS[si] ? lv_color_hex(0x7c3aed) : COL_BTN_GRAY, 0);
            lv_obj_set_style_border_width(sb, 1, 0);
            lv_obj_set_style_pad_all(sb, 0, 0);
            lv_obj_remove_flag(sb, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_t *lbl_sv = lv_label_create(sb);
            lv_label_set_text(lbl_sv, SV[si]);
            lv_obj_set_style_text_font(lbl_sv, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(lbl_sv, SS[si] ? lv_color_hex(0xc4b5fd) : lv_color_hex(0x94a3b8), 0);
            lv_obj_center(lbl_sv);
        }
    }

    /* divider before θ section */
    {
        lv_obj_t *d = lv_obj_create(rp);
        lv_obj_set_pos(d, 16, 316); lv_obj_set_size(d, 318, 1);
        lv_obj_set_style_bg_color(d, lv_color_hex(0x334155), 0);
        lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
        bare_obj(d);
    }

    // ── θ SECTION ─────────────────────────────────────────────
    lv_obj_t *lbl_th_hdr = lv_label_create(rp);
    lv_label_set_text(lbl_th_hdr, "\xce\xb8  (rotation)");
    lv_obj_set_pos(lbl_th_hdr, 16, 322);
    lv_obj_set_style_text_font(lbl_th_hdr, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lbl_th_hdr, lv_color_hex(0x64748b), 0);

    /* θ box inner(16,336,318,44) */
    lv_obj_t *th_box = lv_obj_create(rp);
    lv_obj_set_pos(th_box, 16, 336);
    lv_obj_set_size(th_box, 318, 44);
    lv_obj_set_style_bg_color(th_box, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_bg_opa(th_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(th_box, 6, 0);
    lv_obj_set_style_border_color(th_box, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(th_box, 1, 0);
    lv_obj_set_style_pad_all(th_box, 0, 0);
    lv_obj_remove_flag(th_box, LV_OBJ_FLAG_SCROLLABLE);

    /* range labels */
    {
        lv_obj_t *l = lv_label_create(th_box);
        lv_label_set_text(l, "-45\xc2\xb0");
        lv_obj_set_pos(l, 6, 14);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0x334155), 0);
    }
    {
        lv_obj_t *l = lv_label_create(th_box);
        lv_label_set_text(l, "+45\xc2\xb0");
        lv_obj_set_pos(l, 287, 14);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(0x334155), 0);
    }

    /* θ slider track (40,17,238,10) */
    lv_obj_t *th_track = lv_obj_create(th_box);
    lv_obj_set_pos(th_track, 40, 17); lv_obj_set_size(th_track, 238, 10);
    lv_obj_set_style_bg_color(th_track, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_bg_opa(th_track, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(th_track, 5, 0);
    bare_obj(th_track);

    /* θ fill: +25° of range -45°~+45° → (25+45)/90 * 238 = 185px ≈ 181 per SVG */
    lv_obj_t *th_fill = lv_obj_create(th_box);
    lv_obj_set_pos(th_fill, 40, 17); lv_obj_set_size(th_fill, 181, 10);
    lv_obj_set_style_bg_color(th_fill, lv_color_hex(0xfbbf24), 0);
    lv_obj_set_style_bg_opa(th_fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(th_fill, 5, 0);
    bare_obj(th_fill);

    /* θ knob */
    lv_obj_t *th_knob = lv_obj_create(th_box);
    lv_obj_set_pos(th_knob, 40 + 181 - 9, 13); lv_obj_set_size(th_knob, 18, 18);
    lv_obj_set_style_bg_color(th_knob, lv_color_hex(0xfbbf24), 0);
    lv_obj_set_style_bg_opa(th_knob, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(th_knob, LV_RADIUS_CIRCLE, 0);
    bare_obj(th_knob);

    /* θ value label */
    lv_obj_t *lbl_th_val = lv_label_create(th_box);
    lv_label_set_text(lbl_th_val, "+25\xc2\xb0");
    lv_obj_set_pos(lbl_th_val, 130, 14);
    lv_obj_set_style_text_font(lbl_th_val, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_th_val, lv_color_hex(0xfbbf24), 0);

    // ── BOTTOM BUTTONS ─────────────────────────────────────────
    /* Cancel content(8,410,130,24) */
    lv_obj_t *btn_cancel = lv_btn_create(c);
    lv_obj_set_pos(btn_cancel, 8, 410);
    lv_obj_set_size(btn_cancel, 130, 24);
    lv_obj_set_style_bg_color(btn_cancel, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_cancel, 6, 0);
    lv_obj_set_style_border_color(btn_cancel, COL_BTN_GRAY, 0);
    lv_obj_set_style_border_width(btn_cancel, 1, 0);
    lv_obj_set_style_pad_all(btn_cancel, 0, 0);
    lv_obj_add_event_cb(btn_cancel, btn_back_to_settings_cb, LV_EVENT_CLICKED, NULL);
    {
        lv_obj_t *l = lv_label_create(btn_cancel);
        lv_label_set_text(l, "Cancel");
        lv_obj_set_style_text_color(l, lv_color_hex(0x94a3b8), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        lv_obj_center(l);
    }

    /* Apply content(646,410,146,24) */
    lv_obj_t *btn_apply = lv_btn_create(c);
    lv_obj_set_pos(btn_apply, 646, 410);
    lv_obj_set_size(btn_apply, 146, 24);
    lv_obj_set_style_bg_color(btn_apply, lv_color_hex(0x2d1b69), 0);
    lv_obj_set_style_bg_opa(btn_apply, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn_apply, 6, 0);
    lv_obj_set_style_border_color(btn_apply, lv_color_hex(0x7c3aed), 0);
    lv_obj_set_style_border_width(btn_apply, 1, 0);
    lv_obj_set_style_pad_all(btn_apply, 0, 0);
    lv_obj_add_event_cb(btn_apply, btn_back_to_settings_cb, LV_EVENT_CLICKED, NULL);
    {
        lv_obj_t *l = lv_label_create(btn_apply);
        lv_label_set_text(l, LV_SYMBOL_OK "  Apply");
        lv_obj_set_style_text_color(l, lv_color_hex(0xc4b5fd), 0);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
        lv_obj_center(l);
    }

    /* one-shot timer: draw canvas after screen is ready */
    lv_timer_create(draw_thresh_manual_preview, 50, NULL);
}

// ============================================================
//  scr_settings_home  生成
// ============================================================
static void create_scr_settings_home(void)
{
    scr_settings_home = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_settings_home, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr_settings_home, LV_OPA_COVER, 0);
    bare_obj(scr_settings_home);

    // ── header_bar ───────────────────────────────────────────
    lv_obj_t *hdr = lv_obj_create(scr_settings_home);
    lv_obj_set_size(hdr, 800, 44);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, COL_BG, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    bare_obj(hdr);

    // btn_back: 表示 → scr_main へ戻る
    lv_obj_t *back_btn = lv_btn_create(hdr);
    lv_obj_set_size(back_btn, 36, 28);
    lv_obj_set_pos(back_btn, 4, 8);
    lv_obj_set_style_bg_color(back_btn, COL_BTN_GRAY, 0);
    lv_obj_t *back_icon = lv_label_create(back_btn);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_center(back_icon);
    lv_obj_add_event_cb(back_btn, btn_back_settings_cb, LV_EVENT_CLICKED, NULL);

    // lbl_title
    lv_obj_t *hdr_title = lv_label_create(hdr);
    lv_label_set_text(hdr_title, "Settings");
    lv_obj_set_pos(hdr_title, 48, 14);
    lv_obj_set_style_text_color(hdr_title, COL_WHITE, 0);
    lv_obj_set_style_text_font(hdr_title, &lv_font_montserrat_16, 0);

    // mem_selector (共通外観)
    lv_obj_t *mem_sel = lv_obj_create(hdr);
    lv_obj_set_size(mem_sel, 168, 30);
    lv_obj_set_pos(mem_sel, 316, 7);
    lv_obj_set_style_bg_color(mem_sel, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(mem_sel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mem_sel, 6, 0);
    bare_obj(mem_sel);

    lv_obj_t *mp = lv_btn_create(mem_sel);
    lv_obj_set_size(mp, 24, 22);
    lv_obj_set_pos(mp, 2, 4);
    lv_obj_set_style_bg_color(mp, COL_BTN_GRAY, 0);
    lv_label_set_text(lv_label_create(mp), LV_SYMBOL_LEFT);
    lv_obj_center(lv_obj_get_child(mp, 0));

    lv_obj_t *mno = lv_label_create(mem_sel);
    lv_label_set_text(mno, "001");
    lv_obj_set_pos(mno, 30, 7);
    lv_obj_set_style_text_color(mno, COL_BLUE, 0);
    lv_obj_set_style_text_font(mno, &lv_font_montserrat_16, 0);

    lv_obj_t *mn = lv_btn_create(mem_sel);
    lv_obj_set_size(mn, 24, 22);
    lv_obj_set_pos(mn, 72, 4);
    lv_obj_set_style_bg_color(mn, COL_BTN_GRAY, 0);
    lv_label_set_text(lv_label_create(mn), LV_SYMBOL_RIGHT);
    lv_obj_center(lv_obj_get_child(mn, 0));

    lv_obj_t *mname = lv_label_create(mem_sel);
    lv_label_set_text(mname, "M1: (unnamed)");
    lv_obj_set_pos(mname, 100, 10);
    lv_obj_set_style_text_color(mname, COL_MUTED, 0);
    lv_obj_set_style_text_font(mname, &lv_font_montserrat_10, 0);

    // ── タイルコンテナ ────────────────────────────────────────
    // 5タイル × (w=140 h=240)  gap=12
    // 合計幅: 5×140 + 4×12 = 748  → x開始: (800-748)/2 = 26
    // y中央: 44 + (436-240)/2 = 142

    static const char *tile_titles[] = {
        "Meas. Conditions",
        "Threshold",
        "History",
        "Correction",
        "Detail Settings"
    };
    static const char *tile_subs[] = {
        "Frequency / Gain",
        "Ellipse params",
        "Log & CSV export",
        "Correction coeff.",
        "Language / Clock / I/O"
    };
    static const char *tile_icons[] = {
        LV_SYMBOL_AUDIO,
        LV_SYMBOL_WARNING,
        LV_SYMBOL_LIST,
        LV_SYMBOL_EDIT,
        LV_SYMBOL_SETTINGS
    };

    const int tile_w = 140, tile_h = 240, gap = 12;
    const int x0 = (800 - (5 * tile_w + 4 * gap)) / 2;  // = 26
    const int y0 = 44 + (436 - tile_h) / 2;              // = 142

    for (int i = 0; i < 5; i++) {
        settings_tile[i] = lv_btn_create(scr_settings_home);
        lv_obj_set_size(settings_tile[i], tile_w, tile_h);
        lv_obj_set_pos(settings_tile[i], x0 + i * (tile_w + gap), y0);
        lv_obj_set_style_bg_color(settings_tile[i], COL_SURFACE, 0);
        lv_obj_set_style_bg_opa(settings_tile[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(settings_tile[i], 8, 0);
        lv_obj_set_style_border_width(settings_tile[i], 0, 0);
        lv_obj_set_style_pad_all(settings_tile[i], 12, 0);
        lv_obj_remove_flag(settings_tile[i], LV_OBJ_FLAG_SCROLLABLE);

        // アイコン (LVGLシンボル)
        lv_obj_t *icon = lv_label_create(settings_tile[i]);
        lv_label_set_text(icon, tile_icons[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(icon, COL_BLUE, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 16);

        // タイトル
        lv_obj_t *t_lbl = lv_label_create(settings_tile[i]);
        lv_label_set_text(t_lbl, tile_titles[i]);
        lv_obj_set_style_text_font(t_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(t_lbl, COL_WHITE, 0);
        lv_label_set_long_mode(t_lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(t_lbl, tile_w - 24);
        lv_obj_align(t_lbl, LV_ALIGN_CENTER, 0, 10);

        // サブタイトル
        lv_obj_t *sub = lv_label_create(settings_tile[i]);
        lv_label_set_text(sub, tile_subs[i]);
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(sub, COL_MUTED, 0);
        lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(sub, tile_w - 24);
        lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -12);
    }

    // タイルタップ → 各設定サブ画面へ遷移
    // ※ サブ画面は create_scr_settings_home より先に生成しておく必要がある
    lv_obj_t *sub_screens[5] = {
        scr_meas_cond, scr_threshold, scr_history, scr_correction, scr_detail
    };
    for (int i = 0; i < 5; i++) {
        lv_obj_add_event_cb(settings_tile[i], tile_open_cb,
                            LV_EVENT_CLICKED, sub_screens[i]);
    }
}

// ---- scr_main 生成 ----
static void create_scr_main(void)
{
    // 画面オブジェクト (800x480)
    scr_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_main, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr_main, LV_OPA_COVER, 0);
    bare_obj(scr_main);

    // ── header_bar  y=0 h=44 ──────────────────────────────────
    create_header_bar(scr_main);

    // ── matrix_area  x=0 y=44 w=562 h=418 ───────────────────
    matrix_area = lv_obj_create(scr_main);
    lv_obj_set_size(matrix_area, 562, 418);
    lv_obj_set_pos(matrix_area, 0, 44);
    lv_obj_set_style_bg_color(matrix_area, COL_BG, 0);
    lv_obj_set_style_bg_opa(matrix_area, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(matrix_area, 0, 0);
    bare_obj(matrix_area);

    // ch_cell[0..15]: 4×4グリッド 132×98 各セル
    for (int i = 0; i < 16; i++) {
        create_ch_cell(matrix_area, i);
    }

    // ── right_panel  x=562 y=44 w=238 h=418 ─────────────────
    right_panel = lv_obj_create(scr_main);
    lv_obj_set_size(right_panel, 238, 418);
    lv_obj_set_pos(right_panel, 562, 44);
    lv_obj_set_style_bg_color(right_panel, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(right_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(right_panel, 0, 0);
    lv_obj_set_style_border_width(right_panel, 0, 0);
    lv_obj_set_style_pad_all(right_panel, 8, 0);
    lv_obj_remove_flag(right_panel, LV_OBJ_FLAG_SCROLLABLE);

    // -- トレンドチャートタイトル --
    // "楕円距離トレンド  (選択Ch / 直近10回)"
    lbl_trend_title = lv_label_create(right_panel);
    lv_label_set_text(lbl_trend_title, "Trend (Ch / last 10)");
    lv_obj_set_pos(lbl_trend_title, 0, 0);
    lv_obj_set_style_text_color(lbl_trend_title, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_trend_title, &lv_font_montserrat_10, 0);

    // -- chart_trend: LINE  w=220 h=100  直近10回 楕円距離 --
    chart_trend = lv_chart_create(right_panel);
    lv_obj_set_size(chart_trend, 220, 100);
    lv_obj_set_pos(chart_trend, 0, 14);
    lv_chart_set_type(chart_trend, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart_trend, 10);
    lv_obj_set_style_bg_color(chart_trend, COL_BG, 0);
    lv_obj_set_style_bg_opa(chart_trend, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chart_trend, COL_BTN_GRAY, 0);
    lv_obj_set_style_border_width(chart_trend, 1, 0);
    init_trend_chart();

    // -- pie_area: lv_canvas 100x100  OK/NG円グラフ --
    // 実描画は lv_draw_arc を使って task_ui 側で行う
    pie_area = lv_canvas_create(right_panel);
    lv_obj_set_size(pie_area, 100, 100);
    lv_obj_set_pos(pie_area, 60, 118);
    lv_canvas_set_buffer(pie_area, pie_buf, 100, 100, LV_COLOR_FORMAT_NATIVE);

    // -- count_ok カード: 緑背景 100x56 --
    count_ok = lv_obj_create(right_panel);
    lv_obj_set_size(count_ok, 100, 56);
    lv_obj_set_pos(count_ok, 0, 222);
    lv_obj_set_style_bg_color(count_ok, COL_CELL_OK, 0);
    lv_obj_set_style_bg_opa(count_ok, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(count_ok, 6, 0);
    lv_obj_set_style_border_width(count_ok, 0, 0);
    lv_obj_set_style_pad_all(count_ok, 6, 0);
    lv_obj_remove_flag(count_ok, LV_OBJ_FLAG_SCROLLABLE);

    lbl_ok_title = lv_label_create(count_ok);
    lv_label_set_text(lbl_ok_title, "OK");
    lv_obj_set_pos(lbl_ok_title, 0, 0);
    lv_obj_set_style_text_color(lbl_ok_title, COL_ACCENT_OK, 0);
    lv_obj_set_style_text_font(lbl_ok_title, &lv_font_montserrat_12, 0);

    lbl_ok_val = lv_label_create(count_ok);
    lv_label_set_text(lbl_ok_val, "0");
    lv_obj_set_pos(lbl_ok_val, 0, 18);
    lv_obj_set_style_text_color(lbl_ok_val, COL_WHITE, 0);
    lv_obj_set_style_text_font(lbl_ok_val, &lv_font_montserrat_24, 0);

    // -- count_ng カード: 赤背景 100x56 --
    count_ng = lv_obj_create(right_panel);
    lv_obj_set_size(count_ng, 100, 56);
    lv_obj_set_pos(count_ng, 112, 222);
    lv_obj_set_style_bg_color(count_ng, COL_CELL_NG, 0);
    lv_obj_set_style_bg_opa(count_ng, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(count_ng, 6, 0);
    lv_obj_set_style_border_width(count_ng, 0, 0);
    lv_obj_set_style_pad_all(count_ng, 6, 0);
    lv_obj_remove_flag(count_ng, LV_OBJ_FLAG_SCROLLABLE);

    lbl_ng_title = lv_label_create(count_ng);
    lv_label_set_text(lbl_ng_title, "NG");
    lv_obj_set_pos(lbl_ng_title, 0, 0);
    lv_obj_set_style_text_color(lbl_ng_title, COL_ACCENT_NG, 0);
    lv_obj_set_style_text_font(lbl_ng_title, &lv_font_montserrat_12, 0);

    lbl_ng_val = lv_label_create(count_ng);
    lv_label_set_text(lbl_ng_val, "0");
    lv_obj_set_pos(lbl_ng_val, 0, 18);
    lv_obj_set_style_text_color(lbl_ng_val, COL_WHITE, 0);
    lv_obj_set_style_text_font(lbl_ng_val, &lv_font_montserrat_24, 0);

    // -- 楕円パラメータ表示 --
    lbl_ellipse_a = lv_label_create(right_panel);
    lv_label_set_text(lbl_ellipse_a, "a = -.--- V");
    lv_obj_set_pos(lbl_ellipse_a, 0, 288);
    lv_obj_set_style_text_color(lbl_ellipse_a, COL_WHITE, 0);
    lv_obj_set_style_text_font(lbl_ellipse_a, &lv_font_montserrat_12, 0);

    lbl_ellipse_b = lv_label_create(right_panel);
    lv_label_set_text(lbl_ellipse_b, "b = -.--- V");
    lv_obj_set_pos(lbl_ellipse_b, 0, 308);
    lv_obj_set_style_text_color(lbl_ellipse_b, COL_WHITE, 0);
    lv_obj_set_style_text_font(lbl_ellipse_b, &lv_font_montserrat_12, 0);

    // -- btn_counter_reset: リフレッシュアイコン + "Counter Reset" --
    btn_counter_reset = lv_btn_create(right_panel);
    lv_obj_set_size(btn_counter_reset, 220, 34);
    lv_obj_set_pos(btn_counter_reset, 0, 368);
    lv_obj_set_style_bg_color(btn_counter_reset, COL_BTN_GRAY, 0);
    lv_obj_t *lbl_reset = lv_label_create(btn_counter_reset);
    lv_label_set_text(lbl_reset, LV_SYMBOL_REFRESH "  Counter Reset");
    lv_obj_center(lbl_reset);
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_12, 0);
    lv_obj_add_event_cb(btn_counter_reset, btn_counter_reset_cb, LV_EVENT_CLICKED, NULL);

    // ── status_bar  x=0 y=462 w=800 h=18 ────────────────────
    // (44 + 418 = 462, 480 - 462 = 18)
    status_bar = lv_obj_create(scr_main);
    lv_obj_set_size(status_bar, 800, 18);
    lv_obj_set_pos(status_bar, 0, 462);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x0b1120), 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 2, 0);
    lv_obj_remove_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    lbl_datetime = lv_label_create(status_bar);
    lv_label_set_text(lbl_datetime, "2026-01-01 00:00:00");
    lv_obj_set_pos(lbl_datetime, 4, 0);
    lv_obj_set_style_text_color(lbl_datetime, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_datetime, &lv_font_montserrat_10, 0);

    lbl_latest_ng = lv_label_create(status_bar);
    lv_label_set_text(lbl_latest_ng, "Latest NG: --");
    lv_obj_set_pos(lbl_latest_ng, 200, 0);
    lv_obj_set_style_text_color(lbl_latest_ng, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_latest_ng, &lv_font_montserrat_10, 0);

    lv_screen_load(scr_main);

    // 時計タイマー: 即時更新 + 1秒周期
    clock_timer_cb(NULL);
    timer_clock = lv_timer_create(clock_timer_cb, 1000, NULL);
}

// ============================================================
//  main
// ============================================================
int main()
{
    lv_init();

    /*
     * Optional workaround for users who wants UTF-8 console output.
     * If you don't want that behavior can comment them out.
     *
     * Suggested by jinsc123654.
     */
#if LV_TXT_ENC == LV_TXT_ENC_UTF8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif

    int32_t zoom_level = 100;
    bool allow_dpi_override = false;
    bool simulator_mode = true;
    lv_display_t* display = lv_windows_create_display(
        L"LVGL Windows Simulator Display 1",
        800,
        480,
        zoom_level,
        allow_dpi_override,
        simulator_mode);
    if (!display)
    {
        return -1;
    }

    HWND window_handle = lv_windows_get_display_window_handle(display);
    if (!window_handle)
    {
        return -1;
    }

    HICON icon_handle = LoadIconW(
        GetModuleHandleW(NULL),
        MAKEINTRESOURCE(IDI_LVGL_WINDOWS));
    if (icon_handle)
    {
        SendMessageW(
            window_handle,
            WM_SETICON,
            TRUE,
            (LPARAM)icon_handle);
        SendMessageW(
            window_handle,
            WM_SETICON,
            FALSE,
            (LPARAM)icon_handle);
    }

    lv_indev_t* pointer_indev = lv_windows_acquire_pointer_indev(display);
    if (!pointer_indev)
    {
        return -1;
    }

    lv_indev_t* keypad_indev = lv_windows_acquire_keypad_indev(display);
    if (!keypad_indev)
    {
        return -1;
    }

    lv_indev_t* encoder_indev = lv_windows_acquire_encoder_indev(display);
    if (!encoder_indev)
    {
        return -1;
    }

    create_scr_meas_cond();        // 設定サブ画面は settings_home より先に作成
    create_scr_threshold();
    create_scr_threshold_method();
    create_scr_threshold_manual();
    create_scr_history();
    create_scr_correction();
    create_scr_detail();
    create_scr_settings_home();   // タイルコールバックがサブ画面ポインタを参照するため後
    create_scr_main();            // 最後に load するので後で呼ぶ
    create_mem_popup();           // lv_layer_top() に配置するため全画面生成後に呼ぶ

    while (1)
    {
        uint32_t time_till_next = lv_timer_handler();
        lv_delay_ms(time_till_next);
    }

    return 0;
}
