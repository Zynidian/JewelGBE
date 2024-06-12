# JewelGBE
Jewel is a Gameboy Emulator written in C++ using SFML (although most of the code is written more like it's in standard C)

ROMs are ran by either typing the rom path after the executable in the command line, or by dragging the rom file onto the executable
This emulator has only been worked on for about a month as of now, so there are still compatablility and accuracy issues to work out.

## Specifications
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

## Building
Builds using CMake (same process as https://github.com/SFML/cmake-sfml-project) (has not been tested on Linux/Mac yet!)
precompiled .exe files of versions (including ones made prior to first being uploaded to github) can be found in the versions folder.

![1](https://github.com/Zynidian/JewelGBE/assets/166747411/e7ce9a95-504c-4a02-bca1-98cfb31d19a0)
![2](https://github.com/Zynidian/JewelGBE/assets/166747411/3a62cdee-5969-41cd-a1c7-ab54761cbe47)
![3](https://github.com/Zynidian/JewelGBE/assets/166747411/c63777c2-7e26-4f49-ad7a-fffbf5ceedd4)
![4](https://github.com/Zynidian/JewelGBE/assets/166747411/6feb99d1-cbf9-4b16-b4db-eacaa5562e09)
![5](https://github.com/Zynidian/JewelGBE/assets/166747411/3e82dd32-40cb-48d4-a431-a9ddc843b9bb)
