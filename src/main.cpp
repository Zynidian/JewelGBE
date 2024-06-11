#include <SFML/Graphics.hpp>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>

#include "memory.cpp"
#include "cpu.cpp"
#include "display.cpp"
#include "io.cpp"
#include "sound.cpp"
#include "main.h"

int main(int argc, char** argv)
{
    std::cout << "\n =-=-=- Jewel Gameboy Emulator by Zynidian -=-=-=\n";
    std::cout << " D-PAD: W A S D \n B:[.>]  A:[,<]  Select:[\\|]  Start:[Enter]\n";
    std::cout << " [F1] - Change window resolution (1x-7x)\n";
    std::cout << " [F6] - Stop execution and show Tracelog\n";
    std::cout << " [F8] - Display current CPU state\n";
    std::cout << " [F9] - Log all subroutine calls while held\n\n";

    auto window = sf::RenderWindow{ { 480u, 432u }, "Jewel Gameboy Emulator" };
    window.setFramerateLimit(60);
    sf::Texture lcd;
    sf::Sprite lcd_sprite;
    lcd.create(160, 144);
    lcd_sprite.setTexture(lcd);
    lcd_sprite.setScale(3.0,3.0);
    sf::Vector2u screen_dim(480u, 432u);
    int screen_scale_factor = 3;
    std::fill(std::begin(m),std::end(m), 0);

    unsigned char* boot_rom;
    boot_rom = load_rom((char*)"dmg_boot.bin", default_boot_rom);
    std::copy(&boot_rom[0], &boot_rom[256], &m[0]);
    
    char* rom_file;
    bool game_rom_exist = false;

    std::string rom_name;
    std::string sramname;
    init_ff_256();
    try{
        rom_name = argv[1];
        sramname.append(rom_name);
        if (rom_name.find(".gb") != rom_name.npos){
            rom_file = argv[1];
            game_rom = load_rom((char*)rom_file, ff_256);
            std::copy(&game_rom[256], &game_rom[32768], &m[256]);
            game_rom_exist = true;
        }
    } catch(...){
        game_rom = ff_256;
    }

    if (false){
        //rom_file = (char*)"C:/gb/test/test.gb";
        rom_file = (char*)"Kirby's Dream Land 2 (USA, Europe) (SGB Enhanced).gb";
        game_rom = load_rom((char*)rom_file, ff_256);
        std::copy(&game_rom[256], &game_rom[32768], &m[256]);
        game_rom_exist = true;
    }

    if (game_rom_exist){
        decode_header();
        if (has_ram && has_battery){
            replace(sramname, ".gbc", ".gb");
            replace(sramname, ".gb", ".sav");
            load_save_file(sramname.data());
        }
    }
    // update a texture from an array of pixels
    uint8_t* pixels = new uint8_t[160 * 144 * 4]; //this shows up when pixels is hovered over
    clear_pixel_buffer(pixels, false);

    window.clear();
    window.display();
    init_sound_buffer();

    unsigned char prev_boot_rom_flag_state = m[0xff50];
    int breakpt = 0;
    while (window.isOpen())
    {
        for (auto event = sf::Event{}; window.pollEvent(event);)
        {
            if (event.type == sf::Event::Closed){
                if (!illegalOperation){
                    save_file(sramname);
                }
                window.close();
            }
            if (event.type == sf::Event::GainedFocus){
                allow_user_input = true;
            }
            if (event.type == sf::Event::LostFocus){
                allow_user_input = false;
            }
        }
        bool is_done_rendering = false;
        unsigned int audio_tick_cap_timer = 0;
        init_noise_frame();
        //loops for each M cycle for a whole frame (~17556 times)
        //keep in mind that the actual number of ticks it runs each frame depends on if/how long the lcd is disabled
        while (is_done_rendering == false){
            if (m[0xff50] != prev_boot_rom_flag_state){
                if (m[0xff50] == 0 || !game_rom_exist){
                    std::copy(&boot_rom[0], &boot_rom[256], &m[0]);
                } else {
                    std::copy(&game_rom[0], &game_rom[256], &m[0]);
                }
                prev_boot_rom_flag_state = m[0xff50];
            }
            if (audio_tick_cap_timer < 17556){
                generate_noise_samples();
                tick_pulse_period_sweep();
                clock_timer_registers();
                tick_length_timers();
            }
            run_cpu_cycle();
            //dr mario now freezes when starting a game (after changing how the display timing / disabling works)
            //make noise periods work properly
            //fix other issues with sound (some games worse than others)
            //window auto closes if it cant load game/bios?? (showed up after save was implemented)
            //uppercase .GB doesn't work lmao

            if (PC.W == 0x40e0){
                breakpt = 1;
            }
            if (breakpt == 1 && m_cycle_delay == 0){
                breakpt = 1;
            }
            tick_ppu(pixels);
            audio_tick_cap_timer++;
            is_done_rendering = clock_60hz();
        }
        
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F1)){
            if (!is_f1_pressed && allow_user_input){
                is_f1_pressed = true;
                //lcd_sprite.setScale(screen_scale_factor, screen_scale_factor);
                if (screen_scale_factor >= 7){
                    screen_scale_factor = 1;
                } else{
                    screen_scale_factor++;
                }
                screen_dim.x = 160*screen_scale_factor;
                screen_dim.y = 144*screen_scale_factor;
                window.setSize(screen_dim);
            }
        } else{
            is_f1_pressed = false;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F8)){
            if (!is_f8_pressed && allow_user_input){
                is_f8_pressed = true;
                print_stack_and_register_log();
            }
        } else{
            is_f8_pressed = false;
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F6)){
            if (!is_f6_pressed && allow_user_input){
                is_f6_pressed = true;
                illegalOperation = true;
                if (!illegalOp_done_print){

                    illegalOp_done_print = true;
                    if (LOG_OPERATIONS){
                        print_cpu_operations_log();
                    }
                    std::cout << "\n\n ===== [F6] Forced Stop =====";
                    print_stack_and_register_log();
                }
            }
        }

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F9)){
            if (!is_f9_pressed && allow_user_input){
                is_f9_pressed = 1;
                std::cout << "\n";
            }
            if (is_done_rendering && (is_f9_pressed == 1)){
                printf("\n\n");
                is_f9_pressed = 2;
            }

        } else{
            is_f9_pressed = 0;
        } 
        
        tick_sweep();
        //window.clear();
        if (lcd_enabled && (!lcd_delay_showing)){
            lcd.update(pixels);
        }
        window.draw(lcd_sprite);
        window.display();
        if (lcd_delay_showing){
            lcd_delay_showing = false;
        }
    }
}