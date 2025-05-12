#include "UsbMidiHost.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "errno.h"
#include "driver/gpio.h"

#include <esp32-hal-log.h>
#include "hid_host.h" // Manually imported

#define APP_QUIT_PIN GPIO_NUM_0 // Boot button pin

QueueHandle_t app_event_queue = NULL;

/// Types declaration ///

/**
 * @brief APP event group
 *
 * Application logic can be different. There is a one among other ways to distinguish the
 * event by application event group.
 * In this example we have two event groups:
 * APP_EVENT            - General event, which is APP_QUIT_PIN press event (Generally, it is IO0).
 * APP_EVENT_HID_HOST   - HID Host Driver event, such as device connection/disconnection or input report.
 */
typedef enum {
  APP_EVENT = 0,
  APP_EVENT_HID_HOST
} app_event_group_t;

/**
 * @brief APP event queue
 *
 * This event is used for delivering the HID Host event from callback to a task.
 */
typedef struct {
  app_event_group_t event_group;
  /* HID Host - Device related info */
  struct {
    hid_host_device_handle_t handle;
    hid_host_driver_event_t event;
    void *arg;
  } hid_host_device;
} app_event_queue_t;

/**
 * @brief HID Protocol string names
 */
static const char *hid_proto_name_str[] = {
  "NONE",
  "KEYBOARD",
  "MOUSE"
};

/// Functions declaration ///

static void gpioIsrCallback(void* arg);
static void usbHostTask(void *arg);
void hidHostDeviceCallback(
  hid_host_device_handle_t hid_device_handle,
  const hid_host_driver_event_t event,
  void *arg
);
void hidHostDeviceEvent(
  hid_host_device_handle_t hid_device_handle,
  const hid_host_driver_event_t event,
  void *arg
);
void hidHostInterfaceCallback(
  hid_host_device_handle_t hid_device_handle,
  const hid_host_interface_event_t event,
  void *arg
);
void hidHostReportCallback(const uint8_t *const data, const int length);
static void hidPrintNewDeviceReportHeader(hid_protocol_t proto);

/// Class members definition ///

UsbMidiHost::UsbMidiHost() {

}

UsbMidiHost::~UsbMidiHost() {
  
}

void UsbMidiHost::setup() {
  // Attach callback to BOOT BUTTON to stop USB task
  ESP_LOGD("", "Attaching callback to BOOT button");
  const gpio_config_t input_pin = {
    .pin_bit_mask = BIT64(APP_QUIT_PIN),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .intr_type = GPIO_INTR_NEGEDGE,
  };
  ESP_ERROR_CHECK(gpio_config(&input_pin));
  ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
  ESP_ERROR_CHECK(gpio_isr_handler_add(APP_QUIT_PIN, gpioIsrCallback, NULL));

  // Create USB task
  ESP_LOGD("", "Creating USB task");
  BaseType_t usb_host_task;
  usb_host_task = xTaskCreatePinnedToCore(
    usbHostTask,
    "usb_host_midi_task",
    4096,
    xTaskGetCurrentTaskHandle(),
    2,
    NULL,
    0
  );
  assert(usb_host_task == pdTRUE);

  // Wait for notification from usb_host_midi_task to proceed
  ulTaskNotifyTake(false, 1000);

  /*
   * HID host driver configuration
   * - create background task for handling low level event inside the HID driver
   * - provide the device callback to get new HID Device connection event
   */
  const hid_host_driver_config_t hid_host_driver_config = {
    .create_background_task = true,
    .task_priority = 5,
    .stack_size = 4096,
    .core_id = 0,
    .callback = hidHostDeviceCallback,
    .callback_arg = NULL
  };
  ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

  // Create queue
  app_event_queue_t evt_queue;
  app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));
  ESP_LOGD("", "Waiting for HID Device to be connected");

  while (1) {
    // Wait queue
    if (xQueueReceive(app_event_queue, &evt_queue, portMAX_DELAY)) {
      if (APP_EVENT == evt_queue.event_group) {
        // User pressed button
        usb_host_lib_info_t lib_info;
        ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
        if (lib_info.num_devices == 0) {
          // End while cycle
          break;
        } else {
          ESP_LOGW("", "To shutdown example, remove all USB devices and press button again.");
          // Keep polling
        }
      }

      if (APP_EVENT_HID_HOST ==  evt_queue.event_group) {
        hidHostDeviceEvent(
          evt_queue.hid_host_device.handle,
          evt_queue.hid_host_device.event,
          evt_queue.hid_host_device.arg
        );
      }
    }

    ESP_LOGI("", "HID Driver uninstall");
    ESP_ERROR_CHECK(hid_host_uninstall());
    gpio_isr_handler_remove(APP_QUIT_PIN);
    xQueueReset(app_event_queue);
    vQueueDelete(app_event_queue);
  }
}

/// Functions definition ///

/**
 * BOOT button press callback. Exit HID host task.
 * @param[in] arg  Not used
 */
static void gpioIsrCallback(void* arg) {
  BaseType_t xTaskWoken = pdFALSE;
  const app_event_queue_t evt_queue = {
    .event_group = APP_EVENT,
  };

  if (app_event_queue) {
    xQueueSendFromISR(app_event_queue, &evt_queue, &xTaskWoken);
  }

  if (xTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

/**
 * Start USB Host Install and handle events
 * @param[in] arg  Not used
 */
static void usbHostTask(void *arg) {
  // Install USB
  ESP_LOGD("USB", "Installing USB host");
  const usb_host_config_t host_config = {
    .skip_phy_setup = false,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };
  ESP_ERROR_CHECK(usb_host_install(&host_config));

  // Handle events
  ESP_LOGD("", "Starting USB events handling");
  while (true) {
    uint32_t event_flags;
    usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    // If the single client unregistered, stop event handling process
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      ESP_ERROR_CHECK(usb_host_device_free_all());
      break;
    }
  }

  // Shutdown USB
  ESP_LOGD("", "Shutting down USB");
  vTaskDelay(10); // Short delay to allow clients clean-up
  ESP_ERROR_CHECK(usb_host_uninstall());
  vTaskDelete(NULL);
}

/**
 * @brief HID Host Device callback
 *
 * Puts new HID Device event to the queue
 *
 * @param[in] hid_device_handle HID Device handle
 * @param[in] event             HID Device event
 * @param[in] arg               Not used
 */
void hidHostDeviceCallback(
  hid_host_device_handle_t hid_device_handle,
  const hid_host_driver_event_t event,
  void *arg
) {
  const app_event_queue_t evt_queue = {
    .event_group = APP_EVENT_HID_HOST,
    // HID Host Device related info
    .hid_host_device = {
      .handle = hid_device_handle,
      .event = event,
      .arg = arg
    },
  };

  if (app_event_queue) {
    xQueueSend(app_event_queue, &evt_queue, 0);
  }
}

/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host Device event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hidHostDeviceEvent(
  hid_host_device_handle_t hid_device_handle,
  const hid_host_driver_event_t event,
  void *arg
) {
  hid_host_dev_params_t dev_params;
  ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

  switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED: {
      ESP_LOGD("", "HID Device, protocol '%s' CONNECTED", hid_proto_name_str[dev_params.proto]);

      const hid_host_device_config_t dev_config = {
        .callback = hidHostInterfaceCallback,
        .callback_arg = NULL
      };

      ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));
      if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
        ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
        if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
          ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
        }
      }
      ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
      break;
    }
    
    default:
      break;
  }
}

void hidHostInterfaceCallback(
  hid_host_device_handle_t hid_device_handle,
  const hid_host_interface_event_t event,
  void *arg
) {
  uint8_t data[64] = { 0 };
  size_t data_length = 0;
  hid_host_dev_params_t dev_params;
  ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

  switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
      ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(
        hid_device_handle,
        data,
        64,
        &data_length)
      );

      // if (HID_SUBCLASS_BOOT_INTERFACE == dev_params.sub_class) {
      //   if (HID_PROTOCOL_KEYBOARD == dev_params.proto) {
      //     hid_host_keyboard_report_callback(data, data_length);
      //   } else if (HID_PROTOCOL_MOUSE == dev_params.proto) {
      //     hid_host_mouse_report_callback(data, data_length);
      //   }
      // } else {
      //   hid_host_generic_report_callback(data, data_length);
      // }
      hidHostReportCallback(data, data_length);
      break;

    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
      ESP_LOGD(
        "", "HID Device, protocol '%s' DISCONNECTED",
        hid_proto_name_str[dev_params.proto]
      );
      ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
      break;

    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
      ESP_LOGD("", "HID Device, protocol '%s' TRANSFER_ERROR",
        hid_proto_name_str[dev_params.proto]);
      break;

    default:
      ESP_LOGE("", "HID Device, protocol '%s' Unhandled event",
        hid_proto_name_str[dev_params.proto]);
      break;
  }
}

/**
 * @brief USB HID Host report callback handler
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
void hidHostReportCallback(const uint8_t *const data, const int length) {
  hidPrintNewDeviceReportHeader(HID_PROTOCOL_NONE);
  for (int i = 0; i < length; i++) {
    printf("%02X", data[i]);
  }
  putchar('\r');
}

/**
 * @brief Makes new line depending on report output protocol type
 * @param[in] proto Current protocol to output
 */
static void hidPrintNewDeviceReportHeader(hid_protocol_t proto) {
  static hid_protocol_t prev_proto_output = HID_PROTOCOL_NONE;

  if (prev_proto_output != proto) {
    prev_proto_output = proto;
    printf("\r\n");
    if (proto == HID_PROTOCOL_MOUSE) {
      printf("Mouse\r\n");
    } else if (proto == HID_PROTOCOL_KEYBOARD) {
      printf("Keyboard\r\n");
    } else {
      printf("Generic\r\n");
    }
    fflush(stdout);
  }
}