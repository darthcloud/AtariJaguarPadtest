# BlueRetro Atari Jaguar Padtest

Base on JagStudio v1.0.
Support standard controller, team tap and 6D controller

## Build instructions

Within jagstudio directory:
```bash
build.bat padtest newc
# Replace default padtest.c with the one from this repo
build.bat padtest ROM
```

## Known issues
1. I think the jagstudio framework do it's own polling which sometimes conflict with the main one and create small glitches.
   No sure how to force the jagstudio polling off.
   
2. Frame rate is terrible but this program redetect the controller every frame which a normal games would only at boot time I assume.
