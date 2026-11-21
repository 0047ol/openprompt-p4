#include "prompter_state.h"
#include "crash_log.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "script_library.h"
#include "app_control.h"
#include "device_settings.h"

static SemaphoreHandle_t s_mutex;
static nvs_handle_t s_display_settings;
static const char *TAG = "tp_state";
static char s_text[PROMPTER_MAX_BYTES + 1];
static int64_t s_countdown_deadline;
static int64_t s_playback_clock_us;
static uint64_t s_playback_elapsed_us;
static bool s_playback_started;
static uint32_t s_boot_id;
uint32_t prompter_boot_id(void) { return s_boot_id; }
static prompter_status_t s_state = {.revision = 1, .script_revision = 1,
                                   PROMPTER_DEFAULT_CONTROLS};

/* Fixed-width, versioned NVS record. Keep all preferences in one value so a
 * combined request cannot restore a mixture of old and new preferences. */
typedef struct {
    uint16_t version;
    uint16_t speed;
    uint16_t font;
    uint16_t rotation;
    uint8_t brightness;
    uint8_t reserved[3];
} saved_controls_t;

typedef struct {
    saved_controls_t base;
    uint16_t letter_spacing;
    uint16_t line_spacing;
    uint16_t countdown;
    uint16_t reserved;
} saved_controls_v2_t;

typedef struct {
    saved_controls_t base;
    uint16_t letter_spacing, line_spacing, countdown, reserved;
    uint32_t text_color;
    uint16_t margin_left, margin_right;
    uint8_t loop, text_align, mirror, sight_guide, center_highlight;
    uint8_t padding[3];
} saved_controls_v3_t;

static saved_controls_v3_t saved_controls(const prompter_status_t *state)
{
    saved_controls_v3_t saved = {0};
    saved.base.version = 3;
    saved.base.speed = state->speed;
    saved.base.font = state->font_size;
    saved.base.rotation = state->rotation;
    saved.base.brightness = state->brightness;
    saved.letter_spacing = state->letter_spacing;
    saved.line_spacing = state->line_spacing;
    saved.countdown = state->countdown;
    saved.text_color = state->text_color;
    saved.margin_left = state->margin_left;
    saved.margin_right = state->margin_right;
    saved.loop = state->loop;
    saved.text_align = state->text_align;
    saved.mirror = state->mirror;
    saved.sight_guide = state->sight_guide;
    saved.center_highlight = state->center_highlight;
    saved.padding[0] = state->auto_next;
    saved.padding[1] = state->bold;
    saved.padding[2] = (uint8_t)(state->show_info + 1);
    return saved;
}

static esp_err_t save_controls(const saved_controls_v3_t *saved)
{
    esp_err_t err = nvs_set_blob(s_display_settings, "controls", saved, sizeof(*saved));
    return err == ESP_OK ? nvs_commit(s_display_settings) : err;
}
esp_err_t prompter_store_defaults(const prompter_status_t *values)
{
    saved_controls_v3_t saved=saved_controls(values);
    return save_controls(&saved);
}

static bool valid_utf8(const unsigned char *p, size_t n)
{
    for (size_t i = 0; i < n;) {
        unsigned c = p[i++];
        if (c == 0) return false;
        if (c < 0x80) continue;
        unsigned value, minimum, more;
        if (c >= 0xc2 && c <= 0xdf) { value = c & 31; minimum = 0x80; more = 1; }
        else if (c >= 0xe0 && c <= 0xef) { value = c & 15; minimum = 0x800; more = 2; }
        else if (c >= 0xf0 && c <= 0xf4) { value = c & 7; minimum = 0x10000; more = 3; }
        else return false;
        if (n - i < more) return false;
        while (more--) {
            c = p[i++];
            if ((c & 0xc0) != 0x80) return false;
            value = (value << 6) | (c & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}

esp_err_t prompter_state_init(void)
{
    s_boot_id=esp_random();
    /* Initialize before display startup; never erase Wi-Fi or other namespaces. */
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) return err;
    err = nvs_open("tp_display", NVS_READWRITE, &s_display_settings);
    if (err != ESP_OK) return err;
    saved_controls_v3_t saved = saved_controls(&s_state);
    size_t size = sizeof(saved);
    err = nvs_get_blob(s_display_settings, "controls", &saved, &size);
    bool migrate = err == ESP_ERR_NVS_NOT_FOUND || err == ESP_ERR_NVS_INVALID_LENGTH ||
                   err == ESP_ERR_NVS_TYPE_MISMATCH ||
                   (err == ESP_OK && !((size == sizeof(saved) && saved.base.version == 3) ||
                     (size == sizeof(saved_controls_v2_t) && saved.base.version == 2) ||
                     (size == sizeof(saved_controls_t) && saved.base.version == 1)));
    bool upgrade = err == ESP_OK && saved.base.version < 3;
    if (migrate) {
        saved = saved_controls(&s_state);
        /* Preserve brightness from firmware which stored this preference alone. */
        err = nvs_get_u8(s_display_settings, "brightness", &saved.base.brightness);
        if (err == ESP_ERR_NVS_NOT_FOUND || err == ESP_ERR_NVS_TYPE_MISMATCH) err = ESP_OK;
    }
    if (err != ESP_OK) {
        nvs_close(s_display_settings);
        return err;
    }
    if (saved.base.speed >= 10 && saved.base.speed <= 180) s_state.speed = saved.base.speed;
    if (saved.base.font >= PROMPTER_FONT_MIN && saved.base.font <= PROMPTER_FONT_MAX) s_state.font_size = saved.base.font;
    if (saved.base.rotation == 0 || saved.base.rotation == 90 || saved.base.rotation == 180 || saved.base.rotation == 270)
        s_state.rotation = saved.base.rotation;
    s_state.brightness = saved.base.brightness < PROMPTER_BRIGHTNESS_MIN ? PROMPTER_BRIGHTNESS_MIN :
                         saved.base.brightness > PROMPTER_BRIGHTNESS_MAX ? PROMPTER_BRIGHTNESS_DEFAULT : saved.base.brightness;
    if (saved.letter_spacing <= 32) s_state.letter_spacing = saved.letter_spacing;
    if (saved.line_spacing <= 80) s_state.line_spacing = saved.line_spacing;
    if (saved.countdown <= 30) s_state.countdown = saved.countdown;
    if (saved.text_color <= 0xffffff) s_state.text_color = saved.text_color;
    if (saved.margin_left <= 240) s_state.margin_left = saved.margin_left;
    if (saved.margin_right <= 240) s_state.margin_right = saved.margin_right;
    if (saved.text_align <= 2) s_state.text_align = saved.text_align;
    if (saved.mirror <= 3) s_state.mirror = saved.mirror;
    s_state.loop = saved.loop == 1;
    s_state.sight_guide = saved.sight_guide == 1;
    s_state.center_highlight = saved.center_highlight == 1;
    s_state.auto_next = !s_state.loop && saved.padding[0] == 1;
    s_state.bold = saved.padding[1] == 1;
    s_state.show_info = saved.padding[2] <= 2 ? (int)saved.padding[2] - 1 : -1;
    saved_controls_v3_t normalized = saved_controls(&s_state);
    if (migrate || upgrade || memcmp(&saved, &normalized, sizeof(saved))) {
        err = save_controls(&normalized);
        if (err != ESP_OK) ESP_LOGW(TAG, "Settings repair not persisted: %s", esp_err_to_name(err));
    }
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) nvs_close(s_display_settings);
    ESP_LOGI(TAG, "SETTINGS restored speed=%d font=%d rotation=%d brightness=%d%%",
             s_state.speed, s_state.font_size, s_state.rotation, s_state.brightness);
    return s_mutex ? ESP_OK : ESP_ERR_NO_MEM;
}

bool prompter_text_valid(const char *text,size_t length)
{
    return text && length<=PROMPTER_MAX_BYTES && valid_utf8((const unsigned char *)text,length);
}

esp_err_t prompter_set_script(const char *text, size_t length)
{
    if(teleprompter_power_blocked())return ESP_ERR_INVALID_STATE;
    if (!text || length > PROMPTER_MAX_BYTES || !valid_utf8((const unsigned char *)text, length))
        return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(s_text, text, length);
    s_text[length] = 0;
    s_state.length = length;
    s_state.playing = false;
    s_playback_elapsed_us=0;s_state.playback_elapsed_ms=0;s_playback_clock_us=esp_timer_get_time();
    s_state.shuttle = 0;
    s_playback_started = false;
    s_countdown_deadline = 0;
    s_state.countdown_remaining_ms = 0;
    s_state.loop_count = 0;
    s_state.playback_finished = false;
    s_state.stopped = false;
    s_state.preview_scroll = 0;
    ++s_state.revision;
    ++s_state.script_revision;
    ++s_state.rewind_revision;
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t prompter_control(unsigned mask, bool playing, int speed, int font_size, int rotation)
{
    return prompter_control_full(mask, playing, speed, font_size, rotation, PROMPTER_BRIGHTNESS_DEFAULT);
}

esp_err_t prompter_control_full(unsigned mask, bool playing, int speed, int font_size, int rotation, int brightness)
{
    return prompter_control_extended(mask, playing, speed, font_size, rotation, brightness, 0, 12, 0);
}

/* Caller holds the state mutex. The device clock, not the browser, owns playback. */
static void update_countdown(void)
{
    int64_t now=esp_timer_get_time();
    int64_t from=s_playback_clock_us;
    if(s_countdown_deadline>from)from=s_countdown_deadline;
    if(s_playback_clock_us&&(s_state.playing||s_countdown_deadline)&&now>from)
        s_playback_elapsed_us+=(uint64_t)(now-from);
    s_playback_clock_us=now;s_state.playback_elapsed_ms=s_playback_elapsed_us/1000;
    if (!s_countdown_deadline) return;
    int64_t remaining = s_countdown_deadline - now;
    s_state.countdown_remaining_ms = remaining > 0 ? (int)((remaining + 999) / 1000) : 0;
    if (remaining <= 0) {
        s_countdown_deadline = 0;
        s_state.playing = s_state.length > 0;
        s_playback_started = s_state.playing;
        ++s_state.revision;
    }
}

esp_err_t prompter_control_extended(unsigned mask, bool playing, int speed, int font_size,
    int rotation, int brightness, int letter_spacing, int line_spacing, int countdown)
{
    prompter_status_t values = {.playing=playing,.speed=speed,.font_size=font_size,.rotation=rotation,
        .brightness=brightness,.letter_spacing=letter_spacing,.line_spacing=line_spacing,.countdown=countdown};
    return prompter_control_apply(mask,&values);
}

esp_err_t prompter_control_apply(unsigned mask,const prompter_status_t *v)
{
    return script_library_control(mask,v);
}

esp_err_t prompter_control_validate(unsigned mask, const prompter_status_t *v)
{
    if (!v) return ESP_ERR_INVALID_ARG;
    int speed=v->speed, font_size=v->font_size, rotation=v->rotation, brightness=v->brightness;
    int letter_spacing=v->letter_spacing, line_spacing=v->line_spacing, countdown=v->countdown;
    if (!mask || (mask & ~(TP_PLAYING | TP_SPEED | TP_FONT | TP_REWIND | TP_ROTATION | TP_BRIGHTNESS |
                           TP_LETTER_SPACING | TP_LINE_SPACING | TP_COUNTDOWN | TP_LOOP | TP_TEXT_ALIGN |
                           TP_MARGIN_LEFT | TP_MARGIN_RIGHT | TP_TEXT_COLOR | TP_MIRROR | TP_SIGHT_GUIDE | TP_CENTER_HIGHLIGHT | TP_SHUTTLE | TP_AUTONEXT | TP_BOLD | TP_SHOW_INFO)) ||
        ((mask & TP_SHOW_INFO) && (v->show_info < -1 || v->show_info > 1)) ||
        ((mask & TP_SHUTTLE) && (v->shuttle < -8 || v->shuttle > 8 || !v->shuttle)) ||
        ((mask & TP_TEXT_ALIGN) && (v->text_align < 0 || v->text_align > 2)) ||
        ((mask & TP_MARGIN_LEFT) && (v->margin_left < 0 || v->margin_left > 240)) ||
        ((mask & TP_MARGIN_RIGHT) && (v->margin_right < 0 || v->margin_right > 240)) ||
        ((mask & TP_TEXT_COLOR) && v->text_color > 0xffffff) ||
        ((mask & TP_MIRROR) && (v->mirror < 0 || v->mirror > 3)) ||
        ((mask & TP_BRIGHTNESS) && (brightness < PROMPTER_BRIGHTNESS_MIN || brightness > PROMPTER_BRIGHTNESS_MAX)) ||
        ((mask & TP_SPEED) && (speed < 10 || speed > 180)) ||
        ((mask & TP_FONT) && (font_size < PROMPTER_FONT_MIN || font_size > PROMPTER_FONT_MAX)) ||
        ((mask & TP_LETTER_SPACING) && (letter_spacing < 0 || letter_spacing > 32)) ||
        ((mask & TP_LINE_SPACING) && (line_spacing < 0 || line_spacing > 80)) ||
        ((mask & TP_COUNTDOWN) && (countdown < 0 || countdown > 30)) ||
        ((mask & TP_ROTATION) && rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270))
        return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

esp_err_t prompter_control_runtime(unsigned mask,const prompter_status_t *v,bool persist)
{
    if(!v)return ESP_ERR_INVALID_ARG;
    prompter_status_t normalized=*v;
    prompter_normalize_playback(&mask,&normalized);v=&normalized;
    if(prompter_control_validate(mask,v)!=ESP_OK)return ESP_ERR_INVALID_ARG;
    bool playing=v->playing;
    int speed=v->speed,font_size=v->font_size,rotation=v->rotation,brightness=v->brightness;
    int letter_spacing=v->letter_spacing,line_spacing=v->line_spacing,countdown=v->countdown;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    update_countdown();
    /* Commit preferences before publishing any field of a combined request.
     * Repeated values and playback/rewind alone do not write flash. */
    if ((mask & TP_SHUTTLE) && s_countdown_deadline) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    saved_controls_v3_t previous = saved_controls(&s_state);
    saved_controls_v3_t next = previous;
    if (mask & TP_SPEED) next.base.speed = speed;
    if (mask & TP_FONT) next.base.font = font_size;
    if (mask & TP_ROTATION) next.base.rotation = rotation;
    if (mask & TP_BRIGHTNESS) next.base.brightness = brightness;
    if (mask & TP_LETTER_SPACING) next.letter_spacing = letter_spacing;
    if (mask & TP_LINE_SPACING) next.line_spacing = line_spacing;
    if (mask & TP_COUNTDOWN) next.countdown = countdown;
    if (mask & TP_LOOP) next.loop = v->loop;
    if (mask & TP_AUTONEXT) next.padding[0] = v->auto_next;
    if (mask & TP_BOLD) next.padding[1] = v->bold;
    if (mask & TP_SHOW_INFO) next.padding[2] = (uint8_t)(v->show_info + 1);
    if (mask & TP_TEXT_ALIGN) next.text_align = v->text_align;
    if (mask & TP_MARGIN_LEFT) next.margin_left = v->margin_left;
    if (mask & TP_MARGIN_RIGHT) next.margin_right = v->margin_right;
    if (mask & TP_TEXT_COLOR) next.text_color = v->text_color;
    if (mask & TP_MIRROR) next.mirror = v->mirror;
    if (mask & TP_SIGHT_GUIDE) next.sight_guide = v->sight_guide;
    if (mask & TP_CENTER_HIGHLIGHT) next.center_highlight = v->center_highlight;
    if (persist && memcmp(&previous, &next, sizeof(next))) {
        esp_err_t err = save_controls(&next);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Settings save failed: %s", esp_err_to_name(err));
            xSemaphoreGive(s_mutex);
            return err;
        }
        ESP_LOGI(TAG, "SETTINGS saved speed=%u font=%u rotation=%u brightness=%u%%",
                 next.base.speed, next.base.font, next.base.rotation, next.base.brightness);
    }
    if (mask & TP_SPEED) s_state.speed = speed;
    if (mask & TP_FONT) { s_state.font_size = font_size; ++s_state.rewind_revision; }
    if (mask & TP_ROTATION) { s_state.rotation = rotation; ++s_state.rewind_revision; }
    if (mask & TP_BRIGHTNESS) s_state.brightness = brightness;
    if (mask & TP_LETTER_SPACING) { s_state.letter_spacing = letter_spacing; ++s_state.rewind_revision; }
    if (mask & TP_LINE_SPACING) { s_state.line_spacing = line_spacing; ++s_state.rewind_revision; }
    if (mask & TP_COUNTDOWN) s_state.countdown = countdown;
    if (mask & TP_LOOP) s_state.loop = v->loop;
    if (mask & TP_AUTONEXT) s_state.auto_next = v->auto_next;
    if (mask & TP_BOLD) s_state.bold = v->bold;
    if (mask & TP_SHOW_INFO) s_state.show_info = v->show_info;
    if (mask & TP_TEXT_ALIGN) s_state.text_align = v->text_align;
    if (mask & TP_MARGIN_LEFT) s_state.margin_left = v->margin_left;
    if (mask & TP_MARGIN_RIGHT) s_state.margin_right = v->margin_right;
    if (mask & TP_TEXT_COLOR) s_state.text_color = v->text_color;
    if (mask & TP_MIRROR) s_state.mirror = v->mirror;
    if (mask & TP_SIGHT_GUIDE) s_state.sight_guide = v->sight_guide;
    if (mask & TP_CENTER_HIGHLIGHT) s_state.center_highlight = v->center_highlight;
    if (mask & (TP_TEXT_ALIGN | TP_MARGIN_LEFT | TP_MARGIN_RIGHT | TP_MIRROR)) ++s_state.rewind_revision;
    if (mask & TP_REWIND) {
        s_playback_elapsed_us=0;s_state.playback_elapsed_ms=0;
        s_playback_started = false;
        s_state.shuttle = 0;
        s_state.playback_finished = false;
        s_state.stopped = true;
        s_state.preview_scroll = 0;
        s_state.loop_count = 0;
        ++s_state.rewind_revision;
        s_countdown_deadline = 0;
        s_state.countdown_remaining_ms = 0;
        s_state.playing = false;
    }
    if (mask & TP_PLAYING) {
        bool was_shuttling = s_state.shuttle != 0;
        s_state.shuttle = 0;
        if (playing && s_state.length) s_state.stopped = false;
        if (!playing || !s_state.length) {
            s_state.playing = false;
            s_countdown_deadline = 0;
            s_state.countdown_remaining_ms = 0;
        } else if ((!s_state.playing || was_shuttling) && !s_countdown_deadline) {
            if(s_state.playback_finished) { ++s_state.rewind_revision; s_state.playback_finished=false; s_playback_started=false; }
            if (s_state.countdown && !s_playback_started) {
                s_countdown_deadline = esp_timer_get_time() + (int64_t)s_state.countdown * 1000000;
                s_state.countdown_remaining_ms = s_state.countdown * 1000;
            } else { s_state.playing = true; s_playback_started = true; }
        }
    }
    if ((mask & TP_SHUTTLE) && s_state.length) {
        s_state.shuttle = v->shuttle;
        s_state.playing = true;
        s_playback_started = true;
        s_state.playback_finished = false;
        s_state.stopped = false;
        s_countdown_deadline = 0;
        s_state.countdown_remaining_ms = 0;
    }
    ++s_state.revision;
    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

static void reset_after_automatic_stop(bool finished)
{
    s_playback_elapsed_us=0;s_state.playback_elapsed_ms=0;s_playback_clock_us=esp_timer_get_time();
    s_state.playing=false;
    s_state.shuttle=0;
    s_state.playback_finished=finished;
    s_state.stopped=true;
    s_state.preview_scroll=0;
    s_state.preview_sample_ms=(uint32_t)(esp_timer_get_time()/1000);
    s_state.countdown_remaining_ms=0;
    s_countdown_deadline=0;
    s_playback_started=false;
    ++s_state.rewind_revision;
}

bool prompter_playback_stopped(uint32_t script_revision, uint32_t rewind_revision)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    bool current=s_state.playing&&s_state.script_revision==script_revision&&s_state.rewind_revision==rewind_revision;
    if(current){reset_after_automatic_stop(false);++s_state.revision;}
    xSemaphoreGive(s_mutex);
    return current;
}

bool prompter_playback_finished(uint32_t script_revision, uint32_t rewind_revision)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    bool repeat=false;
    if (s_state.playing && s_state.script_revision==script_revision && s_state.rewind_revision==rewind_revision) {
        repeat=s_state.loop;
        if(repeat) ++s_state.loop_count;
        else reset_after_automatic_stop(true);
        ++s_state.revision;
    }
    xSemaphoreGive(s_mutex);
    return repeat;
}

int prompter_status_json(char *buffer, size_t size, const prompter_status_t *s)
{
    return snprintf(buffer,size,"{\"boot\":%lu,\"revision\":%lu,\"scriptRevision\":%lu,\"length\":%u,\"playing\":%s,\"speed\":%d,\"font\":%d,"
        "\"rotation\":%d,\"brightness\":%d,\"letterSpacing\":%d,\"lineSpacing\":%d,\"countdown\":%d,\"countdownRemainingMs\":%d,"
        "\"loop\":%s,\"autoNext\":%s,\"bold\":%s,\"showInfo\":%d,\"textAlign\":%d,\"marginLeft\":%d,\"marginRight\":%d,\"textColor\":%lu,\"mirror\":%d,"
        "\"sightGuide\":%s,\"centerHighlight\":%s,\"loopCount\":%lu,\"playbackElapsedMs\":%llu,\"playbackFinished\":%s,\"stopped\":%s,\"scroll\":%d,\"previewRevision\":%lu,\"scrollSampleMs\":%lu,\"rewindRevision\":%lu,\"shuttle\":%d,\"lifecycle\":\"%s\",\"powerControl\":true,\"powerProgress\":%u,\"crashPending\":%s,\"crashCount\":%u,\"lastCrashId\":%u,\"settingsRevision\":%lu,\"libraryRevision\":%lu}",
        (unsigned long)s_boot_id,(unsigned long)s->revision,(unsigned long)s->script_revision,(unsigned)s->length,s->playing?"true":"false",s->speed,s->font_size,
        s->rotation,s->brightness,s->letter_spacing,s->line_spacing,s->countdown,s->countdown_remaining_ms,
        s->loop?"true":"false",s->auto_next?"true":"false",s->bold?"true":"false",s->show_info,s->text_align,s->margin_left,s->margin_right,(unsigned long)s->text_color,s->mirror,
        s->sight_guide?"true":"false",s->center_highlight?"true":"false",(unsigned long)s->loop_count,(unsigned long long)s->playback_elapsed_ms,
        s->playback_finished?"true":"false",s->stopped?"true":"false",s->preview_scroll,(unsigned long)s->preview_revision,
        (unsigned long)s->preview_sample_ms,(unsigned long)s->rewind_revision,s->shuttle,teleprompter_lifecycle(),teleprompter_power_progress(),
        crash_log_pending()?"true":"false",crash_log_count(),crash_log_last_id(),
        (unsigned long)device_settings_revision(),(unsigned long)script_library_revision());
}

void prompter_preview_position(int scroll, uint32_t revision)
{
    xSemaphoreTake(s_mutex,portMAX_DELAY);
    s_state.preview_scroll=s_state.stopped?0:scroll;s_state.preview_revision=revision;
    s_state.preview_sample_ms=(uint32_t)(esp_timer_get_time()/1000);
    xSemaphoreGive(s_mutex);
}

void prompter_snapshot(prompter_status_t *status, char *text)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    update_countdown();
    if (status) *status = s_state;
    if (text) memcpy(text, s_text, s_state.length + 1);
    xSemaphoreGive(s_mutex);
}
