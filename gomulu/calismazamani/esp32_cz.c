/*
 * esp32_cz.c — ESP32 Runtime Implementation
 *
 * Tonyukuk Türkçe programlama dili - ESP32 donanım kütüphanesi
 * Hedef: ESP32-WROOM-32 (ESP-IDF v5.x)
 */

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include <stdint.h>
#include <string.h>

#include "gomulu.h"

/* Kart header'ı: IDE proje yapısında kartlar/ altında,
 * standart konumda ../kartlar/ altında olabilir */
#if __has_include("kartlar/esp32_devkit.h")
#include "kartlar/esp32_devkit.h"
#elif __has_include("../kartlar/esp32_devkit.h")
#include "../kartlar/esp32_devkit.h"
#else
/* Varsayılan sabitler */
#define GOMULU_HAVUZ_BOYUT  4096
#define I2C_SDA   21
#define I2C_SCL   22
#define SPI_MOSI  23
#define SPI_MISO  19
#define SPI_SCK   18
#define SPI_SS    5
#define UART_TX   1
#define UART_RX   3
#define PWM_FREKANS 5000
#endif

/* ============================================================
 * Havuz Bellek Yönetimi
 * ============================================================ */

uint8_t _gomulu_havuz[GOMULU_HAVUZ_BOYUT];
uint16_t _gomulu_havuz_ptr = 0;

void *_tr_gomulu_ayir(uint16_t boyut) {
    if (_gomulu_havuz_ptr + boyut > GOMULU_HAVUZ_BOYUT) {
        return (void *)0;
    }
    void *ptr = &_gomulu_havuz[_gomulu_havuz_ptr];
    _gomulu_havuz_ptr += boyut;
    return ptr;
}

void _tr_gomulu_sifirla(void) {
    _gomulu_havuz_ptr = 0;
}

/* ============================================================
 * GPIO Fonksiyonları
 * ============================================================ */

void _tr_pin_modu(int16_t pin, int16_t mod) {
    if (pin < 0 || pin > 39) return;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .intr_type = GPIO_INTR_DISABLE,
    };

    if (mod == CIKIS) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    } else if (mod == GIRIS_PULLUP) {
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    } else if (mod == GIRIS_PULLDOWN) {
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    } else {
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    }

    gpio_config(&io_conf);
}

void _tr_dijital_yaz(int16_t pin, int16_t deger) {
    if (pin < 0 || pin > 39) return;
    gpio_set_level((gpio_num_t)pin, deger ? 1 : 0);
}

int16_t _tr_dijital_oku(int16_t pin) {
    if (pin < 0 || pin > 39) return 0;
    return (int16_t)gpio_get_level((gpio_num_t)pin);
}

/* ============================================================
 * PWM (LEDC) Fonksiyonları
 * ============================================================ */

/* Her pin için kanal ataması takibi */
static int8_t pwm_kanal_tablo[40];
static int8_t pwm_sonraki_kanal = 0;
static int pwm_ilk_kez = 1;

void _tr_pwm_baslat(int16_t pin) {
    if (pin < 0 || pin > 39) return;

    if (pwm_ilk_kez) {
        memset(pwm_kanal_tablo, -1, sizeof(pwm_kanal_tablo));
        pwm_ilk_kez = 0;
    }

    if (pwm_sonraki_kanal >= 8) return;  /* Maks 8 kanal */

    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = PWM_FREKANS,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = (ledc_channel_t)pwm_sonraki_kanal,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ch_conf);

    pwm_kanal_tablo[pin] = pwm_sonraki_kanal;
    pwm_sonraki_kanal++;
}

void _tr_pwm_yaz(int16_t pin, int16_t deger) {
    if (pin < 0 || pin > 39) return;
    if (pwm_kanal_tablo[pin] < 0) return;

    uint32_t duty = (deger > 255) ? 255 : ((deger < 0) ? 0 : (uint32_t)deger);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwm_kanal_tablo[pin], duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwm_kanal_tablo[pin]);
}

/* ============================================================
 * ADC Fonksiyonları
 * ============================================================ */

static adc_oneshot_unit_handle_t adc1_handle = NULL;

/* GPIO -> ADC1 kanal eşleşmesi */
static int gpio_adc_kanal(int16_t pin) {
    switch (pin) {
        case 36: return ADC_CHANNEL_0;
        case 37: return ADC_CHANNEL_1;
        case 38: return ADC_CHANNEL_2;
        case 39: return ADC_CHANNEL_3;
        case 32: return ADC_CHANNEL_4;
        case 33: return ADC_CHANNEL_5;
        case 34: return ADC_CHANNEL_6;
        case 35: return ADC_CHANNEL_7;
        default: return -1;
    }
}

int16_t _tr_analog_oku(int16_t pin) {
    int kanal = gpio_adc_kanal(pin);
    if (kanal < 0) return 0;

    if (!adc1_handle) {
        adc_oneshot_unit_init_cfg_t init_cfg = {
            .unit_id = ADC_UNIT_1,
            .ulp_mode = ADC_ULP_MODE_DISABLE,
        };
        adc_oneshot_new_unit(&init_cfg, &adc1_handle);
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc1_handle, (adc_channel_t)kanal, &chan_cfg);

    int deger = 0;
    adc_oneshot_read(adc1_handle, (adc_channel_t)kanal, &deger);
    return (int16_t)deger;
}

/* ============================================================
 * I2C Fonksiyonları
 * ============================================================ */

static i2c_master_bus_handle_t i2c_bus = NULL;

void _tr_i2c_baslat(int16_t hiz) {
    if (i2c_bus) return;  /* Zaten başlatılmış */

    i2c_master_bus_config_t bus_conf = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_new_master_bus(&bus_conf, &i2c_bus);
}

int16_t _tr_i2c_yaz(int16_t adres, int16_t veri) {
    if (!i2c_bus) return -1;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = (uint16_t)adres,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(i2c_bus, &dev_cfg, &dev) != ESP_OK) return -1;

    uint8_t data = (uint8_t)veri;
    esp_err_t ret = i2c_master_transmit(dev, &data, 1, 100);
    i2c_master_bus_rm_device(dev);
    return (ret == ESP_OK) ? 0 : -1;
}

int16_t _tr_i2c_oku(int16_t adres) {
    if (!i2c_bus) return -1;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = (uint16_t)adres,
        .scl_speed_hz = 100000,
    };
    i2c_master_dev_handle_t dev;
    if (i2c_master_bus_add_device(i2c_bus, &dev_cfg, &dev) != ESP_OK) return -1;

    uint8_t data = 0;
    esp_err_t ret = i2c_master_receive(dev, &data, 1, 100);
    i2c_master_bus_rm_device(dev);
    return (ret == ESP_OK) ? (int16_t)data : -1;
}

/* ============================================================
 * SPI Fonksiyonları
 * ============================================================ */

static spi_device_handle_t spi_dev = NULL;

void _tr_spi_baslat(int16_t hiz) {
    if (spi_dev) return;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SPI_MOSI,
        .miso_io_num = SPI_MISO,
        .sclk_io_num = SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = (int)(hiz * 1000),
        .mode = 0,
        .spics_io_num = SPI_SS,
        .queue_size = 1,
    };
    spi_bus_add_device(SPI2_HOST, &dev_cfg, &spi_dev);
}

int16_t _tr_spi_aktar(int16_t veri) {
    if (!spi_dev) return 0;

    uint8_t tx = (uint8_t)veri;
    uint8_t rx = 0;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &tx,
        .rx_buffer = &rx,
    };
    spi_device_transmit(spi_dev, &t);
    return (int16_t)rx;
}

/* ============================================================
 * UART Fonksiyonları
 * ============================================================ */

#define UART_NUM    UART_NUM_0
#define UART_BUF    256

static int uart_baslatildi = 0;

void _tr_seri_baslat(int16_t baud) {
    if (uart_baslatildi) return;

    uart_config_t uart_cfg = {
        .baud_rate = (int)baud,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_driver_install(UART_NUM, UART_BUF * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_cfg);
    uart_set_pin(UART_NUM, UART_TX, UART_RX, -1, -1);
    uart_baslatildi = 1;
}

void _tr_seri_yaz_byte(int16_t veri) {
    uint8_t data = (uint8_t)veri;
    uart_write_bytes(UART_NUM, (const char *)&data, 1);
}

void _tr_seri_yaz_metin(const char *ptr, uint16_t len) {
    uart_write_bytes(UART_NUM, ptr, len);
}

int16_t _tr_seri_oku(void) {
    uint8_t data = 0;
    int len = uart_read_bytes(UART_NUM, &data, 1, pdMS_TO_TICKS(100));
    return (len > 0) ? (int16_t)data : -1;
}

int16_t _tr_seri_hazir_mi(void) {
    size_t buffered = 0;
    uart_get_buffered_data_len(UART_NUM, &buffered);
    return (buffered > 0) ? 1 : 0;
}

/* ============================================================
 * Zamanlama Fonksiyonları
 * ============================================================ */

void _tr_bekle_ms(int16_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void _tr_bekle_us(int16_t us) {
    ets_delay_us((uint32_t)us);
}

int32_t _tr_milis(void) {
    return (int32_t)(esp_timer_get_time() / 1000);
}

int32_t _tr_mikros(void) {
    return (int32_t)esp_timer_get_time();
}

/* ============================================================
 * Kesme Yönetimi
 * ============================================================ */

void _tr_kesme_ac(void) {
    /* ESP32'de FreeRTOS kesmeleri yönetir, bu stub */
}

void _tr_kesme_kapat(void) {
    /* ESP32'de FreeRTOS kesmeleri yönetir, bu stub */
}

/* ============================================================
 * Sistem Başlatma
 * ============================================================ */

static void _gomulu_sistem_baslat(void) {
    /* ESP-IDF otomatik başlatma yapar */
}

/* Ana fonksiyon - derleyici tarafından çağrılır */
extern int16_t ana(void);

void app_main(void) {
    _gomulu_sistem_baslat();
    ana();
}
