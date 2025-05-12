/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "usb/usb_host.h"

#define CLIENT_NUM_EVENT_MSG        5
#define DEV_MAX_COUNT               8

/// Types definition ///

typedef enum {
  ACTION_OPEN_DEV         = (1 << 0),
  ACTION_GET_DEV_INFO     = (1 << 1),
  ACTION_GET_DEV_DESC     = (1 << 2),
  ACTION_GET_CONFIG_DESC  = (1 << 3),
  ACTION_GET_STR_DESC     = (1 << 4),
  ACTION_CLOSE_DEV        = (1 << 5),
} action_t;

typedef struct {
  usb_host_client_handle_t client_hdl;
  uint8_t dev_addr;
  usb_device_handle_t dev_hdl;
  uint8_t actions;
} usb_device_t;

typedef struct {
  struct {
    union {
      struct {
        uint8_t unhandled_devices: 1;   /**< Device has unhandled devices */
        uint8_t shutdown: 1;            /**<  */
        uint8_t reserved6: 6;           /**< Reserved */
      };
      uint8_t val;                      /**< Class drivers' flags value */
    } flags;                            /**< Class drivers' flags */
    usb_device_t device[DEV_MAX_COUNT]; /**< Class drivers' static array of devices */
  } mux_protected;                      /**< Mutex protected members. Must be protected by the Class mux_lock when accessed */

  struct {
    usb_host_client_handle_t client_hdl;
    SemaphoreHandle_t mux_lock;         /**< Mutex for protected members */
  } constant;                           /**< Constant members. Do not change after installation thus do not require a critical section or mutex */
} class_driver_t;

/// Variables declaration ///

static const char *TAG_USB_CLASS = "CLASS";
static class_driver_t *s_driver_obj;

/// Functions declaration ///

static void clientEventCallback(const usb_host_client_event_msg_t*, void*);
static void actionOpenDev(usb_device_t*);
static void actionGetInfo(usb_device_t*);
static void actionGetDevDesc(usb_device_t*);
static void actionGetConfigDesc(usb_device_t*);
static void actionGetStrDesc(usb_device_t*);
static void actionCloseDev(usb_device_t*);
static void classDriverDeviceHandle(usb_device_t*);
void classDriverTask(void*);
void classDriverClientUnregister(void);

/// Functions definition ///

static void clientEventCallback(const usb_host_client_event_msg_t *event_msg, void *arg) {
  class_driver_t *driver_obj = (class_driver_t *)arg;
  switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      // Save the device address
      xSemaphoreTake(driver_obj->constant.mux_lock, portMAX_DELAY);
      driver_obj->mux_protected.device[event_msg->new_dev.address].dev_addr = event_msg->new_dev.address;
      driver_obj->mux_protected.device[event_msg->new_dev.address].dev_hdl = NULL;
      // Open the device next
      driver_obj->mux_protected.device[event_msg->new_dev.address].actions |= ACTION_OPEN_DEV;
      // Set flag
      driver_obj->mux_protected.flags.unhandled_devices = 1;
      xSemaphoreGive(driver_obj->constant.mux_lock);
      break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      // Cancel any other actions and close the device next
      xSemaphoreTake(driver_obj->constant.mux_lock, portMAX_DELAY);
      for (uint8_t i = 0; i < DEV_MAX_COUNT; i++) {
        if (driver_obj->mux_protected.device[i].dev_hdl == event_msg->dev_gone.dev_hdl) {
          driver_obj->mux_protected.device[i].actions = ACTION_CLOSE_DEV;
          // Set flag
          driver_obj->mux_protected.flags.unhandled_devices = 1;
        }
      }
      xSemaphoreGive(driver_obj->constant.mux_lock);
      break;
    default:
      // Should never occur
      abort();
  }
}

static void actionOpenDev(usb_device_t *device_obj) {
  assert(device_obj->dev_addr != 0);
  ESP_LOGI(TAG_USB_CLASS, "Opening device at address %d", device_obj->dev_addr);
  ESP_ERROR_CHECK(usb_host_device_open(device_obj->client_hdl, device_obj->dev_addr, &device_obj->dev_hdl));
  // Get the device's information next
  device_obj->actions |= ACTION_GET_DEV_INFO;
}

static void actionGetInfo(usb_device_t *device_obj) {
  assert(device_obj->dev_hdl != NULL);
  ESP_LOGI(TAG_USB_CLASS, "Getting device information");
  usb_device_info_t dev_info;
  ESP_ERROR_CHECK(usb_host_device_info(device_obj->dev_hdl, &dev_info));
  ESP_LOGI(TAG_USB_CLASS, "\t%s speed", (char const *[]) {
    "Low", "Full", "High"
  }[dev_info.speed]);
  ESP_LOGI(TAG_USB_CLASS, "\tParent info:");
  // if (dev_info.parent.dev_hdl) {
  //   usb_device_info_t parent_dev_info;
  //   ESP_ERROR_CHECK(usb_host_device_info(dev_info.parent.dev_hdl, &parent_dev_info));
  //   ESP_LOGI(TAG_USB_CLASS, "\t\tBus addr: %d", parent_dev_info.dev_addr);
  //   ESP_LOGI(TAG_USB_CLASS, "\t\tPort: %d", dev_info.parent.port_num);
  // } else {
  //   ESP_LOGI(TAG_USB_CLASS, "\t\tPort: ROOT");
  // }
  ESP_LOGI(TAG_USB_CLASS, "\tbConfigurationValue %d", dev_info.bConfigurationValue);
  // Get the device descriptor next
  device_obj->actions |= ACTION_GET_DEV_DESC;
}

static void actionGetDevDesc(usb_device_t *device_obj) {
  assert(device_obj->dev_hdl != NULL);
  ESP_LOGI(TAG_USB_CLASS, "Getting device descriptor");
  const usb_device_desc_t *dev_desc;
  ESP_ERROR_CHECK(usb_host_get_device_descriptor(device_obj->dev_hdl, &dev_desc));
  usb_print_device_descriptor(dev_desc);
  // Get the device's config descriptor next
  device_obj->actions |= ACTION_GET_CONFIG_DESC;
}

static void actionGetConfigDesc(usb_device_t *device_obj) {
  assert(device_obj->dev_hdl != NULL);
  ESP_LOGI(TAG_USB_CLASS, "Getting config descriptor");
  const usb_config_desc_t *config_desc;
  ESP_ERROR_CHECK(usb_host_get_active_config_descriptor(device_obj->dev_hdl, &config_desc));
  usb_print_config_descriptor(config_desc, NULL);
  // Get the device's string descriptors next
  device_obj->actions |= ACTION_GET_STR_DESC;
}

static void actionGetStrDesc(usb_device_t *device_obj) {
  assert(device_obj->dev_hdl != NULL);
  usb_device_info_t dev_info;
  ESP_ERROR_CHECK(usb_host_device_info(device_obj->dev_hdl, &dev_info));
  if (dev_info.str_desc_manufacturer) {
    ESP_LOGI(TAG_USB_CLASS, "Getting Manufacturer string descriptor");
    usb_print_string_descriptor(dev_info.str_desc_manufacturer);
  }
  if (dev_info.str_desc_product) {
    ESP_LOGI(TAG_USB_CLASS, "Getting Product string descriptor");
    usb_print_string_descriptor(dev_info.str_desc_product);
  }
  if (dev_info.str_desc_serial_num) {
    ESP_LOGI(TAG_USB_CLASS, "Getting Serial Number string descriptor");
    usb_print_string_descriptor(dev_info.str_desc_serial_num);
  }
}

static void actionCloseDev(usb_device_t *device_obj) {
  ESP_ERROR_CHECK(usb_host_device_close(device_obj->client_hdl, device_obj->dev_hdl));
  device_obj->dev_hdl = NULL;
  device_obj->dev_addr = 0;
}

static void classDriverDeviceHandle(usb_device_t *device_obj) {
  uint8_t actions = device_obj->actions;
  device_obj->actions = 0;
  while (actions) {
    if (actions & ACTION_OPEN_DEV) {
      actionOpenDev(device_obj);
    }
    if (actions & ACTION_GET_DEV_INFO) {
      actionGetInfo(device_obj);
    }
    if (actions & ACTION_GET_DEV_DESC) {
      actionGetDevDesc(device_obj);
    }
    if (actions & ACTION_GET_CONFIG_DESC) {
      actionGetConfigDesc(device_obj);
    }
    if (actions & ACTION_GET_STR_DESC) {
      actionGetStrDesc(device_obj);
    }
    if (actions & ACTION_CLOSE_DEV) {
      actionCloseDev(device_obj);
    }

    actions = device_obj->actions;
    device_obj->actions = 0;
  }
}

/**
 * 
 */
void classDriverTask(void *arg) {
  class_driver_t driver_obj = {0};
  usb_host_client_handle_t class_driver_client_hdl = NULL;
  
  ESP_LOGI(TAG_USB_CLASS, "Registering Client");
  SemaphoreHandle_t mux_lock = xSemaphoreCreateMutex();
  if (mux_lock == NULL) {
    ESP_LOGE(TAG_USB_CLASS, "Unable to create class driver mutex");
    vTaskSuspend(NULL);
    return;
  }
  usb_host_client_config_t client_config = {
    .is_synchronous = false,    //Synchronous clients currently not supported. Set this to false
    .max_num_event_msg = CLIENT_NUM_EVENT_MSG,
    .async = {
      .client_event_callback = clientEventCallback,
      .callback_arg = (void *) &driver_obj,
    },
  };
  ESP_ERROR_CHECK(usb_host_client_register(&client_config, &class_driver_client_hdl));

  driver_obj.constant.mux_lock = mux_lock;
  driver_obj.constant.client_hdl = class_driver_client_hdl;
  for (uint8_t i = 0; i < DEV_MAX_COUNT; i++) {
    driver_obj.mux_protected.device[i].client_hdl = class_driver_client_hdl;
  }
  s_driver_obj = &driver_obj;

  while (1) {
    // Driver has unhandled devices, handle all devices first
    if (driver_obj.mux_protected.flags.unhandled_devices) {
      xSemaphoreTake(driver_obj.constant.mux_lock, portMAX_DELAY);
      for (uint8_t i = 0; i < DEV_MAX_COUNT; i++) {
        if (driver_obj.mux_protected.device[i].actions) {
          classDriverDeviceHandle(&driver_obj.mux_protected.device[i]);
        }
      }
      driver_obj.mux_protected.flags.unhandled_devices = 0;
      xSemaphoreGive(driver_obj.constant.mux_lock);
    } else {
      // Driver is active, handle client events
      if (driver_obj.mux_protected.flags.shutdown == 0) {
        usb_host_client_handle_events(class_driver_client_hdl, portMAX_DELAY);
      } else {
        // Shutdown the driver
        break;
      }
    }
  }

  ESP_LOGI(TAG_USB_CLASS, "Deregistering Class Client");
  ESP_ERROR_CHECK(usb_host_client_deregister(class_driver_client_hdl));
  if (mux_lock != NULL) {
    vSemaphoreDelete(mux_lock);
  }
  vTaskSuspend(NULL);
}

/**
 * 
 */
void classDriverClientUnregister(void) {
  // Mark all opened devices
  xSemaphoreTake(s_driver_obj->constant.mux_lock, portMAX_DELAY);
  for (uint8_t i = 0; i < DEV_MAX_COUNT; i++) {
    if (s_driver_obj->mux_protected.device[i].dev_hdl != NULL) {
      // Mark device to close
      s_driver_obj->mux_protected.device[i].actions |= ACTION_CLOSE_DEV;
      // Set flag
      s_driver_obj->mux_protected.flags.unhandled_devices = 1;
    }
  }
  s_driver_obj->mux_protected.flags.shutdown = 1;
  xSemaphoreGive(s_driver_obj->constant.mux_lock);

  // Unblock, exit the loop and proceed to deregister client
  ESP_ERROR_CHECK(usb_host_client_unblock(s_driver_obj->constant.client_hdl));
}