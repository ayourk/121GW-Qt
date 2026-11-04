/* SPDX-License-Identifier: MIT */
/* UI-area fixes for stock 2.05, ported from the defect list of zombu4's 121GW-Open-Firmware (descriptions only; the
 * code here is written from the stock disassembly). Every function replaces one stock `bl` (hooks.tsv). No .data;
 * the one .bss byte (time_req) is only acted on while SETUP page 7 is shown, so its power-on content is harmless. */
#include "ui.h"

/* ---- OF-36 (+ OF-07 LowZ path, OF-32): Mode_ApplyChange, in place of its call to Reading_SetSettleCount ----
 * A mode or range apply cancels a calibration capture still running (offset, gain, Ohm range-6 trim, pending AC
 * frequency points) with the refusal beep, so the capture can never complete in, and be written to, the record of
 * a different mode or range. Then BEEP and FREQ/PULSE/DUTY (settle count 0) blank the main display until their
 * first result instead of drawing the zeroed reading: their settle count becomes 1, so the display update that
 * VA_Reset forces later in the same apply (with the reading just zeroed) is discarded and blanks the display, as it
 * does in every other mode; the first real result then clears the blank and is drawn. */
void ui_apply_settle(void)
{
    BYTE busy = CAL_OFFSET_RUN | CAL_GAIN_RUN | CAL_OHM6_RUN;
    for (int i = 0; i < 4; i++) busy |= CAL_ACPT(i);
    if (busy) {
        CAL_OFFSET_RUN = 0; CAL_GAIN_RUN = 0; CAL_OHM6_RUN = 0;
        for (int i = 0; i < 4; i++) CAL_ACPT(i) = 0;
        stock_Beep_Double();
    }
    stock_Reading_SetSettleCount();
    BYTE m = G_MODE;
    if ((m == 6 || m == 7 || m == 8 || m == 10) && G_SETTLE == 0) G_SETTLE = 1;
}

/* ---- OF-07: calibration refused in BEEP (Key_ShortPress SETUP and MEM, in place of Cal_CaptureRejected) ----
 * BEEP's record is 0x120, which is also OHM range 1's record although the two run different HY3131 rows. */
BYTE ui_cal_rejected(BYTE kind)
{
    BYTE r = stock_Cal_CaptureRejected(kind);
    if (G_MODE == 10) r = 1;
    return r;
}

/* ---- OF-08: current-zero record cleared only when its mode/range is calibrated ----
 * BootScreen no longer clears all six records on entering calibration mode (ui_cal_enter_zero is a no-op in place
 * of CurrentZero_ClearAll); arming a capture in a current mode on range 0 clears that one record and the live trim
 * first, so the capture never sees the user zero. */
void ui_cal_enter_zero(void) { }

BYTE ui_cal_arm(BYTE kind)
{
    BYTE r = stock_Cal_ArmCapture(kind);
    BYTE m = G_MODE;
    if ((CAL_OFFSET_RUN | CAL_GAIN_RUN) && m >= 0x10 && m <= 0x15 && G_RANGE == 0) {
        stock_CurrentZero_Store(m, 0, 0);
        G_TRIM = 0;
    }
    return r;
}

/* ---- Bluetooth: OF-45 bounded transmit, OF-10 one sender at a time, OF-11 first frame, OF-34 no frame while blank */
#define POLLS_BYTE  5000u     /* > 10 ms at 4.19 MHz; one byte at 19200 Bd is 0.52 ms */
#define POLLS_FRAME 20000u    /* > 19 bytes */

static int usart2_clocked(void) { return (RCC_APB1ENR >> 17) & 1; }

/* in place of USART_SendByteBlocking (TIM2 packet start, USART2 TXE interrupt, reply): never waits on an unclocked
 * USART2 (its registers read 0 there, so TXE never comes), and gives up after POLLS_BYTE polls */
void ui_usart_send(void *usart, BYTE b)
{
    (void)usart;
    if (!usart2_clocked()) return;
    for (UINT n = 0; n < POLLS_BYTE; n++)
        if (USART2_SR & 0x80) { USART2_DR = b; return; }
}

/* a stock packet is sent only if the buffer holds a built packet (0xF2): not before the first build, and not when
 * ui_ble_build withheld the frame because the reading was invalid */
static int packet_ready(void) { return G_BLE_TX[0] == 0xF2; }

void ui_ble_tick_send(void *usart, BYTE b)
{
    if (packet_ready()) ui_usart_send(usart, b);
}

void ui_ble_tick_txe(void *usart, UINT it, UINT on)
{
    if (usart2_clocked() && packet_ready()) stock_USART_ITConfig(usart, it, on);
}

static void wait_tx_idle(void)    /* a frame is in flight while the TXE interrupt is enabled */
{
    for (UINT n = 0; n < POLLS_FRAME && usart2_clocked() && (USART2_CR1 & 0x80); n++) { }
}

static UINT tim2_mask(void)
{
    UINT was = NVIC_ISER0 & (1u << IRQ_TIM2);
    NVIC_ICER0 = 1u << IRQ_TIM2;
    __asm volatile ("dsb\n\tisb" ::: "memory");
    return was;
}

static void tim2_unmask(UINT was)
{
    if (was) NVIC_ISER0 = 1u << IRQ_TIM2;     /* a tick that fell in between stays pending and runs now */
}

/* main loop, in place of BLE_BuildPacket_Thunk; also BLE start (OF-11) */
void ui_ble_build(void)
{
    if (G_MODE != 0 && (G_SETTLE || G_BLANK) && !G_AUTOHOLD) {   /* LCD_DrawMain shows blank: no valid value */
        G_BLE_TX[0] = 0;
        return;
    }
    wait_tx_idle();
    UINT was = tim2_mask();               /* TIM2 cannot start the packet while it is half built */
    stock_BLE_BuildPacket_Thunk();
    tim2_unmask(was);
}

/* BLE_ProcessRx, in place of BLE_SendReply: the echo waits for a frame in flight and holds TIM2 off while it is
 * sent, so its bytes cannot interleave with a status packet */
void ui_ble_reply(void)
{
    wait_tx_idle();
    UINT was = tim2_mask();
    BYTE len = BLE_RX_LEN;
    for (BYTE i = 0; i < len; i++) ui_usart_send(USART2_BASE, BLE_RX_COPY[i]);
    tim2_unmask(was);
}

/* BLE_Toggle, in place of BLE_Start: build at once so the first frame is current (OF-11) */
void ui_ble_start(void)
{
    stock_BLE_Start();
    ui_ble_build();
}

/* ---- OF-48: Keys_Scan, in place of Keys_AutoRepeat(1) for REL/PEAK held in the MEM view or SETUP edit ----
 * In edit the press is queued for the release (a tap needs it: the first repeat comes only after 50 ticks); once
 * the hold starts repeating, the queued press is dropped so the release adds no extra step. */
void ui_keys_repeat(BYTE held)
{
    BYTE before = KEY_RPT_STAGE;
    stock_Keys_AutoRepeat(held);
    if (before == 0 && KEY_RPT_STAGE != 0) KEY_QUEUED = 0;
}

/* ---- OF-40 + OF-55: App_Sleep, in place of LowPower_Prepare ----
 * TIM2/TIM3 interrupts off (NVIC and DIER, pending cleared) before the buzzer state is cleared and PE7 driven low,
 * so the TIM2 jack-warning beep cannot set PE7 again during the power-down; Board_Init re-arms both at the wake.
 * The backlight flag follows PE8, which Power_Off drives low, so the first MODE long press after the wake works. */
void ui_sleep_prepare(void)
{
    NVIC_ICER0 = (1u << IRQ_TIM2) | (1u << IRQ_TIM3);
    TIM2_DIER &= ~1u;
    TIM3_DIER &= ~1u;
    __asm volatile ("dsb\n\tisb" ::: "memory");
    NVIC_ICPR0 = (1u << IRQ_TIM2) | (1u << IRQ_TIM3);
    BUZ_ACTIVE = 0;
    BUZ_REPEAT = 0;
    GPIOE_BSRR = 1u << (16 + 7);          /* PE7 low */
    G_BACKLIGHT = 0;
    stock_LowPower_Prepare();
}

/* ---- OF-41: setup page 7 time drawn by main, not by the TIM2 handler ----
 * TIM2 (in place of LCD_SubHourMinuteBlink(1)) only records the request; main draws it before its own refresh (in
 * place of LCD_RefreshDirty), so the reading and the time never share the digit buffers at the same time. */
static volatile BYTE time_req;

void ui_time_request(BYTE flush)
{
    (void)flush;
    time_req = 1;
}

void ui_refresh(void)
{
    if (time_req) {
        time_req = 0;
        if (G_SETUP_PAGE == 7) stock_LCD_SubHourMinuteBlink(1);
    }
    stock_LCD_RefreshDirty();
}
