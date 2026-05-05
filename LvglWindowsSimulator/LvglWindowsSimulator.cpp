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

// ---- 前方宣言 ----
static void btn_start_cb(lv_event_t *e);
static void btn_stop_cb(lv_event_t *e);
static void btn_counter_reset_cb(lv_event_t *e);

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
    lv_obj_add_event_cb(btn_start, btn_start_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_stop,  btn_stop_cb,  LV_EVENT_CLICKED, NULL);
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
static uint32_t    dummy_tick  = 0;
static int32_t     ok_count    = 0;
static int32_t     ng_count    = 0;

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

        ch_state_t state = (dist > 1.0f) ? CH_NG : CH_OK;
        update_ch_cell(ch, state, vx, vy);

        if (state == CH_OK) ok_count++;
        else                ng_count++;
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
}

// カウンターリセットボタン: カウントのみクリア (タイマーは継続)
static void btn_counter_reset_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ok_count = 0;
    ng_count = 0;
    lv_label_set_text(lbl_ok_val, "0");
    lv_label_set_text(lbl_ng_val, "0");
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

    create_scr_main();

    while (1)
    {
        uint32_t time_till_next = lv_timer_handler();
        lv_delay_ms(time_till_next);
    }

    return 0;
}
