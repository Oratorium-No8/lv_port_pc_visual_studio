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

    // mem_selector: 168x30  中央配置
    // (800 - 168) / 2 = 316
    mem_selector = lv_obj_create(header_bar);
    lv_obj_set_size(mem_selector, 168, 30);
    lv_obj_set_pos(mem_selector, 316, 7);
    lv_obj_set_style_bg_color(mem_selector, COL_SURFACE, 0);
    lv_obj_set_style_bg_opa(mem_selector, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mem_selector, 6, 0);
    bare_obj(mem_selector);

    // btn_mem_prev: 24x22  "◀"
    btn_mem_prev = lv_btn_create(mem_selector);
    lv_obj_set_size(btn_mem_prev, 24, 22);
    lv_obj_set_pos(btn_mem_prev, 2, 4);
    lv_obj_set_style_bg_color(btn_mem_prev, COL_BTN_GRAY, 0);
    lv_obj_t *lbl_prev = lv_label_create(btn_mem_prev);
    lv_label_set_text(lbl_prev, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_prev);

    // lbl_mem_no: font=bold-16  color=COL_BLUE  例: "001"
    lbl_mem_no = lv_label_create(mem_selector);
    lv_label_set_text(lbl_mem_no, "001");
    lv_obj_set_pos(lbl_mem_no, 30, 7);
    lv_obj_set_style_text_color(lbl_mem_no, COL_BLUE, 0);
    lv_obj_set_style_text_font(lbl_mem_no, &lv_font_montserrat_16, 0);

    // btn_mem_next: 24x22  "▶"
    btn_mem_next = lv_btn_create(mem_selector);
    lv_obj_set_size(btn_mem_next, 24, 22);
    lv_obj_set_pos(btn_mem_next, 72, 4);
    lv_obj_set_style_bg_color(btn_mem_next, COL_BTN_GRAY, 0);
    lv_obj_t *lbl_next = lv_label_create(btn_mem_next);
    lv_label_set_text(lbl_next, LV_SYMBOL_RIGHT);
    lv_obj_center(lbl_next);

    // lbl_mem_name: font=10  color=COL_MUTED  例: "M1: 未設定"
    lbl_mem_name = lv_label_create(mem_selector);
    lv_label_set_text(lbl_mem_name, "M1: (unnamed)");
    lv_obj_set_pos(lbl_mem_name, 100, 10);
    lv_obj_set_style_text_color(lbl_mem_name, COL_MUTED, 0);
    lv_obj_set_style_text_font(lbl_mem_name, &lv_font_montserrat_10, 0);

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

static void create_scr_correction(void)
{
    lv_obj_t *c = make_settings_subscr(&scr_correction, "Correction");
    lv_obj_t *ph = lv_label_create(c);
    lv_label_set_text(ph, "Correction\n(Correction coeff.)");
    lv_obj_set_style_text_color(ph, COL_MUTED, 0);
    lv_obj_set_style_text_font(ph, &lv_font_montserrat_16, 0);
    lv_label_set_long_mode(ph, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(ph, 400);
    lv_obj_center(ph);
}

static void create_scr_detail(void)
{
    lv_obj_t *c = make_settings_subscr(&scr_detail, "Detail Settings");
    lv_obj_t *ph = lv_label_create(c);
    lv_label_set_text(ph, "Detail Settings\n(Language / Clock / I/O)");
    lv_obj_set_style_text_color(ph, COL_MUTED, 0);
    lv_obj_set_style_text_font(ph, &lv_font_montserrat_16, 0);
    lv_label_set_long_mode(ph, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(ph, 400);
    lv_obj_center(ph);
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
    create_scr_history();
    create_scr_correction();
    create_scr_detail();
    create_scr_settings_home();   // タイルコールバックがサブ画面ポインタを参照するため後
    create_scr_main();            // 最後に load するので後で呼ぶ

    while (1)
    {
        uint32_t time_till_next = lv_timer_handler();
        lv_delay_ms(time_till_next);
    }

    return 0;
}
