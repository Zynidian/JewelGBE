# JewelGBE
Gameboy Emulator written in C++ using SFML

ROMs are ran by either typing the rom path after the executable in the command line, or by dragging the rom file onto the executable

This emulator has only been worked on for about a month as of now, so there are still compatablility and accuracy issues to work out.
Current Specifications:
    Runs original gameboy games, with limited compatability with dual-mode GBC games
    All CPU instructions implemented
    Graphics work with only minor issues in some games
    Sound emulation including noise channel
      -pulse sweeping, noise periods, and channel triggering still have some issues
    Supports the following mappers, along with RAM and .sav files when applicable:
      -ROM only
      -MBC1 (no multicarts, and mode switch not implemented)
      -MBC2 
      -MBC3 (no RTC support at the moment)
      -MBC5
    Compatability is a bit all over the place, but usually games will run either near perfectly, or can't run/crash early on
    
Builds using CMake (same process as https://github.com/SFML/cmake-sfml-project) (has not been tested on Linux/Mac yet!)
precompiled .exe files of versions (including ones made prior to first being uploaded to github) can be found in the versions folder.

![20240611](https://github.com/Zynidian/JewelGBE/assets/166747411/83186178-363a-434f-98a0-5286625cb6f8)
![20240528](https://github.com/Zynidian/JewelGBE/assets/166747411/0361b037-6394-4d0d-90db-0c0ecc8a8743)
![20240601a](https://github.com/Zynidian/JewelGBE/assets/166747411/630fab53-68e8-4df1-8d77-9ac9ae7c7203)
![20240525a](https://github.com/Zynidian/JewelGBE/assets/166747411/d72bd849-1f34-4e57-a538-f1896bca7fa3)
![20240601c](https://github.com/Zynidian/JewelGBE/assets/166747411/fb53da31-d303-4ad7-a12e-6ef0f0b489e0)
