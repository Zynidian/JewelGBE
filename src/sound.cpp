#include <SFML/Audio.hpp>
#include "memory.h"
#include "cpu.h"
#include "sound.h"

double phase_counter = 0;
int wave_index = 0;
signed short PULSE_VOLUME_BASE = 0x10ff;
signed short WAVE_VOLUME_BASE = 0x240;
signed short NOISE_VOLUME_BASE = 0x9ff;

float master_volume_scale = 1.0;

float ch_wave_volume = 0.0;
int sample_rate = 48000;
sf::Time phase;
float temp_vol = 0;
short length_timer_256hz = 0;
unsigned short period_sweep_timer_128hz = 0;
char period_sweep_pace_counter = 0;
unsigned char pulse_1_length_timer = 64;
unsigned char pulse_2_length_timer = 64;
unsigned char wavetable_length_timer = 64;
unsigned char noise_length_timer = 64;
sf::SoundBuffer sf_p1_buffer;
sf::SoundBuffer sf_p2_buffer;
sf::SoundBuffer sf_wv_buffer;
sf::SoundBuffer sf_nz_buffer;
sf::Sound sf_pulse_1;
sf::Sound sf_pulse_2;
sf::Sound sf_wavetable;
sf::Sound sf_noise;
unsigned short noise_LFSR = 0;
unsigned char noise_div_by_175 = 0;
short noise_frame_tick_counter = 0;
unsigned char noise_sample_out_modulo = 1;
unsigned char noise_lsfr_tick_modulo = 1;
unsigned int noise_modulo_clock = 0;
signed short wavetable_sample[256] = {0};
signed short noise_sample[800] = {0};
signed short pulse_75[256];
signed short pulse_50[256];
signed short pulse_25[256];
signed short pulse_12[256];
signed short* pulse_widths[4] = {pulse_12,pulse_25,pulse_50,pulse_75};

void refresh_audio_master_enable(){
    if ((m[AUDIO_MASTER_ENABLE] & 0x80) != 0){
        m[AUDIO_MASTER_ENABLE] = 
            (m[AUDIO_MASTER_ENABLE] & 0x80)
            + wavetable_enable * 4
            + pulse_2_enable * 2
            + pulse_1_enable * 1;
    }
    else {
        pulse_1_enable = false;
        pulse_2_enable = false;
        wavetable_enable = false;
        pulse_1_length = 64;
        pulse_2_length = 64;
        wavetable_length = 64;
        sf_pulse_1.setVolume(0.0);
        sf_pulse_2.setVolume(0.0);
        sf_wavetable.setVolume(0.0);
    }
}

void set_audio_master_volume(unsigned char data){
    char volume_setting = (data & 0b011100000) >> 5;
    volume_setting += (data & 0b000000111);
    master_volume_scale = ((float)volume_setting/16.0);
}

void init_sound_buffer(){
    for (int i = 0; i < 256; i++){        
        if (i >= 64){
            pulse_75[i] = PULSE_VOLUME_BASE;
        } else{
            pulse_75[i] = -PULSE_VOLUME_BASE;
        }
        if (i >= 128){
            pulse_50[i] = PULSE_VOLUME_BASE;
        } else{
            pulse_50[i] = -PULSE_VOLUME_BASE;
        }
        if (i >= 192){
            pulse_25[i] = PULSE_VOLUME_BASE;
        } else{
            pulse_25[i] = -PULSE_VOLUME_BASE;
        }
        if (i >= 224){
            pulse_12[i] = PULSE_VOLUME_BASE;
        } else{
            pulse_12[i] = -PULSE_VOLUME_BASE;
        }
    }

    pulse_1_period.W = 0;
    pulse_2_period.W = 0;
    sf_pulse_1.setBuffer(sf_p1_buffer);
    sf_pulse_2.setBuffer(sf_p2_buffer);
    sf_wavetable.setBuffer(sf_wv_buffer);
    sf_noise.setBuffer(sf_nz_buffer);
    sf_pulse_1.setLoop(true);
    sf_pulse_2.setLoop(true);
    sf_wavetable.setLoop(true);
    sf_noise.setLoop(true);
    sf_pulse_1.setVolume(100.0);
    sf_pulse_2.setVolume(100.0);
    sf_wavetable.setVolume(100.0);
    sf_noise.setVolume(100.0);
    noise_LFSR = 0;
    //sf_pulse_1.play();
    //sf_pulse_2.play();
}

void play_pulse(char channel){
    switch (channel){
        case 1:
            if (pulse_2_enable){
                //apparantly setting the volume to 100*(vol/15) doesn't work, this does basically the same thing tho so no complaints
                sf_pulse_2.setVolume((float)(6.6*(pulse_2_volume)*master_volume_scale));
                sf_pulse_2.pause();
                sf_pulse_2.play();
            }
            break;
        case 0:
            if (pulse_1_enable){
                sf_pulse_1.setVolume((float)(6.6*(pulse_1_volume)*master_volume_scale));
                sf_pulse_1.pause();
                sf_pulse_1.play();
            }
            break;
        case 2:
            if (noise_enable){
                sf_pulse_1.pause();
                sf_noise.setVolume((float)(6.6*(noise_volume)*master_volume_scale));
                sf_pulse_1.play();
            }
    }
}

void tick_pulse_period_sweep(){
    period_sweep_timer_128hz++;
    if (period_sweep_timer_128hz >= 8229){
        period_sweep_timer_128hz = 0;

        if (period_sweep_pace_counter >= pulse_1_period_sweep_pace && (pulse_1_period_sweep_pace != 0)){
            period_sweep_pace_counter = 0;

            unsigned short current_period = pulse_1_period.W & 0x07ff;
            current_period = current_period + (pulse_1_period_sweep_dir*(current_period >> pulse_1_period_sweep_step));
            if (current_period > 0x7ff){
                pulse_1_enable = false;
                pulse_1_length_enable = false;
                pulse_1_volume = 0;
                sf_pulse_1.setVolume(0.0);
            } else if (current_period > 0){
                pulse_1_period.W = (pulse_1_period.W & 0xf000) + current_period;
            }
            float freq = 65536/(2048 - current_period);
            sf_pulse_1.setPitch(freq/(float)(sample_rate/512));

        } else{
            period_sweep_pace_counter++;
        }
    }
}

void set_pulse_period_sweep(unsigned char data){
    pulse_1_period_sweep_dir = -1 * ((data & 0b00001000) != 0);
    pulse_1_period_sweep_pace = (data & 0b01110000) >> 4;
    pulse_1_period_sweep_step = (data & 0b00000111);
    period_sweep_pace_counter = 0;
}

void tick_length_timers(){
    length_timer_256hz++;
    if (length_timer_256hz >= 4115){
        length_timer_256hz = 0;
        //runs at ~255.98 Hz
        if (pulse_1_length_enable){
            pulse_1_length_timer++;
            if (pulse_1_length_timer >= 64){
                pulse_1_enable = false;
                pulse_1_length_enable = false;
                pulse_1_volume = 0;
                sf_pulse_1.setVolume(0.0);
                //sf_pulse_1.pause();
            }
        }
        if (pulse_2_length_enable){
            pulse_2_length_timer++;
            if (pulse_2_length_timer >= 64){
                pulse_2_enable = false;
                pulse_2_length_enable = false;
                pulse_2_volume = 0;
                sf_pulse_2.setVolume(0.0);
                //sf_pulse_2.pause();
            }
        }
        if (wavetable_length_enable){
            wavetable_length_timer++;
            if (wavetable_length_timer >= 64){
                wavetable_enable = false;
                wavetable_length_enable = false;
                sf_wavetable.setVolume(0.0);
                //sf_wavetable.pause();
            }
        }
        if (noise_length_enable){
            noise_length_timer++;
            if (noise_length_timer >= 64){
                noise_enable = false;
                noise_length_enable = false;
                sf_noise.setVolume(0.0);
                noise_LFSR = 0;
                //sf_wavetable.pause();
            }
        }
    }
}

void tick_sweep(){
    if ((pulse_1_sweep_env_pace != 0) && pulse_1_enable){
        pulse_1_sweep_env_val++;
        if (pulse_1_sweep_env_val >= pulse_1_sweep_env_pace){
            if ((pulse_1_volume < 15) && (pulse_1_envelope_dir != 0)){
                pulse_1_volume++;
            }
            if ((pulse_1_volume > 0) && (pulse_1_envelope_dir == 0)){
                pulse_1_volume--; 
            }
            pulse_1_sweep_env_val = 0;
            play_pulse(0);
        }
    }
    if ((pulse_2_sweep_env_pace != 0) && pulse_2_enable){
        pulse_2_sweep_env_val++;
        if (pulse_2_sweep_env_val >= pulse_2_sweep_env_pace){
            if ((pulse_2_volume < 15) && (pulse_2_envelope_dir != 0)){
                pulse_2_volume++;
            }
            if ((pulse_2_volume > 0) && (pulse_2_envelope_dir == 0)){
                pulse_2_volume--;
            }
            pulse_2_sweep_env_val = 0;
            play_pulse(1);
        }
    }
    if ((noise_sweep_env_pace != 0) && noise_enable){
        noise_sweep_env_val++;
        if (noise_sweep_env_val >= noise_sweep_env_pace){
            if ((noise_volume < 15) && (noise_envelope_dir != 0)){
                noise_volume++;
            }
            if ((noise_volume > 0) && (noise_envelope_dir == 0)){
                noise_volume--;
            }
            noise_sweep_env_val = 0;
            play_pulse(2);
        }
    }
}

void set_channel_volume(char channel, unsigned char data){
    switch (channel){
        case 1:
            pulse_1_sweep_env_pace = data & 0x7;
            pulse_1_envelope_dir = data & 0x8;
            pulse_1_volume = (data >> 4) & 0xf;
            //pulse_1_enable = false;
            break;
        case 2:
            pulse_2_sweep_env_pace = data & 0x7;
            pulse_2_envelope_dir = data & 0x8;
            pulse_2_volume = (data >> 4) & 0xf;
            //pulse_2_enable = false;
            break;
        case 3:
            noise_sweep_env_pace = data & 0x7;
            noise_envelope_dir = data & 0x8;
            noise_volume = (data >> 4) & 0xf;
            break;
        default: break;
    }
}

void set_pulse_duty_length(bool channel, unsigned char data){
    if (channel){
        pulse_2_length = (data & 0x3f);
        pulse_2_length_timer = pulse_2_length;
        sf_p2_buffer.loadFromSamples(pulse_widths[((data >> 6) & 0x03)], 256, 1, sample_rate);
    } else{
        pulse_1_length = (data & 0x3f);
        pulse_1_length_timer = pulse_1_length;
        sf_p1_buffer.loadFromSamples(pulse_widths[((data >> 6) & 0x03)], 256, 1, sample_rate);
    }
    play_pulse(channel);
}

void set_pulse_pitch(bool channel, struct word period_word){
    unsigned short period = 0x100*(period_word.H & 0x07) + period_word.L;
    float freq = 65536/(2048 - period);
    pulse_2_length_enable = (period_word.H & 0x40) != 0;
    pulse_1_length_enable = (period_word.H & 0x40) != 0;
    if (channel){
        if (!pulse_2_enable){
            pulse_2_enable = (period_word.H & 0x80) != 0;
            //if (pulse_2_enable){
                pulse_2_volume = (m[CH_PULSE_2_VOLUME_ENV] >> 4); //idk when volume should be updated
            //}   //puyo puyo doesn't update the volume ;-; but writing this so it does work makes other games not have env work
        }
        sf_pulse_2.setPitch(freq/(float)(sample_rate/512));
    } else {
        if (!pulse_1_enable){
            pulse_1_enable = (period_word.H & 0x80) != 0;
            //if (pulse_1_enable){
                pulse_1_volume = (m[CH_PULSE_1_VOLUME_ENV] >> 4);
            //}
        }
        sf_pulse_1.setPitch(freq/(float)(sample_rate/512));
        period_sweep_pace_counter = 0;
    }
    play_pulse(channel);
}

void init_noise_frame(){
    sf_noise.setPitch(1.0);
    sf_noise.play();
    noise_frame_tick_counter = 0;
    noise_div_by_175 = 0;
    noise_modulo_clock = 0;
}

void set_noise_control(unsigned char data){
    noise_enable = (data & 0x80) != 0;
    if (!noise_enable){
        noise_LFSR = 0;
        sf_noise.setVolume(0.0);
    } else{
        sf_noise.setVolume((float)(6.6*(noise_volume)*master_volume_scale));
    }
    noise_length_enable = (data & 0x40) != 0;
}

void set_noise_length(unsigned char data){
    m[CH_NOISE_LENGTH_TIMER] = data;
    noise_length = (data & 0x3f);
    noise_length_timer = noise_length;
}

bool tick_noise_LFSR(){
    bool new_noise_bit = 0;
    new_noise_bit = (noise_LFSR & 1) == ((noise_LFSR & 2) != 0);
    if (noise_mode){
        noise_LFSR = (noise_LFSR & 0b1111111101111111) | (new_noise_bit*0b0000000010000000);
    }
    noise_LFSR |= (new_noise_bit*0b1000000000000000);
    noise_LFSR >>= 1;
    return (noise_LFSR & 1);
}

void generate_noise_samples(){
    noise_div_by_175++;
    if (noise_div_by_175 >= 175){
        noise_div_by_175 = 0;
        if (noise_frame_tick_counter < 100){
            unsigned char index_offset = 0;
            signed short sample_output = 0;
            while (index_offset < 8){
                noise_modulo_clock++;
                if ((noise_modulo_clock % noise_lsfr_tick_modulo) == 0){
                    sample_output = (tick_noise_LFSR()*2)-1;
                }
                if ((noise_modulo_clock % noise_sample_out_modulo) == 0){
                    noise_sample[(noise_frame_tick_counter*8) + index_offset] = ((sample_output)*NOISE_VOLUME_BASE);
                    index_offset++;
                }
            }
            if (noise_enable && (noise_frame_tick_counter % 20 == 19)){
                sf_noise.pause();
                sf_nz_buffer.loadFromSamples(noise_sample, 800, 1, sample_rate);
                sf_noise.setBuffer(sf_nz_buffer);
                sf_noise.play();
            }
        }
        noise_frame_tick_counter++;
    }
}

const unsigned char NOISE_MOD_VALUES[256] = {
    0x0d,0x8e,0x0d,0x47,0x1a,0x47,0x27,0x47,0x34,0x47,0xce,0xe1,0xf5,0xdf,0xe8,0xb5,
    0x0d,0x47,0x1a,0x47,0x34,0x47,0xf5,0xdf,0x68,0x47,0x82,0x47,0x9c,0x47,0xb6,0x47,
    0x1a,0x47,0x34,0x47,0x68,0x47,0x9c,0x47,0xd0,0x47,0xf9,0x44,0xa7,0x26,0xf1,0x2f,
    0x34,0x47,0x68,0x47,0xd0,0x47,0xa7,0x26,0xfc,0x2b,0xf9,0x22,0xa7,0x13,0x29,0x04,
    0x68,0x47,0xd0,0x47,0xfc,0x2b,0xa7,0x13,0xd3,0x12,0xf9,0x11,0xd3,0x0c,0x29,0x02,
    0xd0,0x47,0xfc,0x2b,0xd3,0x12,0xd3,0x0c,0xd3,0x09,0xcd,0x07,0xd3,0x06,0x29,0x01,
    0xfc,0x2b,0xd3,0x12,0xd3,0x09,0xd3,0x06,0xea,0x05,0xb0,0x03,0xd3,0x03,0x52,0x01,
    0xd3,0x12,0xd3,0x09,0xea,0x05,0xd3,0x03,0x5e,0x01,0x75,0x01,0x8d,0x01,0xa4,0x01,
    0xd3,0x09,0xea,0x05,0x5e,0x01,0x8d,0x01,0xbc,0x01,0xea,0x01,0xff,0x01,0xff,0x01,
    0xea,0x05,0x5e,0x01,0xbc,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,
    0x5e,0x01,0xbc,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,
    0xbc,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,
    0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,
    0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,
    0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,
    0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,
};

void set_noise_tone(unsigned char data){
    noise_tone = ((data >> 1) & 0b01111000) | (data & 0x07);
    noise_mode = (data & 0x08) != 0;
    //this function should use clock shift and divider to look up a table of calculated modulo values for different tones
    noise_lsfr_tick_modulo = NOISE_MOD_VALUES[noise_tone*2];
    noise_sample_out_modulo = NOISE_MOD_VALUES[(noise_tone*2)+1];
}

//im gonna use sfml's sound generation rather than generating my own pcm samples
//bc of this, ill have to turn gameboy registers into corresponding sfml values (ex pitch)
void set_wave_pitch(){
    unsigned short period = 0x100*(wave_period_hi & 0x07) + wave_period_lo;
    float freq = 65536/(2048 - period);
    if (!wavetable_enable){
        wavetable_enable = (wave_period_hi & 0x80) != 0;
    }
    wavetable_length_enable = (wave_period_hi & 0x40) != 0;
    sf_wavetable.setPitch(freq/(float)(sample_rate/256));
    if ((m[CH_WAVE_ENABLE] & 0b10000000) != 0 && wavetable_enable){
        sf_wavetable.pause();
        //phase = sf_wavetable.getPlayingOffset();
        sf_wavetable.play();
        //sf_wavetable.setPlayingOffset(phase);
    }
}

void refresh_wave_pattern(){
    for (int wave_index = 0; wave_index < 16; wave_index++){
        signed short wave_sample_1 = (m[0xff30 + wave_index] >> 4) * WAVE_VOLUME_BASE;
        signed short wave_sample_2 = (m[0xff30 + wave_index] & 0x0f) * WAVE_VOLUME_BASE;
        wavetable_sample[(16*wave_index)] = wave_sample_1;
        wavetable_sample[(16*wave_index)+1] = wave_sample_1;
        wavetable_sample[(16*wave_index)+2] = wave_sample_1;
        wavetable_sample[(16*wave_index)+3] = wave_sample_1;
        wavetable_sample[(16*wave_index)+4] = wave_sample_1;
        wavetable_sample[(16*wave_index)+5] = wave_sample_1;
        wavetable_sample[(16*wave_index)+6] = wave_sample_1;
        wavetable_sample[(16*wave_index)+7] = wave_sample_1;
        wavetable_sample[(16*wave_index)+8] = wave_sample_2;
        wavetable_sample[(16*wave_index)+9] = wave_sample_2;
        wavetable_sample[(16*wave_index)+10] = wave_sample_2;
        wavetable_sample[(16*wave_index)+11] = wave_sample_2;
        wavetable_sample[(16*wave_index)+12] = wave_sample_2;
        wavetable_sample[(16*wave_index)+13] = wave_sample_2;
        wavetable_sample[(16*wave_index)+14] = wave_sample_2;
        wavetable_sample[(16*wave_index)+15] = wave_sample_2;
    }
    sf_wv_buffer.loadFromSamples(wavetable_sample, 256, 1, sample_rate);
}

void set_wave_length(unsigned char data){
    m[CH_WAVE_LENGTH_TIMER] = data;
    wavetable_length = (data & 0x3f);
    wavetable_length_timer = wavetable_length;
}

void set_wave_volume(bool override_to_max){
    switch (m[CH_WAVE_VOLUME] & 0b01100000){
        case 32: ch_wave_volume = 100; break;
        case 64: ch_wave_volume = 50; break;
        case 96: ch_wave_volume = 25; break;
        default: ch_wave_volume = 0;
    }
    ch_wave_volume = ch_wave_volume;
    sf_wavetable.setVolume(ch_wave_volume*master_volume_scale);
}

void set_wave_enable(bool override){
    if ((m[CH_WAVE_ENABLE] & 0b10000000) != 0){
        sf_wavetable.pause();
        refresh_wave_pattern();
        //phase = sf_wavetable.getPlayingOffset();
        sf_wavetable.play();
        //sf_wavetable.setPlayingOffset(phase);
    } else{
        sf_wavetable.stop();
        wavetable_enable = false;
        wavetable_length_enable = false;
    }
}

