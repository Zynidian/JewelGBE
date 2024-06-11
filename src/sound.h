#ifndef SOUND_GEN_H
#define SOUND_GEN_H

const unsigned short CH_PULSE_1_PERIOD_SWEEP = 0xff10;
const unsigned short CH_PULSE_1_DUTY_LENGTH = 0xff11;
const unsigned short CH_PULSE_1_VOLUME_ENV = 0xff12;
const unsigned short CH_PULSE_1_FREQ_LO = 0xff13;
const unsigned short CH_PULSE_1_FREQ_HI_CTRL = 0xff14;

const unsigned short CH_PULSE_2_DUTY_LENGTH = 0xff16;
const unsigned short CH_PULSE_2_VOLUME_ENV = 0xff17;
const unsigned short CH_PULSE_2_FREQ_LO = 0xff18;
const unsigned short CH_PULSE_2_FREQ_HI_CTRL = 0xff19;

const unsigned short CH_WAVE_ENABLE = 0xff1a;
const unsigned short CH_WAVE_LENGTH_TIMER = 0xff1b;
const unsigned short CH_WAVE_VOLUME = 0xff1c;
const unsigned short CH_WAVE_FREQ_LO = 0xff1d;
const unsigned short CH_WAVE_FREQ_HI_CTRL = 0xff1e;

const unsigned short CH_NOISE_LENGTH_TIMER = 0xff20;
const unsigned short CH_NOISE_VOLUME_ENV = 0xff21;
const unsigned short CH_NOISE_TONE = 0xff22;
const unsigned short CH_NOISE_CONTROL = 0xff23;
const unsigned short AUDIO_MASTER_VOLUME = 0xff24;
const unsigned short AUDIO_MASTER_ENABLE = 0xff26;
const unsigned short WAVE_RAM = 0xff30;

unsigned char wave_period_lo = 0;
unsigned char wave_period_hi = 0;
struct word pulse_1_period;
struct word pulse_2_period;
unsigned short pulse_1_volume = 0;
unsigned short pulse_2_volume = 0;
unsigned char pulse_1_sweep_env_pace = 0;
unsigned char pulse_2_sweep_env_pace = 0;
unsigned char noise_sweep_env_pace = 0;
unsigned char pulse_1_sweep_env_val = 0;
unsigned char pulse_2_sweep_env_val = 0;
unsigned char noise_sweep_env_val = 0;
unsigned char pulse_1_envelope_dir = 0;
unsigned char pulse_2_envelope_dir = 0;
unsigned char noise_envelope_dir = 0;
unsigned char pulse_1_length = 0;
unsigned char pulse_2_length = 0;
unsigned char wavetable_length = 0;
unsigned char noise_length = 0;
bool pulse_1_length_enable = false;
bool pulse_2_length_enable = false;
bool wavetable_length_enable = false;
bool noise_length_enable = false;
bool pulse_1_enable = false;
bool pulse_2_enable = false;
bool wavetable_enable = false;
bool noise_enable = false;
unsigned char pulse_1_period_sweep_pace = 0;
signed char pulse_1_period_sweep_dir = 0;
unsigned char pulse_1_period_sweep_step = 0;
unsigned char noise_tone = 0;
bool noise_mode = 0;
unsigned char noise_volume = 0;

void refresh_audio_master_enable();
void set_audio_master_volume(unsigned char data);
void tick_sweep();
void tick_pulse_period_sweep(unsigned char data);
void set_pulse_period_sweep(unsigned char data);
void tick_length_timers();
void init_sound_buffer();
void set_channel_volume(char channel, unsigned char data);
void set_pulse_duty_length(bool channel, unsigned char data);
void set_pulse_pitch(bool channel, struct word period);
void init_noise_frame();
void set_noise_control(unsigned char data);
void set_noise_length(unsigned char data);
void generate_noise_samples();
void set_noise_tone(unsigned char data);
void set_wave_pitch();
void refresh_wave_pattern();
void set_wave_length(unsigned char data);
void set_wave_volume(bool override_to_max = false);
void set_wave_enable(bool override = false);
#endif