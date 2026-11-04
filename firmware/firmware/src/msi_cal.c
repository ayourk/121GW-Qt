/*
 * Custom firmware (not in 2.05): measure the MSI clock against the 4 MHz HSE crystal (X2) and set the BLE UART's
 * baud divisor from the measured frequency.
 *
 * Why: the application runs on MSI range 6 (nominally 4.194304 MHz), an RC oscillator, and usart_init() derives
 * USART2's divisor from the nominal value. On a real meter the BLE112 received about 0.5 % of the meter's bytes with
 * bit errors (a 1 KB YMODEM block practically never arrived intact): the bit timing was off by more than the link
 * tolerates. The crystal is accurate; MSI is not.
 *
 * How (RM0038 6.2.14, 19.4; AN3300 figure 12): TIM11 captures the MSI itself (TIM11_OR.TI1_RMP = 01), one capture
 * every 8 MSI cycles, while TIM11 counts an accurate clock: for about 2 ms the CPU runs from the PLL on the crystal,
 * HSE 4 MHz x 12 / 3 = 16 MHz (the bootloader's own setting), so a capture comes every ~30.5 counts. The count
 * differences of MSI_CAL_CAPTURES consecutive captures add up to the 16 MHz ticks in 8 x MSI_CAL_CAPTURES MSI
 * cycles: f_MSI = 8 x pairs x 16 MHz / sum (about 30 ppm). Interrupts are off for those 2 ms (the polling loop must
 * see every capture); a pair with an overcapture between them is skipped anyway. Voltage range 1 and one flash
 * wait state (what SystemInit leaves) are checked and set if needed, then everything is put back as found: SYSCLK
 * MSI, HSE, the PLL's configuration and PLLON (the SDIO clock comes from the PLL VCO: build 3099819d left the PLL
 * off and every card access after it failed with SD_ERROR), flash and voltage settings.
 * The first version captured HSE_RTC with TIM11 counting MSI (RCC_CR.RTCPRE = /16); on the meter (2026-11-03) HSE
 * came up but TIM11 never captured HSE_RTC (RTCSEL is LSI here), so this way round is used instead.
 */
#include "msi_cal.h"
#include "stm32l1xx.h"

#define RCC_CR_HSEON_BIT        (1u << 16)
#define RCC_CR_HSERDY_BIT       (1u << 17)
#define RCC_CR_PLLON_BIT        (1u << 24)
#define RCC_CR_PLLRDY_BIT       (1u << 25)
#define RCC_CFGR_SW_MASK        0x3u
#define RCC_CFGR_SWS_MASK       0xCu
#define RCC_CFGR_PLL_MASK       0x00FD0000u     /* PLLSRC (16), PLLMUL (21:18), PLLDIV (23:22) */
#define RCC_CFGR_PLL_HSE_X12_DIV3 0x00910000u   /* as set_sys_clock(): 4 MHz x 12 / 3 = 16 MHz */
#define RCC_APB1ENR_PWR_BIT     (1u << 28)
#define RCC_APB2ENR_TIM11_BIT   (1u << 4)
#define PWR_CR_VOS_MASK         0x1800u
#define PWR_CR_VOS_RANGE1       0x0800u
#define PWR_CSR_VOSF_BIT        (1u << 4)
#define FLASH_ACR_LATENCY_BIT   (1u << 0)
#define FLASH_ACR_PRFTEN_BIT    (1u << 1)
#define FLASH_ACR_ACC64_BIT     (1u << 2)

#define TIM_OR_MSI              1u              /* TI1_RMP = 01; TI1_RMP_RI (bit 3) = 0 */
#define TIM_CCMR1_TI1_DIV8      (1u | (3u << 2))  /* CC1S = 01 (TI1), IC1PSC = 11 (one capture per 8 edges) */

#define TIMER_HZ                16000000u
#define EDGES_PER_CAPTURE       8u
#define MSI_CAL_CAPTURES        2000u           /* ~3.8 ms; many are lost to overcaptures */
#define MSI_NOMINAL_HZ          4194304u
#define WAIT_LOOPS              0x5000u         /* as the stock HSE start-up wait; also PLL, VOS, switch waits */
#define CAPTURE_WAIT            400u            /* polls; a capture comes every ~30 counts at 16 MHz */

/* result of the last measurement, for INFO; .bt_ram (cleared at reset) */
static uint32_t msi_hz __attribute__((section(".bt_ram")));
/* the last attempt, successful or not: status (MSI_ST_*), counted pairs, summed counts, HSE wait polls */
static struct {
    uint8_t status;
    uint8_t pad;
    uint16_t periods;
    uint32_t total;
    uint32_t hse_polls;
} msi_last __attribute__((section(".bt_ram"), aligned(4)));

uint32_t msi_cal_last_hz(void)
{
    return msi_hz;
}

void msi_cal_last(uint8_t *status, uint16_t *periods, uint32_t *total, uint32_t *hse_polls)
{
    *status = msi_last.status;
    *periods = msi_last.periods;
    *total = msi_last.total;
    *hse_polls = msi_last.hse_polls;
}

static uint8_t wait_set(volatile uint32_t *reg, uint32_t mask, uint32_t value, uint32_t *n)
{
    for (*n = 0; (*reg & mask) != value; ) {
        if (++*n == WAIT_LOOPS) {
            return 1;
        }
    }
    return 0;
}

uint32_t msi_cal_measure(void)
{
    uint32_t cfgr = RCC->CFGR, acr = FLASH->ACR, apb1 = RCC->APB1ENR, apb2 = RCC->APB2ENR;
    uint32_t hse_was_on = RCC->CR & RCC_CR_HSEON_BIT;
    uint32_t pll_was_on = RCC->CR & RCC_CR_PLLON_BIT;
    uint32_t vos, n = 0;
    uint32_t total = 0, periods = 0, got = 0, polls;
    uint16_t last = 0;
    uint8_t have_last = 0, ok = 1, vos_changed = 0;

    msi_last.status = MSI_ST_BUSY;
    msi_last.periods = 0;
    msi_last.total = 0;
    msi_last.hse_polls = 0;
    if ((cfgr & RCC_CFGR_SWS_MASK) != 0) {
        return 0;                               /* SYSCLK is not MSI (a card session runs on HSE): not now */
    }
    /* SystemInit starts the PLL and the application never stops it: the SDIO clock comes from the PLL VCO
       (RM0038 6.2), so a card session (clock_init(1) turns HSE back on) needs it. It can only be reprogrammed
       while off; it is put back exactly as found below */
    RCC->CR &= ~RCC_CR_PLLON_BIT;
    (void)wait_set(&RCC->CR, RCC_CR_PLLRDY_BIT, 0, &n);
    RCC->CR |= RCC_CR_HSEON_BIT;
    if (wait_set(&RCC->CR, RCC_CR_HSERDY_BIT, RCC_CR_HSERDY_BIT, &n) != 0) {
        msi_last.hse_polls = n;
        msi_last.status = MSI_ST_NO_HSE;
        goto restore;
    }
    msi_last.hse_polls = n;
    RCC->APB1ENR = apb1 | RCC_APB1ENR_PWR_BIT;
    __DSB();                                    /* errata 2.2.10 (ST dm00106202): peripheral clock enable delay */
    vos = PWR->CR & PWR_CR_VOS_MASK;
    if (vos != PWR_CR_VOS_RANGE1) {             /* 16 MHz needs range 1 (or 2) */
        PWR->CR = (PWR->CR & ~PWR_CR_VOS_MASK) | PWR_CR_VOS_RANGE1;
        vos_changed = 1;
        (void)wait_set(&PWR->CSR, PWR_CSR_VOSF_BIT, 0, &n);
    }
    FLASH->ACR = acr | FLASH_ACR_ACC64_BIT;     /* 64-bit access before the wait state (RM0038 3.3.4) */
    FLASH->ACR = acr | FLASH_ACR_ACC64_BIT | FLASH_ACR_PRFTEN_BIT | FLASH_ACR_LATENCY_BIT;
    RCC->CFGR = (cfgr & ~RCC_CFGR_PLL_MASK) | RCC_CFGR_PLL_HSE_X12_DIV3;
    RCC->CR |= RCC_CR_PLLON_BIT;
    if (wait_set(&RCC->CR, RCC_CR_PLLRDY_BIT, RCC_CR_PLLRDY_BIT, &n) != 0) {
        msi_last.status = MSI_ST_NO_PLL;
        goto restore;
    }
    RCC->APB2ENR = apb2 | RCC_APB2ENR_TIM11_BIT;
    __DSB();                                    /* errata 2.2.10: the TIM11 clock takes effect a few cycles later */
    TIM11->CR1 = 0;
    TIM11->PSC = 0;
    TIM11->ARR = 0xFFFFu;
    TIM11->OR = TIM_OR_MSI;
    TIM11->CCMR1 = TIM_CCMR1_TI1_DIV8;
    TIM11->CCER = 1u;                           /* CC1E: capture enabled, rising edges */

    __disable_irq();
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_MASK) | RCC_CFGR_SW_PLL;
    if (wait_set(&RCC->CFGR, RCC_CFGR_SWS_MASK, RCC_CFGR_SWS_PLL, &n) != 0) {
        __enable_irq();
        msi_last.status = MSI_ST_NO_SWITCH;
        goto restore;
    }
    TIM11->EGR = 1u;                            /* UG: load PSC, counter to 0 */
    TIM11->SR = 0;
    TIM11->CR1 = 1u;                            /* CEN */
    while (got < MSI_CAL_CAPTURES + 1u) {
        uint32_t sr;

        polls = 0;
        do {
            sr = TIM11->SR;
            if (++polls == CAPTURE_WAIT) {
                ok = 0;
                break;
            }
        } while ((sr & TIM_SR_CC1IF) == 0);
        if (ok == 0) {
            break;
        }
        {
            /* CCR1 first, then the overcapture flag: a capture that lands between seeing CC1IF and reading CCR1
               replaces CCR1 and sets CC1OF, so only CC1OF read AFTER CCR1 tells whether v is the capture that
               set CC1IF (on the meter a third of the captures overcaptured, and checking CC1OF first counted
               those two-period pairs as one: +34 %) */
            uint16_t v = (uint16_t)TIM11->CCR1; /* clears CC1IF */

            if (((sr | TIM11->SR) & TIM_SR_CC1OF) != 0) {
                TIM11->SR = ~TIM_SR_CC1OF;      /* rc_w0 */
                have_last = 0;                  /* v may be a later capture: no pair ends or starts here */
            } else if (have_last != 0) {
                total += (uint16_t)(v - last);
                periods++;
            } else {
                have_last = 1;
            }
            last = v;
        }
        got++;
    }
    TIM11->CR1 = 0;
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_MASK);    /* back to MSI */
    (void)wait_set(&RCC->CFGR, RCC_CFGR_SWS_MASK, 0, &n);
    __enable_irq();
    msi_last.periods = (uint16_t)periods;
    msi_last.total = total;
    if (ok == 0) {
        msi_last.status = (got == 0) ? MSI_ST_NO_CAPTURE : MSI_ST_CAPTURE_STOPPED;
    } else if (periods < MSI_CAL_CAPTURES / 8u) {
        msi_last.status = MSI_ST_OVERCAPTURES;
    } else {
        msi_last.status = MSI_ST_OK;
    }

restore:
    TIM11->CR1 = 0;
    TIM11->CCER = 0;
    TIM11->OR = 0;
    RCC->APB2ENR = apb2;
    RCC->CR &= ~RCC_CR_PLLON_BIT;
    (void)wait_set(&RCC->CR, RCC_CR_PLLRDY_BIT, 0, &n);
    if (hse_was_on == 0) {                      /* HSE as found; the PLL stays off (unused on MSI) */
        RCC->CR &= ~RCC_CR_HSEON_BIT;
        (void)wait_set(&RCC->CR, RCC_CR_HSERDY_BIT, 0, &n);
    }
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_PLL_MASK) | (cfgr & RCC_CFGR_PLL_MASK);
    if (pll_was_on != 0) {
        RCC->CR |= RCC_CR_PLLON_BIT;            /* as found: SDIO needs it (it locks once HSE runs) */
    }
    FLASH->ACR = acr;
    if (vos_changed != 0) {
        PWR->CR = (PWR->CR & ~PWR_CR_VOS_MASK) | vos;
        (void)wait_set(&PWR->CSR, PWR_CSR_VOSF_BIT, 0, &n);
    }
    RCC->APB1ENR = apb1;
    if (msi_last.status != MSI_ST_OK) {
        return 0;
    }
    /* f = 8 x pairs x 16 MHz / total (64-bit: 8 x 1000 x 16e6 = 1.28e11) */
    {
        uint64_t f = ((uint64_t)periods * EDGES_PER_CAPTURE * TIMER_HZ + total / 2u) / total;

        if (f < MSI_NOMINAL_HZ * 9u / 10u || f > MSI_NOMINAL_HZ * 11u / 10u) {
            msi_last.status = MSI_ST_IMPLAUSIBLE;
            return 0;                           /* not plausible for range 6: keep the nominal divisor */
        }
        msi_hz = (uint32_t)f;
        return (uint32_t)f;
    }
}

uint8_t msi_cal_set_baud(USART_TypeDef *usart, uint32_t baud)
{
    uint32_t f, brr, n = 0;

    while ((usart->SR & USART_SR_TC) == 0 && (usart->CR1 & USART_CR1_UE) != 0 && ++n < 100000u) {
    }                                           /* the last byte has left the shift register: the clock may switch */
    f = msi_cal_measure();
    if (f == 0) {
        return 1;
    }
    brr = (f + baud / 2u) / baud;               /* oversampling 16: BRR = USARTDIV x 16 = f / baud */
    usart->BRR = (uint16_t)brr;
    return 0;
}
