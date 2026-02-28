/*
 * avr_cz.c — AVR (Arduino) Runtime Implementation
 *
 * Tonyukuk Türkçe programlama dili - Arduino donanım kütüphanesi
 * Hedef: ATmega328P (Arduino UNO/Nano)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/twi.h>
#include <stdint.h>

#include "gomulu.h"
#include "../kartlar/arduino_uno.h"

/* ============================================================
 * Havuz Bellek Yönetimi
 * ============================================================ */

uint8_t _gomulu_havuz[GOMULU_HAVUZ_BOYUT];
uint16_t _gomulu_havuz_ptr = 0;

void *_tr_gomulu_ayir(uint16_t boyut) {
    if (_gomulu_havuz_ptr + boyut > GOMULU_HAVUZ_BOYUT) {
        return (void *)0;  /* Bellek yetersiz */
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

/* Port register pointer tabloları */
static volatile uint8_t * const port_tablo[] = {&PORTD, &PORTB, &PORTC};
static volatile uint8_t * const ddr_tablo[]  = {&DDRD,  &DDRB,  &DDRC};
static volatile uint8_t * const pin_tablo[]  = {&PIND,  &PINB,  &PINC};

void _tr_pin_modu(int16_t pin, int16_t mod) {
    if (pin < 0 || pin > 19) return;

    uint8_t port_idx = pin_port_tablo[pin];
    uint8_t bit = pin_bit_tablo[pin];

    volatile uint8_t *ddr = ddr_tablo[port_idx];
    volatile uint8_t *port = port_tablo[port_idx];

    if (mod == CIKIS) {
        *ddr |= (1 << bit);         /* Output */
    } else if (mod == GIRIS_PULLUP) {
        *ddr &= ~(1 << bit);        /* Input */
        *port |= (1 << bit);        /* Pull-up aktif */
    } else {
        *ddr &= ~(1 << bit);        /* Input */
        *port &= ~(1 << bit);       /* Pull-up pasif */
    }
}

void _tr_dijital_yaz(int16_t pin, int16_t deger) {
    if (pin < 0 || pin > 19) return;

    uint8_t port_idx = pin_port_tablo[pin];
    uint8_t bit = pin_bit_tablo[pin];

    volatile uint8_t *port = port_tablo[port_idx];

    if (deger) {
        *port |= (1 << bit);        /* HIGH */
    } else {
        *port &= ~(1 << bit);       /* LOW */
    }
}

int16_t _tr_dijital_oku(int16_t pin) {
    if (pin < 0 || pin > 19) return 0;

    uint8_t port_idx = pin_port_tablo[pin];
    uint8_t bit = pin_bit_tablo[pin];

    volatile uint8_t *pin_reg = pin_tablo[port_idx];

    return (*pin_reg & (1 << bit)) ? 1 : 0;
}

/* ============================================================
 * PWM Fonksiyonları
 * ============================================================ */

void _tr_pwm_baslat(int16_t pin) {
    /* PWM timer'ları ayarla */
    switch (pin) {
        case 3:  /* Timer2, OC2B */
            TCCR2A |= (1 << COM2B1) | (1 << WGM20) | (1 << WGM21);
            TCCR2B |= (1 << CS21) | (1 << CS20);  /* Prescaler 32 */
            break;
        case 5:  /* Timer0, OC0B */
            TCCR0A |= (1 << COM0B1) | (1 << WGM00) | (1 << WGM01);
            TCCR0B |= (1 << CS01) | (1 << CS00);  /* Prescaler 64 */
            break;
        case 6:  /* Timer0, OC0A */
            TCCR0A |= (1 << COM0A1) | (1 << WGM00) | (1 << WGM01);
            TCCR0B |= (1 << CS01) | (1 << CS00);
            break;
        case 9:  /* Timer1, OC1A */
            TCCR1A |= (1 << COM1A1) | (1 << WGM10);
            TCCR1B |= (1 << WGM12) | (1 << CS11) | (1 << CS10);
            break;
        case 10: /* Timer1, OC1B */
            TCCR1A |= (1 << COM1B1) | (1 << WGM10);
            TCCR1B |= (1 << WGM12) | (1 << CS11) | (1 << CS10);
            break;
        case 11: /* Timer2, OC2A */
            TCCR2A |= (1 << COM2A1) | (1 << WGM20) | (1 << WGM21);
            TCCR2B |= (1 << CS21) | (1 << CS20);
            break;
    }

    /* Pin'i çıkış olarak ayarla */
    _tr_pin_modu(pin, CIKIS);
}

void _tr_pwm_yaz(int16_t pin, int16_t deger) {
    uint8_t pwm_val = (deger > 255) ? 255 : ((deger < 0) ? 0 : (uint8_t)deger);

    switch (pin) {
        case 3:  OCR2B = pwm_val; break;
        case 5:  OCR0B = pwm_val; break;
        case 6:  OCR0A = pwm_val; break;
        case 9:  OCR1A = pwm_val; break;
        case 10: OCR1B = pwm_val; break;
        case 11: OCR2A = pwm_val; break;
    }
}

/* ============================================================
 * ADC Fonksiyonları
 * ============================================================ */

int16_t _tr_analog_oku(int16_t pin) {
    uint8_t kanal;

    /* Pin -> ADC kanal dönüşümü */
    if (pin >= 14 && pin <= 19) {
        kanal = pin - 14;  /* A0-A5 -> kanal 0-5 */
    } else if (pin >= 0 && pin <= 5) {
        kanal = pin;       /* Doğrudan kanal numarası */
    } else {
        return 0;
    }

    /* ADC'yi ayarla */
    ADMUX = (1 << REFS0) | (kanal & 0x07);  /* AVCC referans, kanal seç */
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);  /* Enable, prescaler 128 */

    /* Dönüşümü başlat */
    ADCSRA |= (1 << ADSC);

    /* Tamamlanmasını bekle */
    while (ADCSRA & (1 << ADSC));

    /* 10-bit sonuç oku */
    return ADC;
}

/* ============================================================
 * I2C (TWI) Fonksiyonları
 * ============================================================ */

void _tr_i2c_baslat(int16_t hiz) {
    /* TWI bit rate ayarla */
    /* SCL frekansı = F_CPU / (16 + 2*TWBR*prescaler) */
    uint8_t twbr_val;

    if (hiz >= 400) {
        twbr_val = 12;  /* ~400kHz */
    } else {
        twbr_val = 72;  /* ~100kHz */
    }

    TWBR = twbr_val;
    TWSR = 0;  /* Prescaler = 1 */
    TWCR = (1 << TWEN);  /* TWI Enable */
}

static void i2c_bekle(void) {
    while (!(TWCR & (1 << TWINT)));
}

static void i2c_baslat_durum(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    i2c_bekle();
}

static void i2c_durdur(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

int16_t _tr_i2c_yaz(int16_t adres, int16_t veri) {
    /* START */
    i2c_baslat_durum();
    if ((TWSR & 0xF8) != TW_START) return -1;

    /* Slave adresi + Write bit */
    TWDR = (adres << 1) | 0;
    TWCR = (1 << TWINT) | (1 << TWEN);
    i2c_bekle();
    if ((TWSR & 0xF8) != TW_MT_SLA_ACK) {
        i2c_durdur();
        return -1;
    }

    /* Veri gönder */
    TWDR = (uint8_t)veri;
    TWCR = (1 << TWINT) | (1 << TWEN);
    i2c_bekle();
    if ((TWSR & 0xF8) != TW_MT_DATA_ACK) {
        i2c_durdur();
        return -1;
    }

    /* STOP */
    i2c_durdur();
    return 0;
}

int16_t _tr_i2c_oku(int16_t adres) {
    /* START */
    i2c_baslat_durum();
    if ((TWSR & 0xF8) != TW_START) return -1;

    /* Slave adresi + Read bit */
    TWDR = (adres << 1) | 1;
    TWCR = (1 << TWINT) | (1 << TWEN);
    i2c_bekle();
    if ((TWSR & 0xF8) != TW_MR_SLA_ACK) {
        i2c_durdur();
        return -1;
    }

    /* Veri oku (NACK ile) */
    TWCR = (1 << TWINT) | (1 << TWEN);
    i2c_bekle();
    uint8_t veri = TWDR;

    /* STOP */
    i2c_durdur();
    return veri;
}

/* ============================================================
 * SPI Fonksiyonları
 * ============================================================ */

void _tr_spi_baslat(int16_t hiz) {
    /* SPI pinlerini ayarla */
    DDRB |= (1 << DDB3) | (1 << DDB5) | (1 << DDB2);  /* MOSI, SCK, SS çıkış */
    DDRB &= ~(1 << DDB4);  /* MISO giriş */

    /* SPI Enable, Master mode */
    uint8_t spcr = (1 << SPE) | (1 << MSTR);

    /* Hız ayarla (prescaler) */
    if (hiz >= 4000) {
        spcr |= (0 << SPR1) | (0 << SPR0);  /* F_CPU/4 */
    } else if (hiz >= 1000) {
        spcr |= (0 << SPR1) | (1 << SPR0);  /* F_CPU/16 */
    } else {
        spcr |= (1 << SPR1) | (0 << SPR0);  /* F_CPU/64 */
    }

    SPCR = spcr;
}

int16_t _tr_spi_aktar(int16_t veri) {
    SPDR = (uint8_t)veri;
    while (!(SPSR & (1 << SPIF)));
    return SPDR;
}

/* ============================================================
 * UART Fonksiyonları
 * ============================================================ */

void _tr_seri_baslat(int16_t baud) {
    /* Baud rate hesapla */
    uint16_t ubrr = (F_CPU / 16 / baud) - 1;

    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;

    /* TX ve RX aktif */
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);

    /* 8 bit veri, 1 stop bit, parity yok */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void _tr_seri_yaz_byte(int16_t veri) {
    /* TX buffer boş olana kadar bekle */
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = (uint8_t)veri;
}

void _tr_seri_yaz_metin(const char *ptr, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        _tr_seri_yaz_byte(ptr[i]);
    }
}

int16_t _tr_seri_oku(void) {
    /* RX buffer dolu olana kadar bekle */
    while (!(UCSR0A & (1 << RXC0)));
    return UDR0;
}

int16_t _tr_seri_hazir_mi(void) {
    return (UCSR0A & (1 << RXC0)) ? 1 : 0;
}

/* ============================================================
 * Zamanlama Fonksiyonları
 * ============================================================ */

/* Milisaniye sayacı (Timer0 interrupt ile güncellenir) */
static volatile uint32_t _milis_sayac = 0;

/* Timer0 Overflow Interrupt Handler */
ISR(TIMER0_OVF_vect) {
    _milis_sayac++;  /* ~1ms her overflow (yaklaşık) */
}

void _tr_bekle_ms(int16_t ms) {
    while (ms-- > 0) {
        _delay_ms(1);
    }
}

void _tr_bekle_us(int16_t us) {
    while (us-- > 0) {
        _delay_us(1);
    }
}

int32_t _tr_milis(void) {
    uint32_t m;
    uint8_t sreg = SREG;
    cli();
    m = _milis_sayac;
    SREG = sreg;
    return (int32_t)m;
}

int32_t _tr_mikros(void) {
    uint32_t m;
    uint8_t t;
    uint8_t sreg = SREG;
    cli();
    m = _milis_sayac;
    t = TCNT0;
    SREG = sreg;
    return (int32_t)((m * 1000) + (t * 4));  /* Yaklaşık */
}

/* ============================================================
 * Kesme Yönetimi
 * ============================================================ */

void _tr_kesme_ac(void) {
    sei();
}

void _tr_kesme_kapat(void) {
    cli();
}

/* ============================================================
 * Sistem Başlatma
 * ============================================================ */

void _gomulu_sistem_baslat(void) {
    /* Timer0'ı millis için ayarla */
    TCCR0A = 0;
    TCCR0B = (1 << CS01) | (1 << CS00);  /* Prescaler 64 */
    TIMSK0 = (1 << TOIE0);  /* Overflow interrupt enable */

    /* Global interrupt enable */
    sei();
}

/* Ana fonksiyon - derleyici tarafından çağrılır */
extern int16_t ana(void);

int main(void) {
    _gomulu_sistem_baslat();
    ana();
    while (1);  /* Sonsuz döngü */
    return 0;
}
