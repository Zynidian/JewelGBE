#include <SFML/Graphics.hpp>
#include "cpu.h"
#include "io.h"
#include "main.h"
#include "memory.h"

int dot_count = 0;
int pixel_segment[4] = {12,12,12,12};
uint8_t color_palette_red[13] = {255,170,96,0 , 255,170,96,0 , 255,170,96,0 , 255};
uint8_t color_palette_green[13] = {255,170,96,0 , 255,170,96,0 , 255,170,96,0 , 250};
uint8_t color_palette_blue[13] = {255,170,96,0 , 255,170,96,0 , 255,170,96,0 , 240};
uint8_t pixel_row[196] = {0};
uint8_t sprite_priority_row[168] = {0};
int pixel_tile_offset = 0;
int ppu_mode = 2;
int ppu_mode_prev = 2;
char sprite_visited[40] = {0};
unsigned char bg_palette[4] = {0,0,0,0};
unsigned char bit_test_table[8] = {1,2,4,8,16,32,64,128};
unsigned char dot_penalty = 0;

const unsigned short OAM_BASE = 0xfe00;
const unsigned short LCD_CONTROL = 0xff40;
const unsigned short LCD_STATUS = 0xff41;
const unsigned short SCROLL_Y = 0xff42;
const unsigned short SCROLL_X = 0xff43;
const unsigned short LCD_SCANLINE = 0xff44;
const unsigned short LCD_SCANLINE_COMPARE = 0xff45;
const unsigned short PALETTE_BG = 0xff47;
const unsigned short PALETTE_S0 = 0xff48;
const unsigned short PALETTE_S1 = 0xff49;
const unsigned short WINDOW_Y = 0xff4a;
const unsigned short WINDOW_X = 0xff4b;


void clear_pixel_buffer(uint8_t* pixels, bool isWhite=false){
    int onlyAlphaFF = 4;
    if (isWhite){
        onlyAlphaFF = 1;
    }
    for (int i=0; i < 92160; i++){
        pixels[i] = 0 ^ (0xff * (i % onlyAlphaFF == (onlyAlphaFF - 1)));
    }
}

void set_4_pixels(uint8_t* pixels){
    if (dot_count >= 80 && m[LCD_SCANLINE] < 144 && dot_count < 240){
        int pixel_index = (m[LCD_SCANLINE]*640) + (((dot_count - 80) % 160)*4);
        pixels[pixel_index] = color_palette_red[pixel_segment[0]];
        pixels[pixel_index+1] = color_palette_green[pixel_segment[0]];
        pixels[pixel_index+2] = color_palette_blue[pixel_segment[0]];
        pixels[pixel_index+4] = color_palette_red[pixel_segment[1]];
        pixels[pixel_index+5] = color_palette_green[pixel_segment[1]];
        pixels[pixel_index+6] = color_palette_blue[pixel_segment[1]];
        pixels[pixel_index+8] = color_palette_red[pixel_segment[2]];
        pixels[pixel_index+9] = color_palette_green[pixel_segment[2]];
        pixels[pixel_index+10] = color_palette_blue[pixel_segment[2]];
        pixels[pixel_index+12] = color_palette_red[pixel_segment[3]];
        pixels[pixel_index+13] = color_palette_green[pixel_segment[3]];
        pixels[pixel_index+14] = color_palette_blue[pixel_segment[3]];
    }
}

unsigned char render_sprites(unsigned char lcd_control, unsigned char lcd_scanline){
    //clear priority buffer (zero priority wont let sprites draw on those pixels anymore)
    for (int i = 0; i < 160; i++){
        sprite_priority_row[i] = 0x7f;
    }
    unsigned char SpritePalletes[8] = {
        (unsigned char)(m[PALETTE_S0]&0x03), 
        (unsigned char)((m[PALETTE_S0]&0x0c) >> 2), 
        (unsigned char )((m[PALETTE_S0]&0x30) >> 4), 
        (unsigned char)((m[PALETTE_S0]&0xc0) >> 6),
        (unsigned char)(m[PALETTE_S1]&0x03), 
        (unsigned char)((m[PALETTE_S1]&0x0c) >> 2), 
        (unsigned char )((m[PALETTE_S1]&0x30) >> 4), 
        (unsigned char)((m[PALETTE_S1]&0xc0) >> 6)};
    bool is8x16 = (lcd_control & 0b00000100) != 0;
    
    char sprites_rendered = 0;
    for (unsigned char sprite_index_x4 = 0; sprite_index_x4 < 160; sprite_index_x4 += 4){
        //if a sprite has been rendered 16 times, dont try to render it more for this frame
        if (sprite_visited[sprite_index_x4/4] >= 16){
            continue;
        }
        //max 10 sprites per line
        if (sprites_rendered > 10){
            break;
        }
        unsigned char sprite_y = m[OAM_BASE + sprite_index_x4] - 16; //im relying on negative numbers being treated as > 0
        //only render sprite if it is eligable to show up on this scanline
        unsigned char sprite_tile_row = lcd_scanline - sprite_y; //between 0 and 7 or 0 and 15(8x16) is valid
        if (sprite_tile_row < 8 + 8*is8x16){
            unsigned char sprite_attributes = m[OAM_BASE + sprite_index_x4 + 3]; //get sprite attributes
            bool isVerticalFlip = (sprite_attributes & 0b01000000) != 0;
            sprite_tile_row ^= (0x07 + 0x08*is8x16)*isVerticalFlip;
            
            //get sprite graphic for this row
            unsigned char sprite_tile = m[OAM_BASE + sprite_index_x4 + 2] & (0xff - 1*is8x16); //get sprite tile
            unsigned char sprite_tile_lo_bits = m[0x8000 + 16*(sprite_tile + 1*(is8x16 && sprite_tile_row > 7)) + 2*(sprite_tile_row & 0x07)];
            unsigned char sprite_tile_hi_bits = m[0x8000 + 16*(sprite_tile + 1*(is8x16 && sprite_tile_row > 7)) + 2*(sprite_tile_row & 0x07) + 1];
            
            //horizontal flipping
            if ((sprite_attributes & 0b00100000) != 0){
                //lo bits
                sprite_tile_lo_bits = (sprite_tile_lo_bits & 0xF0) >> 4 | (sprite_tile_lo_bits & 0x0F) << 4;
                sprite_tile_lo_bits = (sprite_tile_lo_bits & 0xCC) >> 2 | (sprite_tile_lo_bits & 0x33) << 2;
                sprite_tile_lo_bits = (sprite_tile_lo_bits & 0xAA) >> 1 | (sprite_tile_lo_bits & 0x55) << 1;
                //hi bits
                sprite_tile_hi_bits = (sprite_tile_hi_bits & 0xF0) >> 4 | (sprite_tile_hi_bits & 0x0F) << 4;
                sprite_tile_hi_bits = (sprite_tile_hi_bits & 0xCC) >> 2 | (sprite_tile_hi_bits & 0x33) << 2;
                sprite_tile_hi_bits = (sprite_tile_hi_bits & 0xAA) >> 1 | (sprite_tile_hi_bits & 0x55) << 1;
            }
            //determine other attributes
            bool isPaletteS1 = (sprite_attributes & 0b00010000) != 0;
            bool isPrioritySet = (sprite_attributes & 0b10000000) != 0;
            bool isBkgOverSprite = false;
            //write the 8 pixel colors (with the correct palette indicies and sprite priorities) to pixel_row appropriately
            unsigned char sprite_x = m[OAM_BASE + sprite_index_x4 + 1] - 8; //get sprite x
            for (int tile_pixel = 7; tile_pixel >= 0; tile_pixel--){
                char pixel_palette_index = 2*((sprite_tile_hi_bits & bit_test_table[tile_pixel]) != 0) + ((sprite_tile_lo_bits & bit_test_table[tile_pixel]) != 0);
                char pixel_color = SpritePalletes[pixel_palette_index + 4*isPaletteS1] + 4;
                bool isOpaque = (pixel_palette_index != 0);
                //sprite priority is determined here by OAM index (GBC behavior ikik)
                unsigned char sprite_priority = (isOpaque)*(sprite_index_x4 / 4) + 128*isPrioritySet;
                if (((sprite_priority_row[sprite_x] & 0x7f) > (sprite_priority & 0x7f)) && (isOpaque)){
                    sprite_priority_row[sprite_x] = sprite_priority;
                }
                //if sprite pixel is on screen and BkgOverSprite isnt in effect
                isBkgOverSprite = (sprite_priority_row[sprite_x] >= 0x80) && (pixel_row[sprite_x + 8] != bg_palette[0]);
                if ((sprite_x < 160) && (isOpaque) && !isBkgOverSprite){
                    pixel_row[sprite_x + 8] = pixel_color;
                }
                //go to next pixel
                sprite_x++;
            }
            //sprite contributes toward the 10 sprite cap (even if its not on screen)
            sprites_rendered++;
            sprite_visited[sprite_index_x4/4] += 1;
        }
    }
    return (sprites_rendered*6); //not really accurate, but at least it adds some penalty
}

void render_background(unsigned char lcd_control, unsigned char lcd_scanline){
    //get start of tile row
    unsigned short vram_bg_leftmost_tile
        = 0x9800 + 0x400*((lcd_control & 0b00001000) != 0)
        + ((32*((m[SCROLL_Y] + lcd_scanline) >> 3)) % 0x400);
    //make list of all pixels in the row
    pixel_tile_offset = m[SCROLL_X] % 8;
    for (int i = 0; i < 21; i++){
        int current_tile_id = m[vram_bg_leftmost_tile + ((m[SCROLL_X] >> 3) + i) % 32];
        int current_tile_row_addr = 0x8000
            + (0x1000 * (((lcd_control & 0x10) == 0) && (current_tile_id < 0x80))) 
            + (current_tile_id * 16) 
            + ((m[SCROLL_Y] + lcd_scanline) % 8) * 2;

        int current_tile_lo_bits = m[current_tile_row_addr];
        int current_tile_hi_bits = m[current_tile_row_addr+1];
        for (int tile_pixel = 0; tile_pixel < 8; tile_pixel++){
            pixel_row[(i*8) + 15 - tile_pixel - pixel_tile_offset] = bg_palette[2*(current_tile_hi_bits & 0x01) + (current_tile_lo_bits & 0x01)];
            current_tile_lo_bits >>= 1;
            current_tile_hi_bits >>= 1;
        }
    }
}

//similar to render background, but scrolling works quite a bit different
void render_window(unsigned char lcd_control, unsigned char lcd_scanline){
    unsigned char window_y_relative = lcd_scanline - m[WINDOW_Y];
    //get start of tile row
    //accidentally used the actual (masked) value of lcd control rather than using comparison, causing the second tile map to never be used ;-;
    //didnt find this out until using making the window layer work
    unsigned short vram_win_leftmost_tile
        = 0x9800 + 0x400*((lcd_control & 0b01000000) != 0)
        + ((32*((window_y_relative) / 8)) % 0x400);

    int window_x_offset = m[WINDOW_X] - 7;
    pixel_tile_offset = window_x_offset % 8;
    //make list of all pixels in the row
    for (int i = (window_x_offset / 8); i < 21; i++){
        int current_tile_id = m[vram_win_leftmost_tile + ((i - ((window_x_offset) / 8)) % 32)];
        int current_tile_row_addr = 0x8000
            + (0x1000 * (((lcd_control & 0x10) == 0) && (current_tile_id < 0x80))) 
            + (current_tile_id * 16) 
            + (window_y_relative % 8) * 2;
        int current_tile_lo_bits = m[current_tile_row_addr];
        int current_tile_hi_bits = m[current_tile_row_addr+1];
        //draw 8 pixels of one tile
        for (int tile_pixel = 0; tile_pixel < 8; tile_pixel++){
            pixel_row[(i*8) + 15 - tile_pixel + pixel_tile_offset] = bg_palette[2*(current_tile_hi_bits & 0x01) + (current_tile_lo_bits & 0x01)];
            current_tile_lo_bits >>= 1;
            current_tile_hi_bits >>= 1;
        }
    }
}

void tick_ppu(uint8_t* pixels){
    if ((m[LCD_CONTROL] & 0x80) == 0){
        //if (m[LCD_SCANLINE] < 144){
        //    allow_resync_clock = true;
        //}
        lcd_enabled = false;
        m[LCD_SCANLINE] = 0;
        dot_count = 0;
        ppu_mode = 1;
        lcd_sync_vblank_start = false;
    }
    //timing logic (also drives cpu timing)
    if (dot_count >= 455){
        m[LCD_SCANLINE]++;
        dot_count = 0;
        if (m[LCD_SCANLINE] < 144){
            ppu_mode = 2;
            //m[INTERRUPT_FLAG] = m[INTERRUPT_FLAG] & 0b11111110;
        }
        
    } else {
        dot_count = dot_count + 4;
    }
    
    if (m[LCD_SCANLINE] >= 154){
        m[LCD_SCANLINE] = 0;
        ppu_mode = 2;
        for (int i = 0; i < 40; i++){
            sprite_visited[i] = 0;
        }
    }

    if (m[LCD_SCANLINE] >= 144 && lcd_enabled){
        //vblank
        ppu_mode = 1;
        //oml i had it requesting an interrupt EVERY M CYCLE DURING VBLANK
        //(games would run vblank more than once per frame if not explicitly checked)
        if (((m[LCD_SCANLINE]) == 144) && (dot_count == 24)){ //only request at the start of vblank
            m[INTERRUPT_FLAG] = m[INTERRUPT_FLAG] | 0b00000001; //request vblank irq
        }
        if (m[LCD_SCANLINE] >= 154){
            m[LCD_SCANLINE] = 0;
            ppu_mode = 2;
            for (int i = 0; i < 40; i++){
                sprite_visited[i] = 0;
            }
        }
    } else{
        if (!lcd_enabled){
            if ((m[LCD_CONTROL] & 0x80) != 0){
                lcd_enabled = true;
                lcd_delay_showing = true;
            } else{
                m[LCD_STATUS] = 0x80 + (m[LCD_STATUS] & 0b01111000) + 1;
                return;
            }
        }
        //right before visible portion of the screen
        if (dot_count == 76){
            unsigned char current_scanline = m[LCD_SCANLINE];
            unsigned char current_lcd_control = m[LCD_CONTROL];
            for (int i = 0; i < 176; i++){
                pixel_row[i] = 0;
            }

            //get background palette
            bg_palette[0] = (unsigned char)(m[PALETTE_BG]&0x03);
            bg_palette[1] = (unsigned char)((m[PALETTE_BG]&0x0c) >> 2);
            bg_palette[2] = (unsigned char)((m[PALETTE_BG]&0x30) >> 4);
            bg_palette[3] = (unsigned char)((m[PALETTE_BG]&0xc0) >> 6);

            if (current_lcd_control & 0b00000001 != 0){
                render_background(current_lcd_control, current_scanline);
            }

            //if ((current_lcd_control & 0b00100000) != 0){
                //window_y_counter++;
                //render window on top of background pixel row
                //if (window_y_counter >= m[WINDOW_Y]){
                //    render_window(current_lcd_control, current_scanline);
                //}
            //}

            if ((current_scanline >= m[WINDOW_Y]) && ((current_lcd_control & 0b00100000) != 0)){
                render_window(current_lcd_control, current_scanline);
            }
            //get first 10 sprites, and render them on top of background and window
            if (current_lcd_control & 0b00000010){
                dot_penalty = render_sprites(current_lcd_control, current_scanline);
            }
        }
        else if (dot_count >= 80 && m[LCD_SCANLINE] < 144 && dot_count < 240){
            ppu_mode = 3;
            //drawing 4 pixels
            pixel_segment[0] = pixel_row[8 + (dot_count - 80)];
            pixel_segment[1] = pixel_row[9 + (dot_count - 80)];
            pixel_segment[2] = pixel_row[10 + (dot_count - 80)];
            pixel_segment[3] = pixel_row[11 + (dot_count - 80)];

            set_4_pixels(pixels);
        } else if (dot_count < 455 && m[LCD_SCANLINE] < 144 && dot_count >= (dot_penalty + 252)){
            ppu_mode = 0;
        } else {
            ppu_mode = 3;
        }
    }
    //STAT interrupt setting
    if ((ppu_mode_prev != ppu_mode)){
        if ((ppu_mode == 0 && (m[LCD_STATUS] & 0b00001000) != 0) ||
            (ppu_mode == 1 && (m[LCD_STATUS] & 0b00010000) != 0) ||
            (ppu_mode == 2 && (m[LCD_STATUS] & 0b00100000) != 0))
            {
            m[INTERRUPT_FLAG] |= 0b00000010;
        } 
        ppu_mode_prev = ppu_mode;
    }
    //scanline interrupts always seem to happen ~ dot 28, so i made this its own thing separate from the ppu_mode state
    if ((dot_count == 0)
        && (m[LCD_SCANLINE] == m[LCD_SCANLINE_COMPARE])
        && ((m[LCD_STATUS] & 0b01000000) != 0))
        {
        m[INTERRUPT_FLAG] |= 0b00000010;
    }

    m[LCD_STATUS] = 0x80 + (m[LCD_STATUS] & 0b01111000) + (ppu_mode & 0x03) + 4*(m[LCD_SCANLINE] == m[LCD_SCANLINE_COMPARE]);
    lcd_sync_vblank_start = (((m[LCD_SCANLINE]) == 144) && (dot_count == 24));
    if (lcd_sync_vblank_start){// && allow_resync_clock
        lcd_vblank_sync = clock_tick_60hz - 1;
        //allow_resync_clock = false;
    }
}


