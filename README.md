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


Output example:

<pre>
; gimp2gcode for Laser (grbl 1.1)
; Jan 2021 Heiko Schroeter
; Width: 600 [px], Height: 782 [px]
; Laserwidth: 0.15 [mm]
; {Width[mm] = laserwidth * PXwidth, Height[mm] = laserwidth * PXheight}
; Width: 90.0 [mm], Height: 117.3 [mm]
; Bottom Left corner of pic is Pos 0|0 for the laser.
; Laser plot direction is picture bottom up.
; File: (null)
;
;
G21 ; Set units to mm
G90 ; Use absolute coordinates
S0  ; Power off laser i.e. PWM=0
M4  ; Activate Laser with dynamics
F1500 ; Set speed
;
;
G00 X0 Y0 S0
;-->--
G01 X90.00 Y0.00 S50
G01 X90.00 Y0.15 S50 ;u
;--<--
G01 X0.00 Y0.15 S50
G01 X0.00 Y0.30 S50 ;u
.....
.....
;
M5 ; Laser Off
G00 X0 Y0 S0 ; romanus eunt domus ... 
</pre>
