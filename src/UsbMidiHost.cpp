#include "UsbMidiHost.h"

/**
 * Implementation is widely inspired by esp-idf usb host example
 * https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/host/usb_host_lib
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include "driver/gpio.h"

#include <esp32-hal-log.h>
#include "class_driver.h"

#define HOST_LIB_TASK_PRIORITY    2
#define CLASS_TASK_PRIORITY       3
#define INTERRUPT_TASK_PRIORITY   3
#define APP_QUIT_PIN GPIO_NUM_0 // Pin used to stop USB task app, usually Boot button pin

#ifdef CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK
#define ENABLE_ENUM_FILTER_CALLBACK
#endif // CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK

/// Types ///

/**
 * @brief APP event group
 * APP_EVENT            - General event, which is APP_QUIT_PIN press event in this example.
 */
typedef enum {
  APP_EVENT = 0,
} app_event_group_t;

/**
 * @brief APP event queue
 * This event is used for delivering events from callback to a task.
 */
typedef struct {
  app_event_group_t event_group;
} app_event_queue_t;

/// Variables ///

static const char *TAG_USB_host = "USB host lib";
static app_event_queue_t evt_queue;
static QueueHandle_t app_event_queue = NULL;
static TaskHandle_t host_lib_task_hdl, class_driver_task_hdl;

/// Functions declaration ///

static void gpioIsrCallback(void*);
static void usbHostTask(void*);
static void checkInterruptTask(void*);

#ifdef ENABLE_ENUM_FILTER_CALLBACK
static bool set_config_cb(const usb_device_desc_t*, uint8_t*)
#endif // ENABLE_ENUM_FILTER_CALLBACK

/// Class members definition ///

UsbMidiHost::UsbMidiHost() {

}

UsbMidiHost::~UsbMidiHost() {
  
}

void UsbMidiHost::setMidiInCallback(midi_in_callback_t *callback) {
  this->midiInCallback = callback;
}

void UsbMidiHost::setup() {
  // Attach callback to BOOT BUTTON to stop USB task
  ESP_LOGD(TAG_USB_host, "Attaching callback to BOOT button");
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
  ESP_LOGD(TAG_USB_host, "Creating USB task");
  app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));

  BaseType_t task_created;

  task_created = xTaskCreatePinnedToCore(
    usbHostTask,
    "usb_host_midi_task",
    4096,
    xTaskGetCurrentTaskHandle(),
    HOST_LIB_TASK_PRIORITY,
    &host_lib_task_hdl,
    0
  );
  assert(task_created == pdTRUE);

  // Wait until the USB host library is installed
  ulTaskNotifyTake(false, 1000);

  // Create class driver task
  task_created = xTaskCreatePinnedToCore(
    classDriverTask,
    "class",
    5 * 1024,
    (void*)this->midiInCallback,
    CLASS_TASK_PRIORITY,
    &class_driver_task_hdl,
    0
  );
  assert(task_created == pdTRUE);
  // Add a short delay to let the tasks run
  vTaskDelay(10);

  // Create interruption check task
  task_created = xTaskCreatePinnedToCore(
    checkInterruptTask,
    "interruptTask",
    2048,
    NULL,
    INTERRUPT_TASK_PRIORITY,
    NULL,
    1
  );
  assert(task_created == pdTRUE);
}

/// Functions definition ///

/**
 * BOOT button press callback. Exit Host lib task.
 * @param[in] arg  Not used
 */
static void gpioIsrCallback(void* arg) {
  const app_event_queue_t evt_queue = {
    .event_group = APP_EVENT,
  };
  BaseType_t xTaskWoken = pdFALSE;

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
  // Install USB host lib
  ESP_LOGI(TAG_USB_host, "Installing USB Host Library");
  const usb_host_config_t host_config = {
    .skip_phy_setup = false,
    .intr_flags = ESP_INTR_FLAG_LEVEL1,
# ifdef ENABLE_ENUM_FILTER_CALLBACK
    .enum_filter_cb = set_config_cb,
# endif // ENABLE_ENUM_FILTER_CALLBACK
  };
  ESP_ERROR_CHECK(usb_host_install(&host_config));

  // Handle events
  ESP_LOGI(TAG_USB_host, "Starting USB events handling");
  bool has_clients = true;
  bool has_devices = false;
  while (has_clients) {
    uint32_t event_flags;
    ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      ESP_LOGI(TAG_USB_host, "Get FLAGS_NO_CLIENTS");
      if (ESP_OK == usb_host_device_free_all()) {
        ESP_LOGI(TAG_USB_host, "All devices marked as free, no need to wait FLAGS_ALL_FREE event");
        has_clients = false;
      } else {
        ESP_LOGI(TAG_USB_host, "Wait for the FLAGS_ALL_FREE");
        has_devices = true;
      }
      if (has_devices && event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
        ESP_LOGI(TAG_USB_host, "Get FLAGS_ALL_FREE");
        has_clients = false;
      }
    }
  }

  // Shutdown USB
  ESP_LOGI(TAG_USB_host, "No more clients and devices, uninstall USB Host library");
  //Uninstall the USB Host Library
  ESP_ERROR_CHECK(usb_host_uninstall());
  vTaskSuspend(NULL);
}

/**
 * 
 */
static void checkInterruptTask(void *arg) {
  ESP_LOGD(TAG_USB_host, "Start interrupt task on core %d", xPortGetCoreID());
  while (1) {
    // Check queue to know if interruption button has been pressed
    if (xQueueReceive(app_event_queue, &evt_queue, 0)) {
      if (APP_EVENT == evt_queue.event_group) {
        // User pressed button
        usb_host_lib_info_t lib_info;
        ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
        if (lib_info.num_devices != 0) {
          ESP_LOGW(TAG_USB_host, "Shutdown with attached devices.");
        }
        // End while cycle
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  ESP_LOGD(TAG_USB_host, "Shutdown USB task");
  
  // Deregister client
  classDriverClientUnregister();
  vTaskDelay(10);

  // Delete the tasks
  vTaskDelete(class_driver_task_hdl);
  vTaskDelete(host_lib_task_hdl);

  // Delete interrupt and queue
  gpio_isr_handler_remove(APP_QUIT_PIN);
  xQueueReset(app_event_queue);
  vQueueDelete(app_event_queue);

  // Delete own task
  vTaskDelete(NULL);
}

/**
 * @brief Set configuration callback
 * Set the USB device configuration during the enumeration process, must be enabled in the menuconfig
 * @note bConfigurationValue starts at index 1
 * @param[in] dev_desc device descriptor of the USB device currently being enumerated
 * @param[out] bConfigurationValue configuration descriptor index, that will be user for enumeration
 * @return bool
 * - true:  USB device will be enumerated
 * - false: USB device will not be enumerated
 */
#ifdef ENABLE_ENUM_FILTER_CALLBACK
static bool set_config_cb(const usb_device_desc_t *dev_desc, uint8_t *bConfigurationValue)
{
    // If the USB device has more than one configuration, set the second configuration
    if (dev_desc->bNumConfigurations > 1) {
        *bConfigurationValue = 2;
    } else {
        *bConfigurationValue = 1;
    }

    // Return true to enumerate the USB device
    return true;
}
#endif // ENABLE_ENUM_FILTER_CALLBACK
