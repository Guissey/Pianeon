#ifndef _USB_MIDI_HOST_H_
#define _USB_MIDI_HOST_H_

#include <midi_types.h>

class UsbMidiHost {
  public:
    UsbMidiHost();
    ~UsbMidiHost();
    void setMidiInCallback(midi_in_callback_t *);
    void setup();
  private:
    midi_in_callback_t *midiInCallback;
};

#endif /* _USB_MIDI_HOST_H_ */
