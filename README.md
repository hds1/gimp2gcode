# gimp2gcode
Plugin to convert 8Bit grayscale (without Alpha) to gcode (grbl)

Tested with Xubuntu 20.04, Gimp 2.10.18, grbl 1.1

Build:
Copy to folder and run "gimptool-2.0 --install gimp2gcode.c"
This will install the binary in your local ~/.config/gimp folder.

This plugin sholud compile under Gimp3, but this has not been tested.
Not tested on Windows or Macs.

The plugin comes with some default settings for GRBL.
Settings can be changed and saved to your HOMEDIR -> gimp2gcode.conf
which will be read in when the plugin is called next time.

Created NGC files will also be saved to your HOMEDIR.

Prepare picture:
- Convert to 8Bit grayscale
- Remove Alphachannel
- Apply Menu -> "Filter - Misc - gimp2gcode"
