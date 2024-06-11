#include <SFML/Window/Keyboard.hpp>
#include <stdlib.h>
#include <stdio.h>
#include "memory.h"
#include "io.h"
#include "cpu.h"
#include "main.h"

void get_button_state(){
    if (!allow_user_input){
        m[0xff00] |= 0b00001111;
        return;
    }
    unsigned char joypad_register = m[0xff00];
    unsigned char result_button_states = 0;
    if ((joypad_register & 0b00100000) == 0){
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key(kbdef_start))){
            result_button_states |= 0b00001000;
            //printf("START");
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key(kbdef_select))){
            result_button_states |= 0b00000100;
            //printf("SELECT");
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key(kbdef_b))){
            result_button_states |= 0b00000010;
            //printf("B");
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key(kbdef_a))){
            result_button_states |= 0b00000001;
            //printf("A");
        }
    } else if ((joypad_register & 0b00010000) == 0) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key(kbdef_down))){
            result_button_states |= 0b00001000;
            //printf("DOWN");
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key(kbdef_up))){
            result_button_states |= 0b00000100;
            //printf("UP");
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key(kbdef_left))){
            result_button_states |= 0b00000010;
            //printf("LEFT");
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key(kbdef_right))){
            result_button_states |= 0b00000001;
            //printf("RIGHT");
        }
    }
    joypad_register &= 0b11110000;
    result_button_states ^= 0b00001111;
    joypad_register |= result_button_states;
    m[0xff00] = joypad_register;
}

const unsigned char TIMER_TAPS[4] = {
    0b10000000,
    0b00000010,
    0b00001000,
    0b00100000};

void clock_timer_registers(){
    div_counter_prev = divider_counter;
    divider_counter += 1; //~16384 Hz
    if ((divider_counter & 0x3f) == 0){
        m[DIVIDER_REG] += 1;
    }
    //timer_delay_counter is now replaced by the divider, as that is how the timer is clocked on real hw
    //didn't really change compatibility much at all, but hopefully its slightly more accurate
    unsigned char timer_mask = TIMER_TAPS[(m[TIMER_CONTROL] & 0x03)];
    //falling edge detector (more accurate to real hw)
    if (((div_counter_prev & timer_mask) & (~(divider_counter & timer_mask))) != 0){
        if ((m[TIMER_CONTROL] & 0b00000100) != 0){
            if (m[TIMER_COUNTER] == 0xff){
                m[TIMER_COUNTER] = m[TIMER_MODULO];
                m[INTERRUPT_FLAG] |= 0b00000100; //oml i wrote 0x instead of 0b and that made the timer not work right :skull:
            } else{
                m[TIMER_COUNTER] += 1;
            }
        }
    }
}

bool clock_60hz(){
    clock_tick_60hz++;
    if (clock_tick_60hz >= 17556){
        clock_tick_60hz = 0;
    }
    return (clock_tick_60hz == lcd_vblank_sync);
}