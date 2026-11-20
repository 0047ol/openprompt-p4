#include "display_port.h"
#include "display_damage.h"
#include "display_cache.h"
#include "ui_motion_settings.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "esp_attr.h"
#include "esp_async_color_convert.h"
#include "tp_dma2d_prepared.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/ppa.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "board_display.h"
#include "prompter_state.h"

#define LCD_H_RES 800
#define LCD_V_RES 1280
#define LCD_BITS_PER_PIXEL 16
_Static_assert(LCD_H_RES==TP_DISPLAY_CACHE_WIDTH&&LCD_V_RES==TP_DISPLAY_CACHE_HEIGHT,
               "Chrome cache-line coverage must match the physical panel");
#define LVGL_BUFFER_LINES 64
#define LVGL_MAX_WIDTH 1280
#define LCD_BACKLIGHT_GPIO 20
/* esp32p4_10.pdf: LCD_RST is GPIO5, routed to H1 pin 36. */
#define LCD_RESET_GPIO 5
/* Conservative brightness while investigating resets during panel startup. */
#define LCD_BACKLIGHT_MAX_DUTY 1023
/* A short looping manuscript may repeat dozens of pieces in a single frame.
 * Drain bounded batches before submitting more, leaving room for an ISR that
 * has signalled completion but has not yet recycled its transaction object. */
#define COPY_DMA_BACKLOG 16
/* A portrait frame can contain seven tile pieces, two highlight splits and
 * four guide edges. Keep these together so look-ahead can overlap the body
 * copies. Leave two transactions free for callback recycling and headroom. */
#define COPY_DMA_BATCH 14
_Static_assert(COPY_DMA_BATCH<=COPY_DMA_BACKLOG-2,"DMA queue recycling headroom");

static const char *TAG = "tp_display";
static lv_display_t *s_display;
static esp_lcd_panel_handle_t s_panel;
static bool s_logged_first_flush;
static volatile uint32_t s_refresh_count;
static int64_t s_first_flush_us;
static int64_t s_last_backlight_step_us;
static uint32_t s_backlight_duty;
static bool s_standby;
static bool s_backlight_initialized;
static uint16_t *s_panel_framebuffer;
static uint16_t *s_scan_framebuffer;
static uint16_t *s_chrome;
static async_color_convert_handle_t s_copy_dma;
static SemaphoreHandle_t s_copy_done;
static unsigned s_pending_copies;
static unsigned s_copy_peak,s_copy_drains;
static uint64_t s_dma_wait_us;
static uint32_t s_dma_wait_max_us;
static TaskHandle_t volatile s_present_task;
static volatile bool s_waiting_frame;
static portMUX_TYPE s_publish_lock=portMUX_INITIALIZER_UNLOCKED;
static tp_display_damage_set_t s_chrome_damage[2],s_dirty_areas,s_frame_chrome_damage;
static tp_display_cache_t s_dirty_cache;
static uint64_t s_chrome_bytes;
static uint64_t s_cache_bytes;
static uint32_t s_cache_max_us;
static unsigned s_back_index;
static volatile uint32_t s_frame_complete_count;
static uint64_t s_copy_us, s_sync_us;
static uint32_t s_presented_frames;
static int64_t s_stats_started;
static ppa_client_handle_t s_rotate;
static uint16_t *s_slide_background;
static size_t s_slide_capacity;
static int s_slide_x,s_slide_y,s_slide_width,s_slide_height,s_slide_distance;
static int s_dirty_left = LCD_H_RES, s_dirty_top = LCD_V_RES;
static int s_dirty_right = -1, s_dirty_bottom = -1;
static QueueHandle_t s_capture_queue;
typedef struct {
    tp_display_capture_t image;
    SemaphoreHandle_t done;
    atomic_uint references;
    atomic_bool cancelled;
    esp_err_t result;
} capture_job_t;

static void capture_release(capture_job_t *job)
{
    if(atomic_fetch_sub(&job->references,1)!=1)return;
    free(job->image.pixels);
    vSemaphoreDelete(job->done);
    free(job);
}

esp_err_t display_port_capture(tp_display_capture_t *capture,uint32_t timeout_ms)
{
    if(!capture)return ESP_ERR_INVALID_ARG;
    memset(capture,0,sizeof(*capture));
    if(!s_display||!s_capture_queue)return ESP_ERR_INVALID_STATE;
    capture_job_t *job=calloc(1,sizeof(*job));
    if(!job)return ESP_ERR_NO_MEM;
    job->done=xSemaphoreCreateBinary();
    job->image.pixels=heap_caps_malloc(LCD_H_RES*LCD_V_RES*sizeof(uint16_t),MALLOC_CAP_SPIRAM);
    if(!job->done||!job->image.pixels){
        if(job->done)vSemaphoreDelete(job->done);
        free(job->image.pixels);free(job);return ESP_ERR_NO_MEM;
    }
    atomic_init(&job->references,2);atomic_init(&job->cancelled,false);
    if(xQueueSend(s_capture_queue,&job,0)!=pdTRUE){capture_release(job);capture_release(job);return ESP_ERR_INVALID_STATE;}
    esp_err_t result=ESP_ERR_TIMEOUT;
    if(xSemaphoreTake(job->done,pdMS_TO_TICKS(timeout_ms))==pdTRUE){
        result=job->result;
        if(result==ESP_OK){*capture=job->image;job->image.pixels=NULL;}
    }else atomic_store(&job->cancelled,true);
    /* The UI owns a separate reference: a timeout never leaves a dangling
     * destination pointer or semaphore, including cancellation during copy. */
    capture_release(job);
    return result;
}

static bool copy_complete(async_color_convert_handle_t handle,
                          async_color_convert_event_data_t *event, void *context)
{
    (void)handle; (void)event; (void)context;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(s_copy_done, &wake);
    return wake == pdTRUE;
}

static void copy_wait(void)
{
    if(!s_pending_copies)return;
    int64_t started=esp_timer_get_time();
    while (s_pending_copies) {
        ESP_ERROR_CHECK(xSemaphoreTake(s_copy_done, pdMS_TO_TICKS(1000)) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT);
        --s_pending_copies;
    }
    uint32_t elapsed=esp_timer_get_time()-started;
    s_dma_wait_us+=elapsed;
    if(elapsed>s_dma_wait_max_us)s_dma_wait_max_us=elapsed;
}

void display_port_capture_service(void)
{
    capture_job_t *job=NULL;
    if(!s_capture_queue||xQueueReceive(s_capture_queue,&job,0)!=pdTRUE)return;
    job->result=ESP_ERR_TIMEOUT;
    if(!atomic_load(&job->cancelled)){
        copy_wait();
        job->result=esp_cache_msync(s_scan_framebuffer,LCD_H_RES*LCD_V_RES*sizeof(uint16_t),
                                  ESP_CACHE_MSYNC_FLAG_DIR_M2C);
        if(job->result==ESP_OK){
            memcpy(job->image.pixels,s_scan_framebuffer,LCD_H_RES*LCD_V_RES*sizeof(uint16_t));
            job->image.width=LCD_H_RES;job->image.height=LCD_V_RES;
            const unsigned angles[]={0,90,180,270};
            job->image.rotation=angles[lv_display_get_rotation(s_display)];
            job->image.frame=s_frame_complete_count;job->image.captured_us=esp_timer_get_time();
            job->image.asleep=s_standby;
        }
    }
    xSemaphoreGive(job->done);capture_release(job);
}

static void copy_submit(const async_color_convert_request_t *request)
{
    if (s_pending_copies >= COPY_DMA_BATCH) {++s_copy_drains;copy_wait();}
    ESP_ERROR_CHECK(esp_async_color_convert(s_copy_dma, request, copy_complete, NULL));
    ++s_pending_copies;
    if(s_pending_copies>s_copy_peak)s_copy_peak=s_pending_copies;
}

/* Exercise the actual DMA path on private buffers, never on the display.
 * Guard pixels verify that odd-sized rectangles do not overwrite neighbors. */
static esp_err_t copy_self_test(void)
{
    enum { W = 64, H = 32, BYTES = W * H * 2 };
    uint16_t *src = heap_caps_aligned_alloc(64, BYTES, MALLOC_CAP_SPIRAM);
    uint16_t *dst = heap_caps_aligned_alloc(64, BYTES, MALLOC_CAP_SPIRAM);
    if (!src || !dst) { heap_caps_free(src); heap_caps_free(dst); return ESP_ERR_NO_MEM; }
    const int cases[][6] = {{0,0,0,0,64,32}, {1,2,3,5,31,17},
                            {7,9,13,11,1,1}, {31,0,0,1,33,29}};
    bool valid = true;
    for (unsigned n = 0; n < sizeof(cases) / sizeof(cases[0]); ++n) {
        for (int i = 0; i < W * H; ++i) { src[i] = (uint16_t)(i * 37 + n); dst[i] = 0xa55a; }
        ESP_ERROR_CHECK(esp_cache_msync(src, BYTES, ESP_CACHE_MSYNC_FLAG_DIR_C2M));
        ESP_ERROR_CHECK(esp_cache_msync(dst, BYTES,
            ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE));
        int sx=cases[n][0], sy=cases[n][1], dx=cases[n][2], dy=cases[n][3];
        int w=cases[n][4], h=cases[n][5];
        const async_color_convert_request_t req = {
            .src_buffer=src, .src_stride=W, .src_height=H, .src_x=sx, .src_y=sy,
            .dst_buffer=dst, .dst_stride=W, .dst_height=H, .dst_x=dx, .dst_y=dy,
            .copy_width=w, .copy_height=h,
            .src_color_format=ESP_COLOR_FOURCC_RGB16, .dst_color_format=ESP_COLOR_FOURCC_RGB16,
        };
        copy_submit(&req);
        copy_wait();
        ESP_ERROR_CHECK(esp_cache_msync(dst, BYTES, ESP_CACHE_MSYNC_FLAG_DIR_M2C));
        for (int y=0; y<H; ++y) for (int x=0; x<W; ++x) {
            uint16_t expected = x>=dx && x<dx+w && y>=dy && y<dy+h ?
                src[(sy+y-dy)*W+sx+x-dx] : 0xa55a;
            if (dst[y*W+x] != expected) valid = false;
        }
    }
    heap_caps_free(src); heap_caps_free(dst);
    ESP_LOGI(TAG, "DMA2D_COPY_TEST %s: full/odd/offset/single-pixel rectangles and guards", valid ? "PASS" : "FAIL");
    return valid ? ESP_OK : ESP_FAIL;
}

static void copy_rect(const uint16_t *source, int stride, int source_height,
                      int sx, int sy, int x, int y, int width, int height)
{
    const async_color_convert_request_t request = {
        .src_buffer = source, .src_stride = stride, .src_height = source_height,
        .src_x = sx, .src_y = sy, .dst_buffer = s_panel_framebuffer,
        .dst_stride = LCD_H_RES, .dst_height = LCD_V_RES, .dst_x = x, .dst_y = y,
        .copy_width = width, .copy_height = height,
        .src_color_format = ESP_COLOR_FOURCC_RGB16, .dst_color_format = ESP_COLOR_FOURCC_RGB16,
    };
    copy_submit(&request);
}

void display_port_blit(int x, int y, int width, int height,
                       const uint16_t *source, int stride, int source_height, int sx, int sy)
{
    ESP_ERROR_CHECK(x >= 0 && y >= 0 && width > 0 && height > 0 &&
        x + width <= LCD_H_RES && y + height <= LCD_V_RES ? ESP_OK : ESP_ERR_INVALID_ARG);
    copy_rect(source, stride, source_height, sx, sy, x, y, width, height);
}

bool display_port_guide_slide_begin(const lv_area_t *area,lv_color_t background,int distance)
{
    display_port_guide_slide_end();
    if(distance<=0||lv_display_get_rotation(s_display)!=LV_DISPLAY_ROTATION_90)return false;
    int x=area->y1,y=LCD_V_RES-1-area->x2;
    int width=lv_area_get_height(area),height=lv_area_get_width(area);
    if(x<0||y<distance||width<=0||height<=0||x+width>LCD_H_RES||y+height>LCD_V_RES)return false;
    /* Only the exposed strips need clearing. The translated source already
     * includes its background; clearing the entire destination doubles DMA
     * traffic and needlessly allocates a large PSRAM surface. */
    size_t bytes=(size_t)width*distance*sizeof(uint16_t);
    if(bytes>s_slide_capacity){
        uint16_t *next=heap_caps_aligned_alloc(64,bytes,MALLOC_CAP_SPIRAM);
        if(!next)return false;
        heap_caps_free(s_slide_background);s_slide_background=next;s_slide_capacity=bytes;
    }
    uint16_t color=lv_color_to_u16(background);
    for(size_t i=0;i<bytes/sizeof(uint16_t);++i)s_slide_background[i]=color;
    ESP_ERROR_CHECK(esp_cache_msync(s_slide_background,bytes,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M|ESP_CACHE_MSYNC_FLAG_UNALIGNED));
    s_slide_x=x;s_slide_y=y;s_slide_width=width;s_slide_height=height;s_slide_distance=distance;
    return true;
}

void display_port_guide_slide_draw(int offset)
{
    if(!s_slide_height)return;
    if(offset<0)offset=0;
    if(offset>s_slide_distance)offset=s_slide_distance;
    int above=s_slide_distance-offset;
    if(above)copy_rect(s_slide_background,s_slide_width,s_slide_distance,0,0,
        s_slide_x,s_slide_y-s_slide_distance,s_slide_width,above);
    if(offset)copy_rect(s_slide_background,s_slide_width,s_slide_distance,0,0,
        s_slide_x,s_slide_y-offset+s_slide_height,s_slide_width,offset);
    /* Exposed strips and translated content are disjoint, so no intermediate
     * DMA wait is needed. Clear both sides on each alternating scan buffer. */
    /* Both sources are immutable during DMA. Chrome remains at its final
     * layout and only the inactive scan buffer receives translated pixels. */
    copy_rect(s_chrome,LCD_H_RES,LCD_V_RES,s_slide_x,s_slide_y,
        s_slide_x,s_slide_y-offset,s_slide_width,s_slide_height);
}

void display_port_guide_slide_end(void)
{
    if(!s_slide_height)return;
    for(unsigned i=0;i<2;++i)tp_display_damage_set_add(&s_chrome_damage[i],
        s_slide_x,s_slide_y-s_slide_distance,s_slide_x+s_slide_width-1,s_slide_y+s_slide_height-1);
    s_slide_height=0;
}

bool display_port_begin_frame(bool viewport_changed)
{
    tp_display_damage_set_t *pending=&s_chrome_damage[s_back_index];
    s_frame_chrome_damage=*pending;
    if (!pending->count && !viewport_changed) return false;
    if (pending->count) {
        for(unsigned i=0;i<pending->count;++i){
            const tp_display_damage_t *damage=&pending->areas[i];
            int width=damage->right-damage->left+1,height=damage->bottom-damage->top+1;
            copy_rect(s_chrome,LCD_H_RES,LCD_V_RES,damage->left,damage->top,
                      damage->left,damage->top,width,height);
            s_chrome_bytes+=(uint64_t)width*height*2;
        }
        copy_wait();
        pending->count=0;
        /* Each buffer consumes its own accumulated damage. Updates arriving
         * before the other buffer is shown must not erase its pending region. */
    }
    return true;
}

bool display_port_frame_region_changed(int x,int y,int width,int height)
{
    for(unsigned i=0;i<s_frame_chrome_damage.count;++i){
        const tp_display_damage_t *area=&s_frame_chrome_damage.areas[i];
        if(x<=area->right&&x+width>area->left&&y<=area->bottom&&y+height>area->top)return true;
    }
    return false;
}

void display_port_present(void)
{
    copy_wait();
    int64_t started = esp_timer_get_time();
    /* All CPU writes target chrome/tiles; DMA alone writes the panel buffers.
     * Submit one valid row to select the completed buffer. Only this small,
     * clean region needs the public driver's cache synchronization. */
    s_present_task = xTaskGetCurrentTaskHandle();
    ulTaskNotifyTake(pdTRUE, 0);
    /* The DPI ISR is allocated by this display task on the same core. Its
     * callback must not fall between publication and the completion snapshot:
     * that otherwise occasionally waits an unnecessary second display frame.
     * This public API's existing-framebuffer path only syncs our clean single
     * row and assigns cur_fb_index; it performs no blocking copy or wait.
     * IDF 6.1 esp_cache_msync also supports critical-section callers. */
    portENTER_CRITICAL(&s_publish_lock);
    uint32_t before = s_frame_complete_count;
    s_waiting_frame = true;
    esp_err_t published=esp_lcd_panel_draw_bitmap(s_panel, 0, 0, LCD_H_RES, 1, s_panel_framebuffer);
    portEXIT_CRITICAL(&s_publish_lock);
    ESP_ERROR_CHECK(published);
    /* A callback pending before publication now runs after cur_fb_index is
     * set, so it has selected the new buffer before releasing the previous.
     * Keep this contract tied to IDF 6.1 mipi_dsi_dma_trans_done_cb. */
    while ((uint32_t)(s_frame_complete_count - before) < 1) {
        ESP_ERROR_CHECK(ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200)) ? ESP_OK : ESP_ERR_TIMEOUT);
    }
    s_waiting_frame = false;
    uint16_t *previous = s_scan_framebuffer;
    s_scan_framebuffer = s_panel_framebuffer;
    s_panel_framebuffer = previous;
    s_back_index ^= 1;
    s_sync_us += esp_timer_get_time() - started;
    ++s_presented_frames;
}

/* Rotate only the rendered strip, retaining the panel's verified native timing.
 * Small tiles keep both source and destination PSRAM accesses near each other. */
static void write_strip(const lv_area_t *area, const uint8_t *pixels, int stride,
                        lv_display_rotation_t rotation)
{
    int width = lv_area_get_width(area), height = lv_area_get_height(area);
    if (rotation == LV_DISPLAY_ROTATION_0) {
        for (int y = 0; y < height; ++y)
            memcpy(s_chrome + (area->y1 + y) * LCD_H_RES + area->x1,
                   pixels + y * stride, width * sizeof(uint16_t));
        return;
    }
    if (s_rotate) {
        int dx, dy;
        if (rotation == LV_DISPLAY_ROTATION_90) {
            dx = area->y1; dy = LCD_V_RES - 1 - area->x2;
        } else if (rotation == LV_DISPLAY_ROTATION_180) {
            dx = LCD_H_RES - 1 - area->x2; dy = LCD_V_RES - 1 - area->y2;
        } else {
            dx = LCD_H_RES - 1 - area->y2; dy = area->x1;
        }
        ppa_srm_oper_config_t operation = {
            .in = {.buffer = pixels, .pic_w = stride / sizeof(uint16_t), .pic_h = height,
                   .block_w = width, .block_h = height, .srm_cm = PPA_SRM_COLOR_MODE_RGB565},
            .out = {.buffer = s_chrome,
                    .buffer_size = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
                    .pic_w = LCD_H_RES, .pic_h = LCD_V_RES,
                    .block_offset_x = dx, .block_offset_y = dy,
                    .srm_cm = PPA_SRM_COLOR_MODE_RGB565},
            .rotation_angle = rotation == LV_DISPLAY_ROTATION_90 ? PPA_SRM_ROTATION_ANGLE_90 :
                              rotation == LV_DISPLAY_ROTATION_180 ? PPA_SRM_ROTATION_ANGLE_180 :
                                                                  PPA_SRM_ROTATION_ANGLE_270,
            .scale_x = 1.0f, .scale_y = 1.0f,
            .mode = PPA_TRANS_MODE_BLOCKING,
        };
        esp_err_t err = ppa_do_scale_rotate_mirror(s_rotate, &operation);
        if (err == ESP_OK) return;
        ESP_LOGW(TAG, "Hardware rotation failed (%s); using CPU", esp_err_to_name(err));
        ppa_unregister_client(s_rotate);
        s_rotate = NULL;
    }
    if (rotation == LV_DISPLAY_ROTATION_180) {
        for (int y = 0; y < height; ++y) {
            const uint16_t *src = (const uint16_t *)(pixels + y * stride);
            uint16_t *dst = s_chrome +
                (LCD_V_RES - 1 - area->y1 - y) * LCD_H_RES + LCD_H_RES - 1 - area->x1;
            for (int x = 0; x < width; ++x) dst[-x] = src[x];
        }
        return;
    }
    /* Each source column becomes one contiguous destination row. Keeping the
     * complete row together avoids repeatedly evicting half-written PSRAM
     * cache lines when a strip is wider than the cache. */
    for (int x = 0; x < width; ++x) {
        const uint8_t *src = pixels + x * sizeof(uint16_t);
        if (rotation == LV_DISPLAY_ROTATION_90) {
            uint16_t *dst = s_chrome +
                (LCD_V_RES - 1 - area->x1 - x) * LCD_H_RES + area->y1;
            for (int y = 0; y < height; ++y, src += stride) dst[y] = *(const uint16_t *)src;
        } else {
            uint16_t *dst = s_chrome +
                (area->x1 + x) * LCD_H_RES + LCD_H_RES - 1 - area->y1;
            for (int y = 0; y < height; ++y, src += stride) dst[-y] = *(const uint16_t *)src;
        }
    }
}

static void include_dirty_area(const lv_area_t *area, lv_display_rotation_t rotation)
{
    int left, top, right, bottom;
    if (rotation == LV_DISPLAY_ROTATION_0) {
        left = area->x1; right = area->x2; top = area->y1; bottom = area->y2;
    } else if (rotation == LV_DISPLAY_ROTATION_90) {
        left = area->y1; right = area->y2;
        top = LCD_V_RES - 1 - area->x2; bottom = LCD_V_RES - 1 - area->x1;
    } else if (rotation == LV_DISPLAY_ROTATION_180) {
        left = LCD_H_RES - 1 - area->x2; right = LCD_H_RES - 1 - area->x1;
        top = LCD_V_RES - 1 - area->y2; bottom = LCD_V_RES - 1 - area->y1;
    } else {
        left = LCD_H_RES - 1 - area->y2; right = LCD_H_RES - 1 - area->y1;
        top = area->x1; bottom = area->x2;
    }
    if (left < s_dirty_left) s_dirty_left = left;
    if (right > s_dirty_right) s_dirty_right = right;
    if (top < s_dirty_top) s_dirty_top = top;
    if (bottom > s_dirty_bottom) s_dirty_bottom = bottom;
    tp_display_damage_set_add(&s_dirty_areas,left,top,right,bottom);
    tp_display_cache_mark(&s_dirty_cache,left,top,right,bottom);
}

IRAM_ATTR static bool display_refresh_done(esp_lcd_panel_handle_t panel,
                                          esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    (void)panel;
    (void)edata;
    (void)user_ctx;
    s_refresh_count++;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    ++s_frame_complete_count;
#endif
    return false;
}

IRAM_ATTR static bool display_frame_complete(esp_lcd_panel_handle_t panel,
                                             esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    (void)panel;
    (void)edata;
    (void)user_ctx;
    ++s_frame_complete_count;
    BaseType_t wake = pdFALSE;
    if (s_waiting_frame && s_present_task) vTaskNotifyGiveFromISR(s_present_task, &wake);
    return wake == pdTRUE;
}

static void display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels)
{
    if (!s_logged_first_flush) {
        ESP_LOGI(TAG, "First LVGL flush: (%" PRId32 ",%" PRId32 ")-(%" PRId32 ",%" PRId32 "), pixels=%p",
                 area->x1, area->y1, area->x2, area->y2, (void *)pixels);
        s_logged_first_flush = true;
        s_first_flush_us = esp_timer_get_time();
    }
    int64_t started = esp_timer_get_time();
    lv_display_rotation_t rotation = lv_display_get_rotation(display);
    int stride = lv_display_get_buf_active(display)->header.stride;
    write_strip(area, pixels, stride, rotation);
    include_dirty_area(area, rotation);
    s_copy_us += esp_timer_get_time() - started;
    if (lv_display_flush_is_last(display)) {
        started = esp_timer_get_time();
        if(s_dirty_bottom>=s_dirty_top){
            /* A rotated bar is narrow but spans many physical rows. Sync
             * its touched cache lines, not every pixel between those rows. */
            int64_t cache_started=esp_timer_get_time();
            unsigned first=0,count;
            while((first=tp_display_cache_next(&s_dirty_cache,first,&count))<TP_DISPLAY_CACHE_LINE_COUNT){
                size_t bytes=count*TP_DISPLAY_CACHE_LINE_BYTES;
                ESP_ERROR_CHECK(esp_cache_msync((uint8_t *)s_chrome+first*TP_DISPLAY_CACHE_LINE_BYTES,
                    bytes,ESP_CACHE_MSYNC_FLAG_DIR_C2M));
                s_cache_bytes+=bytes;first+=count;
            }
            uint32_t cache_us=esp_timer_get_time()-cache_started;
            if(cache_us>s_cache_max_us)s_cache_max_us=cache_us;
            memset(&s_dirty_cache,0,sizeof(s_dirty_cache));
            for(unsigned i=0;i<2;++i)for(unsigned j=0;j<s_dirty_areas.count;++j){
                const tp_display_damage_t *area=&s_dirty_areas.areas[j];
                tp_display_damage_set_add(&s_chrome_damage[i],area->left,area->top,area->right,area->bottom);
            }
        }
        s_sync_us += esp_timer_get_time() - started;
        s_dirty_left = LCD_H_RES; s_dirty_top = LCD_V_RES;
        s_dirty_right = s_dirty_bottom = -1;
        s_dirty_areas.count=0;
    }
    lv_display_flush_ready(display);
}

static esp_err_t backlight_init(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer init failed");

    const ledc_channel_config_t channel = {
        .gpio_num = LCD_BACKLIGHT_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    esp_err_t result=ledc_channel_config(&channel);
    s_backlight_initialized=result==ESP_OK;
    return result;
}

void display_port_boot_dark(void)
{
    /* Establish the next low output before releasing a previous shutdown hold.
     * Do this before USB/NVS/storage startup, not just before LCD creation. */
    gpio_set_level(LCD_BACKLIGHT_GPIO,0);
    gpio_set_direction(LCD_BACKLIGHT_GPIO,GPIO_MODE_OUTPUT);
    gpio_pullup_dis(LCD_BACKLIGHT_GPIO);
    gpio_pulldown_en(LCD_BACKLIGHT_GPIO);
    gpio_hold_dis(LCD_BACKLIGHT_GPIO);
}

void display_port_power_off(void)
{
    /* Shutdown handlers may run outside the UI task. Do not touch LVGL, DMA,
     * frame semaphores, or allocate here. Leave scan/reset transients unlit. */
    s_standby=true;
    if(s_backlight_initialized)ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_1,0);
    gpio_set_level(LCD_BACKLIGHT_GPIO,0);
    gpio_set_direction(LCD_BACKLIGHT_GPIO,GPIO_MODE_OUTPUT);
    gpio_pullup_dis(LCD_BACKLIGHT_GPIO);
    gpio_pulldown_en(LCD_BACKLIGHT_GPIO);
    esp_err_t held=gpio_hold_en(LCD_BACKLIGHT_GPIO);
    s_backlight_duty=0;
    ESP_LOGI(TAG,"DISPLAY_POWER_OFF backlight=0 hold=%s",esp_err_to_name(held));
}

esp_err_t display_port_init(void)
{
    ESP_RETURN_ON_ERROR(backlight_init(), TAG, "backlight init");
    ESP_LOGI(TAG, "DISPLAY_STAGE panel initialization; backlight duty=0");
    ESP_RETURN_ON_ERROR(board_display_init(&s_panel), TAG, "board display init");
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(s_panel, 2,
                        (void **)&s_scan_framebuffer, (void **)&s_panel_framebuffer),
                        TAG, "panel framebuffer");
    s_chrome = heap_caps_aligned_calloc(64, LCD_H_RES * LCD_V_RES, 2, MALLOC_CAP_SPIRAM);
    ESP_RETURN_ON_FALSE(s_chrome, ESP_ERR_NO_MEM, TAG, "chrome framebuffer");
    ESP_RETURN_ON_ERROR(esp_cache_msync(s_scan_framebuffer, LCD_H_RES * LCD_V_RES * 2,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE), TAG, "scan buffer cache");
    ESP_RETURN_ON_ERROR(esp_cache_msync(s_panel_framebuffer, LCD_H_RES * LCD_V_RES * 2,
        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE), TAG, "draw buffer cache");
    s_copy_done = xSemaphoreCreateCounting(COPY_DMA_BACKLOG, 0);
    ESP_RETURN_ON_FALSE(s_copy_done, ESP_ERR_NO_MEM, TAG, "copy completion semaphore");
    /* Large RGB565 PSRAM copies run beside continuous DPI reads. Use the
     * DMA2D-supported 128-byte burst to reduce transaction overhead. The
     * driver's odd/offset rectangle startup check still guards boundaries. */
    const async_color_convert_config_t copy_config = {.backlog = COPY_DMA_BACKLOG, .dma_burst_size = 128};
    ESP_RETURN_ON_ERROR(tp_dma2d_prepared_install(&copy_config, &s_copy_dma), TAG, "prepared DMA2D");
    ESP_RETURN_ON_ERROR(copy_self_test(), TAG, "DMA2D pixel validation");
    /* PPA is disabled while checking the user's moving-block flicker report.
     * Keep the verified CPU rotation as the display/PSRAM contention control. */
    ESP_LOGI(TAG, "CPU rotation; double buffering enabled");
    const esp_lcd_dpi_panel_event_callbacks_t callbacks = {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        .on_vsync = display_refresh_done,
        .on_frame_buf_complete = display_frame_complete,
#else
        .on_refresh_done = display_refresh_done,
#endif
    };
    ESP_RETURN_ON_ERROR(esp_lcd_dpi_panel_register_event_callbacks(s_panel, &callbacks, NULL),
                        TAG, "DPI callbacks");

    lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
    ESP_RETURN_ON_FALSE(display, ESP_ERR_NO_MEM, TAG, "LVGL display creation failed");
    size_t buffer_size = LVGL_MAX_WIDTH * LVGL_BUFFER_LINES * sizeof(lv_color16_t);
    void *buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(buffer, ESP_ERR_NO_MEM, TAG, "LVGL PSRAM draw buffer allocation failed");
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, display_flush);
    lv_display_set_buffers(display, buffer, NULL, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    s_capture_queue=xQueueCreate(1,sizeof(capture_job_t *));
    ESP_RETURN_ON_FALSE(s_capture_queue, ESP_ERR_NO_MEM, TAG, "capture request queue");
    s_display = display;
    ESP_LOGI(TAG, "JD9365 ready: %dx%d, 2-lane MIPI DSI, RGB565; hardware pattern disabled", LCD_H_RES, LCD_V_RES);
    return ESP_OK;
}

void display_port_tick(void)
{
    display_port_capture_service();
    if (s_standby) return;
    /* Keep the boost converter off until the panel and first LVGL frame have
     * started. Separate its load transient from panel/PSRAM initialization. */
    int64_t now = esp_timer_get_time();
    if (!s_stats_started) s_stats_started = now;
    if (now - s_stats_started >= 10000000) {
        ESP_LOGI(TAG, "FLUSH_WORK frames=%lu copy_avg_us=%lu sync_avg_us=%lu chrome_bytes=%llu cache_bytes=%llu cache_max_us=%lu dma_avg_us=%lu dma_max_us=%lu batch_drains=%u submit_peak=%u",
                 (unsigned long)s_presented_frames,
                 (unsigned long)(s_presented_frames ? s_copy_us / s_presented_frames : 0),
                 (unsigned long)(s_presented_frames ? s_sync_us / s_presented_frames : 0),
                 (unsigned long long)s_chrome_bytes,(unsigned long long)s_cache_bytes,(unsigned long)s_cache_max_us,
                 (unsigned long)(s_presented_frames?s_dma_wait_us/s_presented_frames:0),
                 (unsigned long)s_dma_wait_max_us,s_copy_drains,s_copy_peak);
        s_chrome_bytes=0;
        s_cache_bytes=0;s_cache_max_us=0;
        s_dma_wait_us=0;s_dma_wait_max_us=0;s_copy_drains=s_copy_peak=0;
        s_copy_us = s_sync_us = 0;
        s_presented_frames = 0;
        s_stats_started = now;
    }
    prompter_status_t state;
    prompter_snapshot(&state, NULL);
    /* Last line of defense once the startup frame is ready. */
    int brightness = state.brightness < PROMPTER_BRIGHTNESS_MIN ? PROMPTER_BRIGHTNESS_MIN : state.brightness;
    if (brightness > PROMPTER_BRIGHTNESS_MAX) brightness = PROMPTER_BRIGHTNESS_MAX;
    uint32_t target = (brightness * LCD_BACKLIGHT_MAX_DUTY + 50) / 100;
    if (!s_logged_first_flush || s_refresh_count < 2 ||
        now - s_first_flush_us < 500000 ||
        now - s_last_backlight_step_us < 20000 ||
        s_backlight_duty == target) return;
    uint32_t next = target > s_backlight_duty ?
        (target - s_backlight_duty > 20 ? s_backlight_duty + 20 : target) :
        (s_backlight_duty - target > 20 ? s_backlight_duty - 20 : target);
    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, next);
    if (err == ESP_OK) err = ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Backlight update failed: %s", esp_err_to_name(err));
        return;
    }
    s_backlight_duty = next;
    s_last_backlight_step_us = now;
    if (next == target) ESP_LOGI(TAG, "BACKLIGHT applied=%d%% duty=%lu", brightness, (unsigned long)next);
}

void display_port_set_standby(bool standby)
{
    s_standby = standby;
    if (standby) {
        /* Dim the last frame and then fully turn off the backlight. */
        uint32_t from = s_backlight_duty;
        const unsigned steps=(TP_MOTION_SLEEP_MS+19)/20;
        for (unsigned step=1; step<=steps; ++step) {
            uint32_t next=from-from*step/steps;
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, next));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        s_backlight_duty = 0;
        /* Blank the inactive scan buffer after dimming, then present it.
         * Ambient light must not reveal the last script while asleep. */
        copy_wait();
        memset(s_panel_framebuffer, 0, LCD_H_RES * LCD_V_RES * sizeof(uint16_t));
        ESP_ERROR_CHECK(esp_cache_msync(s_panel_framebuffer,
            LCD_H_RES * LCD_V_RES * sizeof(uint16_t), ESP_CACHE_MSYNC_FLAG_DIR_C2M));
        display_port_present();
        /* Wake rebuilds both scan buffers from the retained UI and text tiles. */
        for(unsigned i=0;i<2;++i)tp_display_damage_set_add(&s_chrome_damage[i],0,0,LCD_H_RES-1,LCD_V_RES-1);
        ESP_LOGI(TAG,"SLEEP_BACKLIGHT off duty=0");
        ESP_LOGI(TAG,"SLEEP_FRAME black; retained UI will redraw on wake");
    }
}

lv_display_t *display_port_get_lvgl_display(void)
{
    return s_display;
}

uint32_t display_port_get_refresh_count(void)
{
    return s_refresh_count;
}
