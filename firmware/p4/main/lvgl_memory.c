#include <string.h>
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "lvgl_memory.h"

esp_err_t lvgl_memory_init(void)
{
    return heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0 ? ESP_OK : ESP_ERR_NO_MEM;
}

/* Called only after the display task has initialized PSRAM. */
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void *mem, size_t bytes)
{ (void)mem; (void)bytes; return NULL; }
void lv_mem_remove_pool(lv_mem_pool_t pool) { (void)pool; }
void *lv_malloc_core(size_t size)
{ return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
void *lv_realloc_core(void *ptr, size_t size)
{ return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
void lv_free_core(void *ptr) { heap_caps_free(ptr); }
void lv_mem_monitor_core(lv_mem_monitor_t *info)
{
    memset(info, 0, sizeof(*info));
    info->total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    info->free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    info->free_biggest_size = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
}
lv_result_t lv_mem_test_core(void) { return LV_RESULT_OK; }
