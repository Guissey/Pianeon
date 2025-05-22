#ifndef _USB_MIDI_HOST_H_
#define _USB_MIDI_HOST_H_

#include <midi_types.h>
#include <cstddef>

class UsbMidiHost {
  public:
    UsbMidiHost();
    ~UsbMidiHost();
    void setMidiInCallback(midi_in_callback_t *);
    void setup();
  private:
    midi_in_callback_t *midiInCallback = NULL;
};

#endif /* _USB_MIDI_HOST_H_ */
