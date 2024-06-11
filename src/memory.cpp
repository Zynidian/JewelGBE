#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include "io.h"
#include "cpu.h"
#include "memory.h"
#include "sound.h"

bool mbc1_mode = false;
short mbc5_bank = 0;

void init_ff_256(){
    for (int i = 0; i < 256; i++){
        ff_256[i] = 0xff;
    }
    std::copy(&ff_256[0], &ff_256[256], &m[256]);
}

const unsigned char mapper_number_conversion[256] = {0,1,0x81,0xc1,0,0x82,0xc2,0,0x80,0xc0,0,0,0,0,0,0x63,0xe3,3,0x83,0xc3,5,0x85,0xc5,5,0x85,0xc5};
const int mapper_rom_sizes[9] = {0x8000,0x10000,0x20000,0x40000,0x80000,0x100000,0x200000,0x400000,0x800000,};
const int mapper_ram_sizes[6] = {0x2000,0x2000,0x2000,0x8000,0x20000,0x10000};

bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

void decode_header(){
    mapper_number = mapper_number_conversion[m[0x0147]] & 0x1f;
    has_ram = (mapper_number_conversion[m[0x0147]] & 0x80) != 0;
    has_battery = (mapper_number_conversion[m[0x0147]] & 0x40) != 0;
    has_rtc = (mapper_number_conversion[m[0x0147]] & 0x20) != 0;
    rom_size = mapper_rom_sizes[m[0x0148] % 9];
    ram_size = mapper_ram_sizes[m[0x0149] % 6];

    if (cart_size < rom_size){
        rom_size = cart_size & 0xff8000;
    }
    switch (mapper_number){
        case 2: 
            ram_size = 512;
            has_ram = true;
            rom_size = rom_size*(rom_size < 0x40000) + 0x40000*(rom_size >= 0x40000);
            break;
        default: break;
    }
    game_sram = (unsigned char*)std::malloc(ram_size);
    std::fill(&game_sram[0], &game_sram[ram_size], 0xff);
}

void unload_rom(){
    std::free(game_rom);
}

void copy_oam(unsigned char data){
    unsigned short oam_memory_to_copy = (data << 8);
    for (int i = 0; i < 0xa0; i++){
        write((0xfe00 + i), read(oam_memory_to_copy + i));
    }
}

void save_file(std::string name){
    if (has_ram && has_battery){
        replace(name, ".gbc", ".gb");
        replace(name, ".gb", ".sav");
        std::ofstream outstream(name, std::ofstream::trunc | std::ofstream::binary);
        
        if (outstream.is_open()){
            for (int i = 0; i < ram_size; i++){
                outstream.put((char)game_sram[i]);
            }
            outstream.close();
        } else {
            std::cout << "could not write to SAV file!!";
        }
    }
}

void load_save_file(char* filepath){
    sf::FileInputStream stream;
    std::string text_buffer("'");
    text_buffer.append(filepath);

    if (stream.open(filepath)) {
        if (ram_size < 0x1ffff){
            unsigned char* ram_buffer = (unsigned char*)std::malloc(stream.getSize());
            stream.read(ram_buffer,stream.getSize());
            text_buffer.append("'\n");
            std::cout << text_buffer;
            std::copy(&ram_buffer[0], &ram_buffer[ram_size], &game_sram[0]);
        } else{
            text_buffer.append("' \n   ERROR: RAM exceeds max size of 0x1ffff\n");
            std::cout << text_buffer;
        }
    }
    else{
        text_buffer.append("' \n   WARNING: SAV file not found\n");
        std::cout << text_buffer;
    }
}

unsigned char* load_rom(char* filepath, unsigned char* default_rom){
    sf::FileInputStream stream;
    std::string text_buffer("'");
    text_buffer.append(filepath);

    if (stream.open(filepath)) {
        //todo: if a new ROM is loaded, the pointer that is returned needs to be free()'d first
        cart_size = stream.getSize();
        if (cart_size < 0x7ffffff){
            unsigned char* rom_buffer = (unsigned char*)std::malloc(stream.getSize());
            stream.read(rom_buffer,stream.getSize());
            text_buffer.append("'\n");
            std::cout << text_buffer;
            return rom_buffer;
        } else{
            text_buffer.append("' \n   ERROR: ROM exceeds max size of 0x7ffffff\n");
            std::cout << text_buffer;
            return default_rom;
        }
    }
    else{
        text_buffer.append("' \n   ERROR: ROM not found\n");
        std::cout << text_buffer;
        return default_rom;
    }
}


void write_mbc1(unsigned short address, char data){
    unsigned int bank_addr;
    unsigned int bank_addr_end;
    switch (address & 0xe000){
        case 0x0000://ram enable
            sram_enable = (data & 0x0f) == 0x0a;
            break;
        case 0x2000://rom bank select
            data = data & 0x1f;
            if (data == 0){
                data = 1;
            }
            bank_addr = (data*0x4000) % rom_size;
            bank_addr_end = bank_addr + 0x4000;
            std::copy(&game_rom[bank_addr], &game_rom[bank_addr_end], &m[0x4000]);
            break;
        case 0x4000:
            if (mbc1_mode){
                //idrk how this works right now lmao
            } else{
                if (data >= 0x04){
                    break;
                }
                data = data & 0x03;
                sram_bank_offset = (data*0x2000) % ram_size;
            }
            break;
        case 0x6000:
            mbc1_mode = (data & 0x01) == 1;
            break;
        case 0xa000:
            if (sram_enable && has_ram){
                game_sram[sram_bank_offset + (address - 0xa000)] = data;
            }; break;
        default: break;
    }
}

void write_mbc2(unsigned short address, char data){
    unsigned int bank_addr;
    unsigned int bank_addr_end;
    switch(address & 0xe100){
        case 0x0000://ram enable
        case 0x2000:
            sram_enable = (data & 0x0f) == 0x0a;
            break;
        case 0x0100:
        case 0x2100:
            //rom bank select
            data = data & 0x0f;
            if (data == 0){
                data = 1;
            }
            bank_addr = (data*0x4000) % rom_size;
            bank_addr_end = bank_addr + 0x4000;
            std::copy(&game_rom[bank_addr], &game_rom[bank_addr_end], &m[0x4000]);
            break;
        case 0xa000:
        case 0xa100:
            //mirror 512 bytes and dont allow upper nybble
            if (sram_enable){
                game_sram[address & 0x1ff] = data & 0x0f;
            }
        default: break;
    }
}

void write_mbc3(unsigned short address, char data){
    unsigned int bank_addr;
    unsigned int bank_addr_end;
    switch (address & 0xe000){
        case 0x0000://ram enable
            sram_enable = (data & 0x0f) == 0x0a;
            break;
        case 0x2000://rom bank select
            data = data & 0x7f;
            if (data == 0){
                data = 1;
            }
            bank_addr = (data*0x4000) % rom_size;
            bank_addr_end = bank_addr + 0x4000;
            std::copy(&game_rom[bank_addr], &game_rom[bank_addr_end], &m[0x4000]);
            break;
        case 0x4000:
            if (data > 3){
                rtc_in_a000 = true;
                break;
            }
            rtc_in_a000 = false;
            sram_bank_offset = (data*0x2000) % ram_size;
            break;
        case 0xa000:
            if (sram_enable && has_ram){
                game_sram[sram_bank_offset + (address - 0xa000)] = data;
            }; break;
        default: break;
    }
}

void write_mbc5(unsigned short address, char data){
    unsigned int bank_addr;
    unsigned int bank_addr_end;
    switch (address & 0xf000){
        case 0x0000://ram enable
        case 0x1000:
            sram_enable = (data & 0x0f) == 0x0a;
            break;
        case 0x2000://rom bank lower 8 bits
            mbc5_bank = (mbc5_bank & 0x100) + data;
            bank_addr = (mbc5_bank*0x4000) % rom_size;
            bank_addr_end = bank_addr + 0x4000;
            std::copy(&game_rom[bank_addr], &game_rom[bank_addr_end], &m[0x4000]);
            break;
        case 0x3000://rom bank highest bit
            mbc5_bank = (mbc5_bank & 0xff) + ((data & 0x01) * 0x100);
            bank_addr = (mbc5_bank*0x4000) % rom_size;
            bank_addr_end = bank_addr + 0x4000;
            std::copy(&game_rom[bank_addr], &game_rom[bank_addr_end], &m[0x4000]);
            break;
        case 0x4000:
        case 0x5000:
            if (data >= 0x10){
                break;
            }
            data = data & 0x0f;
            sram_bank_offset = (data*0x2000) % ram_size;
            break;
        case 0x6000:
            break;
        case 0x7000:
            break;
        case 0xa000:
        case 0xb000:
            if (sram_enable && has_ram){
                game_sram[sram_bank_offset + (address - 0xa000)] = data;
            }; break;
        default: break;
    }
}

void write(unsigned short address, unsigned char data){
    unsigned short oam_memory_to_copy = 0;
    //mapper registers and cart ram access
    if ((address < 0x8000) || ((address < 0xc000) && (address >= 0xa000))){
        switch (mapper_number){
            case 1: write_mbc1(address, data); break;
            case 2: write_mbc2(address, data); break;
            case 3: write_mbc3(address, data); break;
            case 5: write_mbc5(address, data); break;
            default: m[address] = data;
        }
    } else {
        switch (address){
            case 0xff00:
                data = (data & 0b00110000) | (m[0xff00] & 0b00001111) | 0b11000000;
                m[0xff00] = data;
                get_button_state();
                break;
            case 0xff01: break;
            case 0xff02: break;
            case DIVIDER_REG:
                m[DIVIDER_REG] = 0;
                divider_counter = 0;
                break;
            case OAM_DMA:
                copy_oam(data);
                break;
            case CH_PULSE_1_PERIOD_SWEEP: m[address] = data; set_pulse_period_sweep(data); break;
            case CH_PULSE_1_DUTY_LENGTH: m[address] = data; set_pulse_duty_length(false,data); break;
            case CH_PULSE_1_VOLUME_ENV: m[address] = data; set_channel_volume(1,data); break;
            case CH_PULSE_1_FREQ_HI_CTRL:
                pulse_1_period.H = data; m[address] = (data & 0x40); 
                set_pulse_pitch(false,pulse_1_period); break;
            case CH_PULSE_1_FREQ_LO:
                pulse_1_period.L = data;
                set_pulse_pitch(false,pulse_1_period); break;
            case CH_PULSE_2_DUTY_LENGTH: m[address] = data; set_pulse_duty_length(true,data); break;
            case CH_PULSE_2_VOLUME_ENV: m[address] = data; set_channel_volume(2,data); break;
            case CH_PULSE_2_FREQ_HI_CTRL:
                pulse_2_period.H = data; m[address] = (data & 0x40); 
                set_pulse_pitch(true,pulse_2_period); break;
            case CH_PULSE_2_FREQ_LO:
                pulse_2_period.L = data;
                set_pulse_pitch(true,pulse_2_period); break;
            case CH_WAVE_ENABLE: m[address] = data; set_wave_enable(); break;
            case CH_WAVE_LENGTH_TIMER: set_wave_length(data); break;
            case CH_WAVE_FREQ_HI_CTRL:
                wave_period_hi = data; m[address] = (data & 0x40); set_wave_pitch(); break;
            case CH_WAVE_FREQ_LO: wave_period_lo = data; set_wave_pitch(); break;
            case CH_WAVE_VOLUME: m[address] = data; set_wave_volume(); break;
            case CH_NOISE_LENGTH_TIMER: set_noise_length(data); break;
            case CH_NOISE_VOLUME_ENV: m[address] = data; set_channel_volume(3,data); break;
            case CH_NOISE_TONE: m[address] = data; set_noise_tone(data); break;
            case CH_NOISE_CONTROL: m[address] = (data & 0x40); set_noise_control(data); break;
            case AUDIO_MASTER_VOLUME: m[address] = data; set_audio_master_volume(data); break;
            case AUDIO_MASTER_ENABLE: m[address] = data; refresh_audio_master_enable(); break;
            default:
                m[address] = data;
        }
    }
}

void write(unsigned short address, struct word data){
    write(address, data.L);
    write(address+1, data.H);
}


unsigned char read_mbc1_ram(unsigned short address){
    if (sram_enable && has_ram){
        return game_sram[sram_bank_offset + (address - 0xa000)];
    } else{
        return 0xff;
    }
};

unsigned char read_mbc2_ram(unsigned short address){
    if (sram_enable){
        return game_sram[address & 0x01ff];
    } else{
        return 0xff;
    }
};

unsigned char read_mbc3_ram(unsigned short address){
    if (sram_enable){
        if (rtc_in_a000){
            return 0;
        } else{
            return game_sram[sram_bank_offset + (address - 0xa000)];
        }
    } else{
        return 0xff;
    }
};


unsigned char read(unsigned short address){
    if ((address >= 0xa000) && (address < 0xc000)){
        switch (mapper_number){
            case 1: return read_mbc1_ram(address); break;
            case 2: return read_mbc2_ram(address); break;
            case 3: return read_mbc3_ram(address); break;
            case 5: return read_mbc1_ram(address); break;
            default: return m[address];
        }
    } else{
        switch (address){
            case 0xff01: return 0xff; break;
            case 0xff02: return 0x7f; break;
            default: return m[address];
        }
    }
    return 0;
}

unsigned short readword(unsigned short address){
    struct word val;
    val.L = read(address);
    val.H = read(address+1);
    return val.W;
}