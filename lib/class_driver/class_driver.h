/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#ifndef _USB_MIDI_CLASS_H_
#define _USB_MIDI_CLASS_H_

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include <midi_types.h>

#define CLIENT_NUM_EVENT_MSG        5
#define DEV_MAX_COUNT               8

/// Types ///

typedef enum {
  ACTION_OPEN_DEV         = (1 << 0),
  ACTION_SETUP_MIDI       = (1 << 1),
  ACTION_GET_DEV_INFO     = (1 << 2),
  ACTION_GET_DEV_DESC     = (1 << 3),
  ACTION_GET_CONFIG_DESC  = (1 << 4),
  ACTION_GET_STR_DESC     = (1 << 5),
  ACTION_CLOSE_DEV        = (1 << 6),
} action_t;

typedef struct {
  usb_host_client_handle_t client_hdl;
  uint8_t dev_addr;
  usb_device_handle_t dev_hdl;
  uint8_t actions;
  usb_transfer_t *midi_in_transfer = NULL;
  uint8_t midi_interface_number;
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

/// Variables ///

static const char *TAG_MIDI_CLASS = "MIDI CLASS";
static class_driver_t *s_driver_obj;
static midi_in_callback_t *midiInCallbackPtr = NULL;

/// Functions declaration ///

static void clientEventCallback(const usb_host_client_event_msg_t*, void*);
static void classDriverDeviceHandle(usb_device_t*);
static void actionOpenDev(usb_device_t*);
static void actionSetupMidi(usb_device_t*);
static void actionGetInfo(usb_device_t*);
static void actionGetDevDesc(usb_device_t*);
static void actionGetConfigDesc(usb_device_t*);
static void actionGetStrDesc(usb_device_t*);
static void actionCloseDev(usb_device_t*);
static void transferCallback(usb_transfer_t*);
void classDriverTask(void*);
void classDriverClientUnregister(void);

/// Functions definition ///

static void clientEventCallback(const usb_host_client_event_msg_t *event_msg, void *arg) {
  class_driver_t *driver_obj = (class_driver_t *)arg;
  switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      ESP_LOGI(TAG_MIDI_CLASS, "New device connected");
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
      ESP_LOGI(TAG_MIDI_CLASS, "Device disconnected");
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

static void classDriverDeviceHandle(usb_device_t *device_obj) {
  uint8_t actions = device_obj->actions;
  device_obj->actions = 0;
  while (actions) {
    if (actions & ACTION_OPEN_DEV) {
      actionOpenDev(device_obj);
    }
    if (actions & ACTION_SETUP_MIDI) {
      actionSetupMidi(device_obj);
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

static void actionOpenDev(usb_device_t *device_obj) {
  assert(device_obj->dev_addr != 0);
  ESP_LOGI(TAG_MIDI_CLASS, "Opening device at address %d", device_obj->dev_addr);
  ESP_ERROR_CHECK(usb_host_device_open(device_obj->client_hdl, device_obj->dev_addr, &device_obj->dev_hdl));
  // Get the device's information next
  device_obj->actions |= ACTION_SETUP_MIDI;
}

/**
 * Look for a MIDI interface and claim it.
 * Then look for its IN endpoint and attach it a callback.
 */
static void actionSetupMidi(usb_device_t *device_obj) {
  const usb_config_desc_t *config_desc;
  ESP_ERROR_CHECK(usb_host_get_active_config_descriptor(device_obj->dev_hdl, &config_desc));
  
  const uint8_t *p = &config_desc->val[0];
  uint8_t bLength;
  bool is_midi_desc = false; // Indicates that we are reading a MIDI interface descriptor
  bool is_midi_in_endpoint_ready = false; // True if a MIDI IN endpoint has been found

  // Iterate over config desc
  for (int i = 0; i < config_desc->wTotalLength; i+=bLength, p+=bLength) {
    bLength = *p;
    if ((i + bLength) <= config_desc->wTotalLength) {
      const uint8_t bDescriptorType = *(p + 1);
      switch (bDescriptorType) {
        case USB_B_DESCRIPTOR_TYPE_INTERFACE: {
          // Cast as interface descriptor type
          const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;

          // Check if interface is MIDI and update is_midi_desc accordingly
          if ((intf->bInterfaceClass == USB_CLASS_AUDIO) &&
              (intf->bInterfaceSubClass == 3) &&
              (intf->bInterfaceProtocol == 0))
          {
            is_midi_desc = true;
            ESP_LOGD(TAG_MIDI_CLASS, "Claiming a MIDI device! number: %d, alt: %d", intf->bInterfaceNumber, intf->bAlternateSetting);
            // Claim MIDI interface
            ESP_ERROR_CHECK(usb_host_interface_claim(
              device_obj->client_hdl,
              device_obj->dev_hdl,
              intf->bInterfaceNumber,
              intf->bAlternateSetting
            ));
            device_obj->midi_interface_number = intf->bInterfaceNumber; // Used later to release interface on disconnection
          } else {
            is_midi_desc = false;
          }
          break;
        }
          
        case USB_B_DESCRIPTOR_TYPE_ENDPOINT: {
          if (!is_midi_desc) break;

          // Cast as endpoint descriptor
          const usb_ep_desc_t *endpoint = (const usb_ep_desc_t *)p;

          // Must be bulk for MIDI
          if ((endpoint->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) != USB_BM_ATTRIBUTES_XFER_BULK) {
            ESP_LOGD(TAG_MIDI_CLASS, "Not bulk endpoint: 0x%02x", endpoint->bmAttributes);
            break;
          }

          if (endpoint->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) {
            if (is_midi_in_endpoint_ready) break;
            // MIDI IN endpoint
            ESP_LOGD(TAG_MIDI_CLASS, "Setting up MIDI IN endpoint from address 0x%02x", endpoint->bEndpointAddress);
            ESP_ERROR_CHECK(usb_host_transfer_alloc(endpoint->wMaxPacketSize, 0, &device_obj->midi_in_transfer));
            device_obj->midi_in_transfer->device_handle = device_obj->dev_hdl;
            device_obj->midi_in_transfer->bEndpointAddress = endpoint->bEndpointAddress;
            device_obj->midi_in_transfer->num_bytes = endpoint->wMaxPacketSize;
            device_obj->midi_in_transfer->callback = transferCallback;
            ESP_ERROR_CHECK(usb_host_transfer_submit(device_obj->midi_in_transfer));
            is_midi_in_endpoint_ready = true;
            // Log to the serial port regardless of the log level to have the serial LED give a visual feedback
            log_printf("MIDI device connected\n");
          } else {
            // MIDI OUT endpoint
            ESP_LOGD(TAG_MIDI_CLASS, "Found MIDI OUT endpoint at address 0x%02x", endpoint->bEndpointAddress);
          }
          break;
        }
          
        default:
          break;
      }
    }
    else {
      ESP_LOGD(TAG_MIDI_CLASS, "USB Descriptor invalid");
      return;
    }
  }

  // Log device infos only if log level is info or above
  #ifdef CORE_DEBUG_LEVEL
  if (CORE_DEBUG_LEVEL >= ESP_LOG_INFO) {
    device_obj->actions |= ACTION_GET_DEV_INFO;
  }
  #endif // CORE_DEBUG_LEVEL
}

static void actionGetInfo(usb_device_t *device_obj) {
  assert(device_obj->dev_hdl != NULL);
  ESP_LOGI(TAG_MIDI_CLASS, "Getting device information");
  usb_device_info_t dev_info;
  ESP_ERROR_CHECK(usb_host_device_info(device_obj->dev_hdl, &dev_info));
  ESP_LOGI(TAG_MIDI_CLASS, "\t%s speed", (char const *[]) {
    "Low", "Full", "High"
  }[dev_info.speed]);
  ESP_LOGI(TAG_MIDI_CLASS, "\tbConfigurationValue %d", dev_info.bConfigurationValue);
  // Get the device descriptor next
  device_obj->actions |= ACTION_GET_DEV_DESC;
}

static void actionGetDevDesc(usb_device_t *device_obj) {
  assert(device_obj->dev_hdl != NULL);
  ESP_LOGI(TAG_MIDI_CLASS, "Getting device descriptor");
  const usb_device_desc_t *dev_desc;
  ESP_ERROR_CHECK(usb_host_get_device_descriptor(device_obj->dev_hdl, &dev_desc));
  usb_print_device_descriptor(dev_desc);
  // Get the device's config descriptor next
  device_obj->actions |= ACTION_GET_CONFIG_DESC;
}

static void actionGetConfigDesc(usb_device_t *device_obj) {
  assert(device_obj->dev_hdl != NULL);
  ESP_LOGI(TAG_MIDI_CLASS, "Getting config descriptor");
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
    ESP_LOGI(TAG_MIDI_CLASS, "Getting Manufacturer string descriptor");
    usb_print_string_descriptor(dev_info.str_desc_manufacturer);
  }
  if (dev_info.str_desc_product) {
    ESP_LOGI(TAG_MIDI_CLASS, "Getting Product string descriptor");
    usb_print_string_descriptor(dev_info.str_desc_product);
  }
  if (dev_info.str_desc_serial_num) {
    ESP_LOGI(TAG_MIDI_CLASS, "Getting Serial Number string descriptor");
    usb_print_string_descriptor(dev_info.str_desc_serial_num);
  }
  // No actions next
}

static void actionCloseDev(usb_device_t *device_obj) {
  if (device_obj->midi_in_transfer != NULL) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(usb_host_transfer_free(device_obj->midi_in_transfer));
    ESP_ERROR_CHECK_WITHOUT_ABORT(
      usb_host_interface_release(device_obj->client_hdl, device_obj->dev_hdl, device_obj->midi_interface_number)
    );
    device_obj->midi_in_transfer = NULL;
  }
  ESP_ERROR_CHECK(usb_host_device_close(device_obj->client_hdl, device_obj->dev_hdl));
  device_obj->dev_hdl = NULL;
  device_obj->dev_addr = 0;
}

/**
 * MIDI IN endpoint transfer callback.
 * Call *midiInCallbackPtr with the MIDI packet as parameter.
 */
static void transferCallback(usb_transfer_t *transfer) {
  int in_xfer = transfer->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK;
  if ((transfer->status == USB_TRANSFER_STATUS_COMPLETED) && in_xfer) {
    const midi_usb_packet *packets = (midi_usb_packet*)transfer->data_buffer;
    for (int i = 0; i < transfer->actual_num_bytes / 4; i += 1) {
      const midi_usb_packet packet = packets[i];
      if ((packet.usb_cable_number | packet.code_index_number | packet.midi_channel | packet.midi_type | packet.midi_data_1 | packet.midi_data_2) == 0) {
        break;
      }
      if (midiInCallbackPtr != NULL) {
        (*midiInCallbackPtr)(packet);
      }
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(usb_host_transfer_submit(transfer));
  }
}

/**
 * @param[in] arg  Midi in callback
 */
void classDriverTask(void *arg) {
  midiInCallbackPtr = (midi_in_callback_t*)arg;
  class_driver_t driver_obj = {0};
  usb_host_client_handle_t class_driver_client_hdl = NULL;
  
  ESP_LOGI(TAG_MIDI_CLASS, "Registering Client");
  SemaphoreHandle_t mux_lock = xSemaphoreCreateMutex();
  if (mux_lock == NULL) {
    ESP_LOGE(TAG_MIDI_CLASS, "Unable to create class driver mutex");
    vTaskSuspend(NULL);
    return;
  }
  usb_host_client_config_t client_config = {
    .is_synchronous = false,    // Synchronous clients currently not supported. Set this to false
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

  ESP_LOGI(TAG_MIDI_CLASS, "Deregistering Class Client");
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

#endif /* _USB_MIDI_CLASS_H_ */
