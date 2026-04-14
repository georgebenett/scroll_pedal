/*
 * BLE HID scroll pedal — keyboard mode.
 *
 * Advertises as a BLE keyboard named "scroll pedal".
 * Button press  → Down Arrow key press   (scrolls page in Safari/Chrome)
 * Button release → key release report
 *
 * Keyboard report layout (8 bytes, Report ID 1):
 *   [0] modifiers  (always 0)
 *   [1] reserved   (always 0)
 *   [2] keycode 1  (0x51 = Down Arrow on press, 0x00 on release)
 *   [3..7]         (always 0)
 */

#include "hid_pedal.h"
#include "esp_hid_gap.h"

#include <string.h>
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_hidd.h"
#include "esp_hidd_gatts.h"
#include "esp_timer.h"

static const char *TAG = "HID_PEDAL";

#define HID_KEY_DOWN_ARROW  0x51   /* USB HID keyboard usage for Down Arrow */
#define HID_KEY_UP_ARROW    0x52   /* USB HID keyboard usage for Up Arrow   */
#define HID_REPORT_ID       1

/* ---- HID Report Descriptor: Keyboard, Report ID 1 ---- */
static const uint8_t s_keyboard_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    /* Modifier keys (8 bits) */
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0xE0,        //   Usage Minimum (Left Control)
    0x29, 0xE7,        //   Usage Maximum (Right GUI)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Var, Abs)
    /* Reserved byte */
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x03,        //   Input (Const, Var, Abs)
    /* Keycodes (6 keys) */
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,        //   Usage Minimum (0x00)
    0x29, 0x65,        //   Usage Maximum (0x65)
    0x81, 0x00,        //   Input (Data, Array, Abs)
    0xC0,              // End Collection
};

static esp_hid_raw_report_map_t s_report_maps[] = {
    {
        .data = s_keyboard_report_map,
        .len  = sizeof(s_keyboard_report_map),
    },
};

static esp_hid_device_config_t s_hid_config = {
    .vendor_id         = 0x16C0,
    .product_id        = 0x05DF,
    .version           = 0x0100,
    .device_name       = "scroll pedal",
    .manufacturer_name = "GB",
    .serial_number     = "0001",
    .report_maps       = s_report_maps,
    .report_maps_len   = 1,
};

static esp_hidd_dev_t  *s_hid_dev           = NULL;
static volatile bool    s_connected         = false;
static esp_bd_addr_t    s_peer_bda;
static bool             s_have_peer         = false;
static bool             s_manual_disconnect = false;
static esp_timer_handle_t s_adv_timer       = NULL;

/* Delay (µs) before re-advertising after a manual disconnect.
 * Long enough that the old phone's auto-reconnect doesn't win the race,
 * short enough that the new phone connects quickly once you tap it. */
#define ADV_RESTART_DELAY_US  (5 * 1000 * 1000)   /* 5 s */

static void adv_restart_cb(void *arg)
{
    ESP_LOGI("HID_PEDAL", "Restarting advertising");
    esp_hid_ble_gap_adv_start();
}

/* ---- HID device event callback ----------------------------------------- */

static void hidd_event_callback(void *handler_args, esp_event_base_t base,
                                int32_t id, void *event_data)
{
    esp_hidd_event_t       event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "BLE HID started, begin advertising");
        esp_hid_ble_gap_adv_start();
        break;

    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "Host connected");
        s_connected = true;
        break;

    case ESP_HIDD_PROTOCOL_MODE_EVENT:
        ESP_LOGI(TAG, "Protocol mode[%u]: %s",
                 param->protocol_mode.map_index,
                 param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;

    case ESP_HIDD_CONTROL_EVENT:
        break;

    case ESP_HIDD_OUTPUT_EVENT:
    case ESP_HIDD_FEATURE_EVENT:
        break;

    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "Host disconnected (%s)",
                 esp_hid_disconnect_reason_str(
                     esp_hidd_dev_transport_get(param->disconnect.dev),
                     param->disconnect.reason));
        s_connected = false;
        if (s_manual_disconnect) {
            s_manual_disconnect = false;
            /* Delay re-advertising so the old phone's auto-reconnect
             * doesn't immediately win. The other phone connects during
             * this window using its existing bond — no re-pairing needed. */
            esp_timer_start_once(s_adv_timer, ADV_RESTART_DELAY_US);
        } else {
            esp_hid_ble_gap_adv_start();
        }
        break;

    case ESP_HIDD_STOP_EVENT:
        ESP_LOGI(TAG, "BLE HID stopped");
        break;

    default:
        break;
    }
}

/* ---- GATTS wrapper: captures peer address then delegates --------------- */

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                 esp_ble_gatts_cb_param_t *param)
{
    if (event == ESP_GATTS_CONNECT_EVT) {
        memcpy(s_peer_bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        s_have_peer = true;
    } else if (event == ESP_GATTS_DISCONNECT_EVT) {
        s_have_peer = false;
    }
    esp_hidd_gatts_event_handler(event, gatts_if, param);
}

/* ---- Stub required by esp_hid_gap.c ------------------------------------ */
void ble_hid_task_start_up(void) {}

/* ---- Public API --------------------------------------------------------- */

esp_err_t hid_pedal_init(void)
{
    esp_err_t ret;

    esp_timer_create_args_t ta = {
        .callback = adv_restart_cb,
        .name     = "adv_restart",
    };
    ESP_ERROR_CHECK(esp_timer_create(&ta, &s_adv_timer));

    ret = esp_hid_gap_init(HIDD_BLE_MODE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_hid_gap_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, s_hid_config.device_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_hid_ble_gap_adv_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_gatts_register_callback(gatts_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GATTS register callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_hidd_dev_init(&s_hid_config, ESP_HID_TRANSPORT_BLE,
                            hidd_event_callback, &s_hid_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_hidd_dev_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

/*
 * delta != 0 → send Down Arrow key press
 * delta == 0 → send key release (all zeros)
 */
void hid_pedal_scroll(int8_t delta)
{
    if (!s_connected || s_hid_dev == NULL) {
        return;
    }

    /* 8-byte keyboard report: [modifier, reserved, key1..key6] */
    uint8_t report[8] = {0};
    if (delta < 0) {
        report[2] = HID_KEY_DOWN_ARROW;
    } else if (delta > 0) {
        report[2] = HID_KEY_UP_ARROW;
    }

    esp_hidd_dev_input_set(s_hid_dev, 0, HID_REPORT_ID, report, sizeof(report));
}

bool hid_pedal_is_connected(void)
{
    return s_connected;
}

void hid_pedal_disconnect(void)
{
    if (!s_connected || !s_have_peer) {
        return;
    }
    ESP_LOGI(TAG, "Disconnecting BLE host (bond kept)");
    s_manual_disconnect = true;
    esp_ble_gap_disconnect(s_peer_bda);
}
