#include <SFML/Window/Keyboard.hpp>

#ifndef IO_COMMANDS_H
#define IO_COMMANDS_H

bool allow_user_input = true;
int kbdef_down = sf::Keyboard::S;
int kbdef_up = sf::Keyboard::W;
int kbdef_left = sf::Keyboard::A;
int kbdef_right = sf::Keyboard::D;
int kbdef_start = sf::Keyboard::Enter;
int kbdef_select = sf::Keyboard::BackSlash;
int kbdef_a = sf::Keyboard::Period;
int kbdef_b = sf::Keyboard::Comma;
unsigned char div_counter_prev = 0;
unsigned char divider_counter = 0;

int clock_tick_60hz = 0;
bool allow_resync_clock = false;
int lcd_vblank_sync = 16422;

const unsigned short DIVIDER_REG = 0xff04;
const unsigned short TIMER_COUNTER = 0xff05;
const unsigned short TIMER_MODULO = 0xff06;
const unsigned short TIMER_CONTROL = 0xff07;
const unsigned short OAM_DMA = 0xff46;

void get_button_state();
void clock_timer_registers();

#endif