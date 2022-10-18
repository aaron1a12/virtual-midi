# Virtual MIDI
Creates a virtual MIDI device for use in musical applications. Virtual MIDI can be used to filter or transform incoming musical notes from a source MIDI device to your desired Digital Audio Workstation (DAW) or MIDI host.

## Reason for this app
I wrote this program in 2018 to solve an urgent problem with the way Cubase handles sustained notes entered with a sustain pedal. It only supports two options (holding sustained notes and ignoring CC 64) but more filters could easily be added.

## How to use
1. Run the app and open it from the tray in the taskbar.
2. Under "Source Device," you will find the list of MIDI devices connected to your PC. If your device is not visible, click "Refresh."
 Click on the desired device and it will become the source of all MIDI commands filtered through Virtual MIDI.
3. In your DAW, be sure to always use "Virtual MIDI Device" as your MIDI source and not your actual connected device.

## Virtual MIDI SDK
This software depends on the virtualMIDI SDK, written by Tobias Erichsen.
