#ifndef CPU_H
#define CPU_H

struct word{
    union{
        struct{
            unsigned char L;
            unsigned char H;
        };
        unsigned short W;
    };
};

const unsigned short INTERRUPT_ENABLE = 0xffff;
const unsigned short INTERRUPT_FLAG = 0xff0f;

int opcode_set[512] = {0};
int f9_column_counter = 0;

unsigned char A;
struct word BC;
struct word DE;
struct word HL;
struct word PC;
struct word SP;

#endif