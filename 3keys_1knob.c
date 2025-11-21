// ===================================================================================
// Project:   10-LED Status Indicator with Soft-Reboot Button for CH551/CH552/CH554
// Version:   v1.0
// Description:
// -----------
// - Drives a 10-pixel NeoPixel string where each pixel represents a host status.
// - Each status is set individually via set_status(), which can be triggered by
//   a host-sent USB OUT report. Each update resets the pixel timeout; expired
//   entries are cleared automatically.
// - On boot, an internal status update sets the first LED to orange.
// - A dedicated button sends Ctrl+Alt+Del to the host for a soft reboot.
// ===================================================================================

#include <stdint.h>

#include <config.h>     // user configurations
#include <system.h>     // system functions
#include <delay.h>      // delay functions
#include <neo.h>        // NeoPixel functions
#include <usb_conkbd.h> // USB HID keyboard functions

#define LED_COUNT 10

// Timeout handling (in milliseconds)
#define STATUS_TIMEOUT_MS 5000
#define LOOP_DELAY_MS 20

// Colors
#define COLOR_OFF  \
  { 0, 0, 0, 0 }
#define COLOR_ORANGE \
  { NEO_MAX, (uint8_t)(NEO_MAX / 3), 0, STATUS_TIMEOUT_MS }

struct LedStatus
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint16_t remaining_ms;
};

static __xdata struct LedStatus statuses[LED_COUNT];
static uint8_t button_last = 0;

// ===================================================================================
// Helper Functions
// ===================================================================================
static void apply_status_strip(void)
{
  EA = 0; // disable interrupts while shifting out LED data
  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
    NEO_writeColor(statuses[i].r, statuses[i].g, statuses[i].b);
  }
  EA = 1; // enable interrupts again
  NEO_latch();
}

void set_status(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
  if (index >= LED_COUNT)
    return;

  statuses[index].r = r;
  statuses[index].g = g;
  statuses[index].b = b;
  statuses[index].remaining_ms = STATUS_TIMEOUT_MS;
}

static void clear_status(uint8_t index)
{
  if (index >= LED_COUNT)
    return;

  statuses[index].r = 0;
  statuses[index].g = 0;
  statuses[index].b = 0;
  statuses[index].remaining_ms = 0;
}

static void update_timeouts(uint16_t elapsed_ms)
{
  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
    if (statuses[i].remaining_ms == 0)
      continue;

    if (statuses[i].remaining_ms <= elapsed_ms)
    {
      clear_status(i);
    }
    else
    {
      statuses[i].remaining_ms -= elapsed_ms;
    }
  }
}

static void send_ctrl_alt_del(void)
{
  KBD_press(KBD_KEY_LEFT_CTRL);
  KBD_press(KBD_KEY_LEFT_ALT);
  KBD_press(KBD_KEY_DELETE);

  DLY_ms(10);

  KBD_release(KBD_KEY_DELETE);
  KBD_release(KBD_KEY_LEFT_ALT);
  KBD_release(KBD_KEY_LEFT_CTRL);
}

static void handle_reboot_button(void)
{
  uint8_t pressed = !PIN_read(PIN_KEY1); // active low
  if (pressed && !button_last)
  {
    send_ctrl_alt_del();
  }
  button_last = pressed;
}

// ===================================================================================
// Main Function
// ===================================================================================
void main(void)
{
  // Setup
  NEO_init();
  CLK_config();
  DLY_ms(5);
  KBD_init();
  WDT_start();

  PIN_input_PU(PIN_KEY1); // reboot button with pull-up

  for (uint8_t i = 0; i < LED_COUNT; i++)
  {
    clear_status(i);
  }

  // Internal status update: LED 0 is orange on boot
  struct LedStatus boot_color = COLOR_ORANGE;
  set_status(0, boot_color.r, boot_color.g, boot_color.b);
  apply_status_strip();

  // Main loop
  while (1)
  {
    handle_reboot_button();

    update_timeouts(LOOP_DELAY_MS);
    apply_status_strip();

    DLY_ms(LOOP_DELAY_MS);
    WDT_reset();
  }
}
