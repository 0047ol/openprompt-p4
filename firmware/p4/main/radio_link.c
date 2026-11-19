#include "radio_link.h"

#include <inttypes.h>
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "sdmmc_cmd.h"

static const char *TAG = "radio_link";

esp_err_t radio_link_probe(void)
{
    /* Candidate wiring used by P4/C6 modules. Successful enumeration is
       required before treating this as the actual board pin assignment. */
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1;
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = 400;
    host.command_timeout_ms = 300;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.clk = GPIO_NUM_18;
    slot.cmd = GPIO_NUM_19;
    slot.d0 = GPIO_NUM_14;
    slot.d1 = GPIO_NUM_15;
    slot.d2 = GPIO_NUM_16;
    slot.d3 = GPIO_NUM_17;
    slot.width = 1;
    slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "SDIO probe: CLK=18 CMD=19 D0=14, 1-bit 400kHz; C6 reset untouched");
    esp_err_t err = sdmmc_host_init();
    if (err != ESP_OK) {
        return err;
    }
    err = sdmmc_host_init_slot(host.slot, &slot);
    if (err == ESP_OK) {
        sdmmc_card_t card = {0};
        err = sdmmc_card_init(&host, &card);
        if (err == ESP_OK && !card.is_sdio) {
            ESP_LOGW(TAG, "Candidate pins responded as memory, not an SDIO radio");
            err = ESP_ERR_NOT_SUPPORTED;
        }
        if (err == ESP_OK) {
            uint8_t cccr = 0;
            err = sdmmc_io_read_byte(&card, 0, 0, &cccr);
            ESP_LOGI(TAG, "SDIO_REPLY functions=%u OCR=0x%08" PRIx32
                     " CCCR=0x%02x read=%s", card.num_io_functions,
                     card.ocr, cccr, esp_err_to_name(err));
            uint8_t cis[128];
            size_t size = sizeof(cis);
            if (err == ESP_OK &&
                sdmmc_io_get_cis_data(&card, cis, sizeof(cis), &size) == ESP_OK) {
                sdmmc_io_print_cis_info(cis, size, stdout);
            }
        }
    }
    sdmmc_host_deinit();
    ESP_LOGI(TAG, "SDIO probe finished: %s", esp_err_to_name(err));
    return err;
}
