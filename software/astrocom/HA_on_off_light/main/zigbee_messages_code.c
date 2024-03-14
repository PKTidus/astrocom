#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "esp_zb_light.h"
#include "nvs_flash.h"
#include "esp_zigbee.h"
#include "esp_zigbee_types.h"

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile light (End Device) source code.
#endif

static const char *TAG = "ESP_ZB_ON_OFF_LIGHT";
static const char *TAG2 = "ZB_MESSAGING";

// Function to send a string message over Zigbee
void send_zigbee_message(const char *message) {
    esp_err_t ret;

    // Prepare Zigbee payload
    esp_zb_frame_t zb_frame = {
        .endpoint = ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_BASIC,
        .addr_mode = ESP_ZB_ADR_MODE_SHORT_NO_ACK,
        .dst_addr = 0x0000, // Example short address of the destination device
        .payload_length = strlen(message),
        .payload = (uint8_t *)message
    };

    // Send Zigbee message
    ret = esp_zigbee_send_packet(&zb_frame);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG2, "Failed to send Zigbee message: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG2, "Zigbee message sent successfully: %s", message);
    return ESP_OK;
}

/********************* Define functions **************************/
// Existing attribute callback function
void attr_cb(uint8_t status, uint8_t endpoint, uint16_t cluster_id, uint16_t attr_id, void *new_value) {
    if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        uint8_t value = *(uint8_t *)new_value;
        if (attr_id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
            ESP_LOGI(TAG, "on/off light set to %hd", value);
            light_driver_set_power((bool)value);
        }
    } else {
        ESP_LOGI(TAG, "cluster:0x%x, attribute:0x%x changed ", cluster_id, attr_id);
    }
}

// Existing Zigbee signal handler function
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
        case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
            ESP_LOGI(TAG, "Zigbee stack initialized");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
            break;
        case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
            if (err_status == ESP_OK) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                /* commissioning failed */
                ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %d)", err_status);
            }
            break;
        case ESP_ZB_BDB_SIGNAL_STEERING:
            if (err_status == ESP_OK) {
                esp_zb_ieee_addr_t extended_pan_id;
                esp_zb_get_extended_pan_id(extended_pan_id);
                ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d)",
                         extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                         extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                         esp_zb_get_pan_id(), esp_zb_get_current_channel());
            } else {
                ESP_LOGI(TAG, "Network steering was not successful (status: %d)", err_status);
                esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
            }
            break;
        case ESP_ZB_ZCL_SIGNAL_MSG:
            {
                esp_zb_zcl_received_t *zb_msg = (esp_zb_zcl_received_t *)signal_struct->p_app_signal;
                uint16_t cluster_id = zb_msg->cluster_id;
                uint16_t attr_id = zb_msg->attr_id;
                uint8_t endpoint = zb_msg->ep;
                uint8_t *payload = zb_msg->payload;
                uint16_t payload_length = zb_msg->payload_length;
                
                // Handle incoming message
                if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_BASIC && attr_id == ESP_ZB_ZCL_ATTR_BASIC_MODEL_ID) {
                    // Example: Handle basic cluster and model ID attribute
                    ESP_LOGI(TAG2, "Received message on endpoint %d: %.*s", endpoint, payload_length, payload);
                }
                // Add more handling for other clusters and attributes as needed

                break;
            }
        default:
            ESP_LOGI(TAG, "ZDO signal: %d, status: %d", sig_type, err_status);
            break;
    }
}

// Existing Zigbee task
static void esp_zb_task(void *pvParameters) {
    /* initialize Zigbee stack with Zigbee end-device config */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    /* set the on-off light device config */
    esp_zb_on_off_light_cfg_t light_cfg = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
    esp_zb_ep_list_t *esp_zb_on_off_light_ep = esp_zb_on_off_light_ep_create(HA_ESP_LIGHT_ENDPOINT, &light_cfg);
    esp_zb_device_register(esp_zb_on_off_light_ep);
    esp_zb_device_add_set_attr_value_cb(attr_cb);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

// Task to send Zigbee message
void send_zigbee_task(void *pvParameters) {
    const char *message = "Hello, Zigbee!";
    send_zigbee_message(message);
    vTaskDelete(NULL);
}

void app_main(void) {
    // Initialize Zigbee stack and configure Zigbee light
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    light_driver_init(LIGHT_DEFAULT_OFF);

    // Create Zigbee task
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);

    // Create task to send Zigbee message
    xTaskCreate(send_zigbee_task, "Send_Zigbee_Message", 4096, NULL, 5, NULL);
}
