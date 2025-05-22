#ifndef _MIDI_TYPES_H
#define _MIDI_TYPES_H

#include <cstdint>

typedef struct __attribute__((__packed__)) {
  uint8_t code_index_number : 4;
  uint8_t usb_cable_number : 4;
  uint8_t midi_channel : 4;
  uint8_t midi_type : 4;
  uint8_t midi_data_1;
  uint8_t midi_data_2;
} midi_usb_packet;

typedef void midi_in_callback_t(midi_usb_packet);

#endif /* _MIDI_TYPES_H */ 
