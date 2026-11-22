#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define PROMPTER_MAX_BYTES 8192
#define PROMPTER_BRIGHTNESS_MIN 10
#define PROMPTER_BRIGHTNESS_MAX 100
#define PROMPTER_BRIGHTNESS_DEFAULT 100
/* Product defaults; saved system preferences and per-document styles override them. */
#define PROMPTER_DEFAULT_CONTROLS \
    .speed=100, .font_size=90, .rotation=90, .brightness=PROMPTER_BRIGHTNESS_DEFAULT, \
    .letter_spacing=0, .line_spacing=20, .countdown=0, .loop=false, .auto_next=false, .text_align=0, \
    .margin_left=0, .margin_right=0, .text_color=0xffffff, .mirror=0, \
    .sight_guide=true, .center_highlight=true, .show_info=-1, .page_mode=0, .page_seconds=15, .page_transition=0, .page_paragraphs=1
#define TP_PLAYING 1U
#define TP_SPEED 2U
#define TP_FONT 4U
#define TP_REWIND 8U
#define TP_ROTATION 16U
#define TP_BRIGHTNESS 32U
#define TP_LETTER_SPACING 64U
#define TP_LINE_SPACING 128U
#define TP_COUNTDOWN 256U
#define TP_LOOP 512U
#define TP_TEXT_ALIGN 1024U
#define TP_MARGIN_LEFT 2048U
#define TP_MARGIN_RIGHT 4096U
#define TP_TEXT_COLOR 8192U
#define TP_MIRROR 16384U
#define TP_SIGHT_GUIDE 32768U
#define TP_CENTER_HIGHLIGHT 65536U
#define TP_SHUTTLE 131072U
#define TP_AUTONEXT 262144U
#define TP_BOLD 524288U
#define TP_SHOW_INFO 1048576U
#define TP_PAGE_MODE 2097152U
#define TP_PAGE_SECONDS 4194304U
#define TP_PAGE_TRANSITION 8388608U
#define TP_PAGE_STEP 16777216U
#define TP_SEEK 33554432U
#define TP_PAGE_PARAGRAPHS 67108864U
#define PROMPTER_FONT_MIN 24
#define PROMPTER_FONT_MAX 160

typedef struct {
    uint32_t revision;
    uint32_t script_revision;
    uint32_t rewind_revision;
    size_t length;
    bool playing;
    int shuttle; /* transient signed playback multiplier; 0 means normal playback */
    int speed;
    int font_size;
    int rotation;
    int brightness;
    int letter_spacing;
    int line_spacing;
    int countdown;
    int countdown_remaining_ms;
    bool loop;
    bool auto_next;
    bool bold;
    int show_info; /* -1 inherits device defaults; 0 hides both; 1 shows both */
    int text_align; /* 0 left, 1 center, 2 right */
    int margin_left, margin_right; /* logical screen pixels, 0..240 */
    uint32_t text_color; /* 0xRRGGBB */
    int mirror; /* bit 0 horizontal, bit 1 vertical, text only */
    bool sight_guide, center_highlight;
    int page_mode; /* 0 continuous scroll, 1 timed whole pages */
    int page_seconds, page_transition; /* seconds per page; 0 cut, 1 short slide */
    int page_paragraphs; /* 1 starts a new page after manuscript blank lines */
    int page_step; /* request only: -1 previous page, +1 next page */
    int page_index, page_count, page_remaining_ms;
    uint32_t page_changed_ms;
    int seek_position; /* transient: pixels in scroll mode, page index in page mode */
    uint32_t seek_revision;
    uint32_t loop_count;
    uint64_t playback_elapsed_ms; /* Active playback time, excluding countdown/pause. */
    bool playback_finished;
    bool stopped; /* explicit manual/automatic stop, distinct from pause or initial entry */
    int preview_scroll;
    uint32_t preview_revision;
    uint32_t preview_sample_ms;
} prompter_status_t;

/* A positive mode selection also clears its counterpart. Legacy data with
 * both enabled keeps the previous effective behavior: repeat this document. */
static inline void prompter_normalize_playback(unsigned *mask,prompter_status_t *values)
{
    if((*mask&TP_LOOP)&&values->loop){*mask|=TP_AUTONEXT;values->auto_next=false;}
    else if((*mask&TP_AUTONEXT)&&values->auto_next){*mask|=TP_LOOP;values->loop=false;}
}

esp_err_t prompter_state_init(void);
esp_err_t prompter_set_script(const char *text, size_t length);
esp_err_t prompter_control(unsigned mask, bool playing, int speed, int font_size, int rotation);
esp_err_t prompter_control_full(unsigned mask, bool playing, int speed, int font_size, int rotation, int brightness);
esp_err_t prompter_control_extended(unsigned mask, bool playing, int speed, int font_size,
    int rotation, int brightness, int letter_spacing, int line_spacing, int countdown);
void prompter_snapshot(prompter_status_t *status, char *text);
esp_err_t prompter_control_apply(unsigned mask, const prompter_status_t *values);
esp_err_t prompter_control_validate(unsigned mask, const prompter_status_t *values);
esp_err_t prompter_control_runtime(unsigned mask,const prompter_status_t *values,bool persist);
bool prompter_text_valid(const char *text,size_t length);
esp_err_t prompter_store_defaults(const prompter_status_t *values);
/* Called when the last text line has left the screen. Never writes NVS. */
bool prompter_playback_finished(uint32_t script_revision, uint32_t rewind_revision);
/* Automatic reverse boundary stop. Resets progress without pretending the text ended. */
bool prompter_playback_stopped(uint32_t script_revision, uint32_t rewind_revision);
int prompter_status_json(char *buffer, size_t size, const prompter_status_t *state);
uint32_t prompter_boot_id(void);
void prompter_preview_position(int scroll, uint32_t revision);
/* Publish pagination only for the layout that was actually rendered. */
bool prompter_page_layout(const prompter_status_t *layout, int pages, int index);
