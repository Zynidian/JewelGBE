#include "cpu.h"
#include "memory.h"
#include "main.h"
#include "sound.h"
#include <iostream>

bool Carry;
bool Negative;
bool Zero;
bool HalfCarry;
bool isHalted = false;
bool isStopped = false;
bool InterruptMasterEnable = false;
int opcode;
int m_cycle_delay = 0;
bool illegalOperation = false;
int interrupt_delay = 0;
bool interrupt_pending;
bool illegalOp_done_print = false;
bool LOG_CALLS = true;
bool LOG_OPERATIONS = true;

unsigned short TRACEBACK_AMOUNT = 4096;

unsigned char log_operations_circular_buffer[0x10000] = {0};
unsigned short call_log_circular_buffer[65] = {0};
unsigned short operations_log_head = 0;
int call_log_head = 0;

//bugs i had with this:
//jump/call/ret instructions with conditions did not increase program counter appropriately if the condition was false
//multiple routines used (var & 0x[some bit to test] == 1) when deciding flags, which doesnt work and should instead use != 0
//  similar issue with == 1*(some condition), in which the 1 was replaced with the appropriately scaled value
//rst instructions didn't take into account the program counter auto inc
//opcodes $01,$e9 did not have a 'break;' after their case statement
//'inline' functions that saved back the results to a register were not doing so (changed to be regular functions)
//compare instructions werent setting carry flags correctly
//jp a16 instructions were using get_imm_addr() prior to condition checking, incrementing the program counter too much on false conditions
//while loop for determining which pending interrupt to handle used >> and << on variables without assignment (fixed by using >>= and <<=)
//a problem i had more with the display code, var & mask != 0 or similar statements were doing order of operations wrong (needed parenthesis)
//add/subtract operations had issues with half-carry flag implementation
//inc/dec with 8 bit registers had even worse half carry implementations
//rotates with A (single byte opcode versions) were not always setting the Z flag to false
//cpu would only wake up from a halt instruction if an interrupt was pending AND IME == true, when in reality, only the interrupt pending condition is needed
//unrelated to cpu, but sram banking was editing the copied version, not the actual bank (caused pokemon to have black boxes for sprites and other issues)
//opcode $e8 (add sp, e8) had the operand cast as a signed char for flag calculations when it shouldnt have been
//add hl instructions's half carry was not taking the low byte into account
//DAA's full carry implementation was wrong
//disabling the LCD did not really work, and the cpu's timing was tied to the lcd's dot/scanline counters
//  this caused games like mario land 2 to not work, and was fixed by doing the following:
//      disabling the LCD now resets LY to 0 and keeps the dot count prepetually around dot 400
//      when disabled, the LCD wont do any rendering, and will instead wait for it to be reenabled
//      when the LCD is reenabled, it starts from the beginning of a frame, but wont display anything new until the next frame
//      because LY (scanline) and the dot count dont work when it's disabled, a spearate 60hz counter is used for cpu timing (flipping display just before vblank)

char op_default_delays[512] = { //the default path will always be the lowest cycle count an instruction may take
    1,3,2,2,1,1,2,1,5,2,2,2,1,1,2,1,
    1,3,2,2,1,1,2,1,3,2,2,2,1,1,2,1,
    2,3,2,2,1,1,2,1,2,2,2,2,1,1,2,1,
    2,3,2,2,3,3,3,1,2,2,2,2,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    2,2,2,2,2,2,1,2,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
    2,3,3,4,3,4,2,4,2,4,3,1,3,6,2,4,
    2,3,3,1,3,4,2,4,2,4,3,1,3,1,2,4,
    3,3,2,1,1,4,2,4,4,1,4,1,1,1,2,4,
    3,3,2,1,1,4,2,4,3,2,4,1,1,1,2,4,

    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
    2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
    2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
    2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2};

void print_working_ops(){
    std::cout << std::hex << (unsigned short)opcode;
    std::cout << " ";
}

void cpu_log_working_ops(int opcode){
    if (opcode_set[opcode] == 0){
        opcode_set[opcode] = 1;
        //print_working_ops();
    }
}

void cpu_log_calls(unsigned short calling_addr, unsigned short target_addr){
    if (is_f9_pressed){
        f9_column_counter++;
        if (f9_column_counter >= 6){
            printf("\n");
            f9_column_counter = 0;
        }
        printf(" %04x->%04x ", (int)calling_addr, (int)target_addr);
    }
    if (LOG_CALLS){
        call_log_circular_buffer[call_log_head] = calling_addr;
        call_log_circular_buffer[call_log_head+1] = target_addr;
        call_log_head = (call_log_head + 2) % 64;
    }
}

void print_stack_and_register_log(){
    std::cout << "\n Opcode '"<<std::hex<<opcode<<"'";
    std::cout << "\n PC:"<<std::hex<<PC.W<<"  SP:"<<SP.W<<"     Z:"<<1*Zero<<" N:"<<1*Negative<<" H:"<<1*HalfCarry<<" C:"<<1*Carry<<"";
    std::cout << "\n A: "<<std::hex<<(int)A<<"  BC: "<<BC.W<<"  DE: "<<DE.W<<"  HL: "<<HL.W;
    std::cout << "\n\n Stack:";
    for (unsigned short i = ((SP.W-64) & 0xfff0) ; i != ((SP.W+64) & 0xfff0); i++){
        if (i%16 == 0){
            std::cout << "\n";
        }
        if (i == SP.W) {
            std::cout << ">";
        } else{
            std::cout << " ";
        }
        printf("%02x", (int)m[i]);
    }

    std::cout << "\n\n Call Logs:\n ";
    for (int log_index = (call_log_head & 0xf8) + 8; log_index < call_log_head + 64; log_index += 2){
        printf("%04x->", (int)call_log_circular_buffer[log_index % 64]);
        printf("%04x", (int)call_log_circular_buffer[(log_index + 1) % 64]);
        if ((log_index % 8) == 6){
            printf("\n ");
        } else{
            printf(", ");
        }
    }
}

void print_cpu_operations_log(){
    printf("\n\n =-=-=-=-=-= Tracelog =-=-=-=-=-=\n\n  PC    +0 +1 +2   \t A  B C  D E  H L  SP   Z N H C  IE\n");
    for (unsigned short index = (unsigned short)((operations_log_head - TRACEBACK_AMOUNT) & 0xfff0); index != (unsigned short)((operations_log_head & 0xfff0) + 32); index += 16){
        printf(" %02x%02x:  ", (int)log_operations_circular_buffer[index],
            (int)log_operations_circular_buffer[index+1]);

        printf("%02x %02x %02x  \t", (int)log_operations_circular_buffer[index+2],
            (int)log_operations_circular_buffer[index+3],
            (int)log_operations_circular_buffer[index+4]);

        printf("%02x %02x%02x %02x%02x %02x%02x %02x%02x  ", (int)log_operations_circular_buffer[index+6],
            (int)log_operations_circular_buffer[index+7],
            (int)log_operations_circular_buffer[index+8],
            (int)log_operations_circular_buffer[index+9],
            (int)log_operations_circular_buffer[index+10],
            (int)log_operations_circular_buffer[index+11],
            (int)log_operations_circular_buffer[index+12],
            (int)log_operations_circular_buffer[index+13],
            (int)log_operations_circular_buffer[index+14]);

        unsigned char flags = log_operations_circular_buffer[index+15];
        if ((flags & 0x80) != 0){
            std::cout << "Z ";
        } else {
            std::cout << "- ";
        }
        if ((flags & 0x40) != 0){
            std::cout << "N ";
        } else {
            std::cout << "- ";
        }
        if ((flags & 0x20) != 0){
            std::cout << "H ";
        } else {
            std::cout << "- ";
        }
        if ((flags & 0x10) != 0){
            std::cout << "C  ";
        } else {
            std::cout << "-  ";
        }
        unsigned char flag_byte = log_operations_circular_buffer[index+5];
        printf("%01x%02x\n", (int)((flag_byte & 0x20) != 0), (int)(flag_byte & 0x1f));
    }
}

unsigned char cpu_get_flag_byte(){
    return (0x80*Zero + 0x40*Negative + 0x20*HalfCarry + 0x10*Carry);
}


void cpu_set_flags(char flag_byte){
    Zero = ((flag_byte & 0x80) != 0);
    Negative = ((flag_byte & 0x40) != 0);
    HalfCarry = ((flag_byte & 0x20) != 0);
    Carry = ((flag_byte & 0x10) != 0);
}

unsigned short cpu_get_imm_word(){
    PC.W++;
    struct word val;
    val.L = read(PC.W);
    PC.W++;
    val.H = read(PC.W);
    return val.W;
}


unsigned char cpu_stack_pop(){
    unsigned char val = read(SP.W); 
    SP.W++;
    return val;
}

unsigned short cpu_stack_pop_word(){
    struct word val;
    val.L = read(SP.W); 
    SP.W++; 
    val.H = read(SP.W); 
    SP.W++;
    return val.W;
}

void cpu_stack_push(char val){
    SP.W--;
    write(SP.W, val);
}

void cpu_stack_push_word(struct word val){
    SP.W--;
    write(SP.W, val.H);
    SP.W--;
    write(SP.W, val.L);
}

void cpu_stack_push_word(unsigned short val){
    SP.W--;
    write(SP.W, ((val & 0xff00) >> 8));
    SP.W--;
    write(SP.W, val & 0xff);
}

unsigned char cpu_inc_dec(unsigned char var, bool decrement){
    unsigned char var_prime = var + 1 + (-2*decrement);
    HalfCarry = ((var & 0x0f) == 0x0f*!decrement);
    Zero = ((var_prime & 0xff) == 0);
    Negative = decrement;
    return var_prime;
}

unsigned short cpu_inc_dec(unsigned short var, bool decrement){
    unsigned short var_prime = var + 1 + (-2*decrement);
    HalfCarry = ((var & 0x0f) == 0x0f*!decrement);
    Zero = ((var_prime & 0xffff) == 0);
    Negative = decrement;
    return var_prime;
}


void cpu_jump(unsigned short address, int alt_m_cycle, bool condition = true){
    if (condition){
        PC.W = address;
        m_cycle_delay = alt_m_cycle;
        PC.W--;
    } else{
        PC.W += 2;
    }
}

void cpu_jump_a16(int alt_m_cycle, bool condition = true){
    if (condition){
        PC.W = cpu_get_imm_word();
        m_cycle_delay = alt_m_cycle;
        PC.W--;
    } else{
        PC.W += 2;
    }
}

void cpu_jump_rel(int alt_m_cycle, bool condition = true){
    if (condition){
        PC.W = PC.W + 1 + (signed char)read(PC.W+1);
        m_cycle_delay = alt_m_cycle;
    } else{
        PC.W += 1;
    }
}

void cpu_return(int alt_m_cycle, bool condition = true){
    if (condition){
        PC.W = cpu_stack_pop_word();
        m_cycle_delay = alt_m_cycle;
        PC.W--;
    }
}

void cpu_call(int alt_m_cycle, bool condition = true){
    if (condition){
        unsigned short prev_addr = PC.W;
        cpu_stack_push_word(PC.W+3);
        unsigned short address = cpu_get_imm_word();
        cpu_log_calls(prev_addr, address);
        PC.W = address;
        m_cycle_delay = alt_m_cycle;
        PC.W--;
    } else{
        PC.W += 2;
    }
}

void cpu_daa_adjust(){
    int adjust = 0;
    unsigned int val = A;

    if (((Negative == false) && (A & 0x0f) > 0x09) || (HalfCarry == true)){
        adjust |= 0x06;
    }
    if (((Negative == false) && (A > 0x99)) || (Carry == true)){
        adjust |= 0x60;
        Carry = true;
    }
    val += (adjust ^ (0xff*Negative)) + 1*Negative;
    A = val & 0xff;
    HalfCarry = false;
    Zero = (A == 0);
}

void cpu_add_sub(unsigned char var, bool plus1, bool invert){
    HalfCarry = ((0x10 & ((A & 0xf) + ((var & 0xf) ^ (0xff*invert)) + 1*plus1)) == 0x10);
    unsigned int val = A + (var ^ (0xff*invert)) + 1*plus1;
    Carry = ((val & 0x100) == 0x100*!invert);
    A = val & 0xff;
    Zero = (A == 0);
    Negative = invert;
}

void cpu_add_hl_16(struct word var){
    HalfCarry = (
        (0x10 & ((HL.H & 0xf) + (var.H & 0xf)
        + (((HL.W & 0xff) + (var.W & 0xff)) > 0xff))) != 0);
    unsigned int val = HL.W + var.W;
    Carry = ((val & 0x10000) != 0);
    HL.W = val & 0xffff;
    Negative = false;
}

unsigned short cpu_add_sp_relative(){
    PC.W++;
    unsigned char offset = read(PC.W);
    unsigned short val = SP.L + offset;
    HalfCarry = ((0x10 & ((SP.L & 0xf) + (offset & 0xf))) == 0x10);
    Carry = ((val & 0x100) != 0);
    Negative = false;
    Zero = false;
    return SP.W + (signed char)offset;
}

void cpu_logic_and(unsigned char var){
    HalfCarry = true;
    Negative = false;
    Carry = false;
    A = A & var;
    Zero = (A == 0);
}

void cpu_logic_or(unsigned char var){
    HalfCarry = false;
    Negative = false;
    Carry = false;
    A = A | var;
    Zero = (A == 0);
}

void cpu_logic_xor(unsigned char var){
    HalfCarry = false;
    Negative = false;
    Carry = false;
    A = A ^ var;
    Zero = (A == 0);
}

void cpu_compare(unsigned char var){
    HalfCarry = ((0x10 & ((A & 0xf) + ~(var & 0xf) + 1)) != 0);
    unsigned int val = A + ~var + 1;
    Carry = ((val & 0x100) != 0);
    Zero = ((val & 0xff) == 0);
    Negative = true;
}

unsigned char cpu_rotate_right(unsigned char var){
    Negative = false;
    HalfCarry = false;
    Carry = ((var & 0x01) != 0);
    var = (var >> 1) + (0x80 * Carry);
    Zero = (var == 0);
    return (var);
}

unsigned char cpu_rotate_left(unsigned char var){
    Negative = false;
    HalfCarry = false;
    Carry = ((var & 0x80) != 0);
    var = (var << 1) + (1 * Carry);
    Zero = (var == 0);
    return (var);
}

unsigned char cpu_shift_right(unsigned char var, bool bit_7){
    Negative = false;
    HalfCarry = false;
    unsigned int shifted_var = var + (0x100 * bit_7);
    Carry = ((shifted_var & 0x01) != 0);
    var = (shifted_var >> 1);
    Zero = (var == 0);
    return (var);
}

unsigned char cpu_shift_left(unsigned char var, bool bit_0){
    Negative = false;
    HalfCarry = false;
    unsigned int shifted_var = (var << 1) + 1*bit_0;
    Carry = ((shifted_var & 0x100) != 0);
    var = (shifted_var & 0xff);
    Zero = (var == 0);
    return (var);
}

unsigned char cpu_swap_nybbles(unsigned char var){
    Negative = false;
    HalfCarry = false;
    Carry = false;
    Zero = (var == 0); //swapping nybbles doesn't change if it's zero or not
    return (((var & 0xf0) >> 4) | ((var & 0x0f) << 4));
}

inline void cpu_test_bit(unsigned char var, unsigned char bitmask){
    Negative = false;
    HalfCarry = true;
    Zero = ((var & bitmask) == 0);
}
inline void cpu_test_bit(unsigned short addr, unsigned char bitmask){
    Negative = false;
    HalfCarry = true;
    Zero = ((read(addr) & bitmask) == 0);
}
unsigned char cpu_reset_bit(unsigned char var, unsigned char bitmask){
    return var & (~bitmask);
}
inline void cpu_reset_bit(unsigned short addr, unsigned char bitmask){
    write(addr, (read(addr) & (~bitmask)));
}
unsigned char cpu_set_bit(unsigned char var, unsigned char bitmask){
    return var | bitmask;
}
inline void cpu_set_bit(unsigned short addr, unsigned char bitmask){
    write(addr, (read(addr) | bitmask));
}

//yes, i did do opcode decoding with a 512 case switch statement
//am i insane? probably a little.. but it is efficient since it will become a jump table
void run_cpu_cycle(){
    if (illegalOperation){
        //mute sound
        m[AUDIO_MASTER_ENABLE] = 0;
        refresh_audio_master_enable();
        if (!illegalOp_done_print){
            illegalOp_done_print = true;
            if (LOG_OPERATIONS){
                print_cpu_operations_log();
            }
            std::cout << "\n\n ===== Illegal Operation Encountered!! =====";
            print_stack_and_register_log();
        }
        return;
    }
    if (m_cycle_delay <= 0){
        if (interrupt_delay > 0){
            interrupt_delay++;
        }
        if (interrupt_delay >= 3){
            InterruptMasterEnable = true;
            interrupt_delay = 0;
        }

        interrupt_pending = ((m[INTERRUPT_FLAG] & 0x1f) & m[INTERRUPT_ENABLE]) != 0;
        if (interrupt_pending){
            isHalted = false;
        }
        if (interrupt_pending && InterruptMasterEnable){
            unsigned char flags = m[INTERRUPT_FLAG] | 0x80;
            unsigned char bitflip_mask = 1;
            while ((flags & 0x01) == 0){
                flags >>= 1;
                bitflip_mask <<= 1;
            }
            //default case should never be reached, but if it does, it stops the cpu;
            unsigned short new_addr = 0xfec0;
            switch (bitflip_mask){
                case 0x01: new_addr = 0x40; break;
                case 0x02: new_addr = 0x48; break;
                case 0x04: new_addr = 0x50; break;
                case 0x08: new_addr = 0x58; break;
                case 0x10: new_addr = 0x60; break;
                default: illegalOperation = true; return;
            }
            m[INTERRUPT_FLAG] = m[INTERRUPT_FLAG] ^ bitflip_mask; //reset interrupt flag
            InterruptMasterEnable = false; //prevent an interrupt from happening usually until reti is called
            cpu_stack_push_word(PC);
            PC.W = new_addr; //jump to appropriate interrupt handler
            m_cycle_delay = 5;
            interrupt_delay = 0;
            return;
        }
        if (isHalted){
            return;
        }
        //fetch opcode
        opcode = read(PC.W);
        if (opcode == 0xcb){
            PC.W++;
            opcode = read(PC.W) + 0x100;
        }
        m_cycle_delay = op_default_delays[opcode]; //most instructions use only 1 delay value

        if (LOG_OPERATIONS){
            log_operations_circular_buffer[operations_log_head] = PC.H; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = PC.L; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = read(PC.W); operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = read(PC.W+1); operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = read(PC.W+2); operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = (0x20*InterruptMasterEnable)+(m[INTERRUPT_ENABLE] & 0x1f); operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = A; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = BC.H; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = BC.L; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = DE.H; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = DE.L; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = HL.H; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = HL.L; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = SP.H; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = SP.L; operations_log_head++;
            log_operations_circular_buffer[operations_log_head] = cpu_get_flag_byte(); operations_log_head++;
        }
        cpu_log_working_ops(opcode);
        //decode and execute
        switch (opcode) {
            case 0x00: m_cycle_delay = 1; break;
            case 0x01: BC.W = cpu_get_imm_word(); break;
            case 0x02: write(BC.W, A); break;
            case 0x03: BC.W++; break;
            case 0x04: BC.H = cpu_inc_dec(BC.H, false); break;
            case 0x05: BC.H = cpu_inc_dec(BC.H, true); break;
            case 0x06: PC.W++; BC.H = read(PC.W); break;
            case 0x07: A = cpu_rotate_left(A); Zero = false; break;
            case 0x08: write(cpu_get_imm_word(), SP); break;
            case 0x09: cpu_add_hl_16(BC); break;
            case 0x0a: A = read(BC.W); break;
            case 0x0b: BC.W--; break;
            case 0x0c: BC.L = cpu_inc_dec(BC.L, false); break;
            case 0x0d: BC.L = cpu_inc_dec(BC.L, true); break;
            case 0x0e: PC.W++; BC.L = read(PC.W); break;
            case 0x0f: A = cpu_rotate_right(A); Zero = false; break;
            case 0x10: isStopped = true; PC.W++; break;
            case 0x11: DE.W = cpu_get_imm_word(); break;
            case 0x12: write(DE.W, A); break;
            case 0x13: DE.W++; break;
            case 0x14: DE.H = cpu_inc_dec(DE.H, false); break;
            case 0x15: DE.H = cpu_inc_dec(DE.H, true); break;
            case 0x16: PC.W++; DE.H = read(PC.W); break;
            case 0x17: A = cpu_shift_left(A, Carry); Zero = false; break;
            case 0x18: cpu_jump_rel(3, true); break;
            case 0x19: cpu_add_hl_16(DE); break;
            case 0x1a: A = read(DE.W); break;
            case 0x1b: DE.W--; break;
            case 0x1c: DE.L = cpu_inc_dec(DE.L, false); break;
            case 0x1d: DE.L = cpu_inc_dec(DE.L, true); break;
            case 0x1e: PC.W++; DE.L = read(PC.W); break;
            case 0x1f: A = cpu_shift_right(A, Carry); Zero = false; break;

            case 0x20: cpu_jump_rel(3, Zero == false); break;
            case 0x21: HL.W = cpu_get_imm_word(); break;
            case 0x22: write(HL.W, A); HL.W++; break;
            case 0x23: HL.W++; break;
            case 0x24: HL.H = cpu_inc_dec(HL.H, false); break;
            case 0x25: HL.H = cpu_inc_dec(HL.H, true); break;
            case 0x26: PC.W++; HL.H = read(PC.W); break;
            case 0x27: cpu_daa_adjust(); break;
            case 0x28: cpu_jump_rel(3, Zero == true); break;
            case 0x29: cpu_add_hl_16(HL); break;
            case 0x2a: A = read(HL.W); HL.W++; break;
            case 0x2b: HL.W--; break;
            case 0x2c: HL.L = cpu_inc_dec(HL.L, false); break;
            case 0x2d: HL.L = cpu_inc_dec(HL.L, true); break;
            case 0x2e: PC.W++; HL.L = read(PC.W); break;
            case 0x2f: A = ~A; Negative = true; HalfCarry = true; break;
            case 0x30: cpu_jump_rel(3, Carry == false); break;
            case 0x31: SP.W = cpu_get_imm_word(); break;
            case 0x32: write(HL.W, A); HL.W--; break;
            case 0x33: SP.W++; break;
            case 0x34: write(HL.W, cpu_inc_dec(read(HL.W), false)); break;
            case 0x35: write(HL.W, cpu_inc_dec(read(HL.W), true)); break;
            case 0x36: PC.W++; write(HL.W, read(PC.W)); break;
            case 0x37: Carry = true; Negative = false; HalfCarry = false; break;
            case 0x38: cpu_jump_rel(3, Carry == true); break;
            case 0x39: cpu_add_hl_16(SP); break;
            case 0x3a: A = read(HL.W); HL.W--; break;
            case 0x3b: SP.W--; break;
            case 0x3c: A = cpu_inc_dec(A, false); break;
            case 0x3d: A = cpu_inc_dec(A, true); break;
            case 0x3e: PC.W++; A = read(PC.W); break;
            case 0x3f: Carry = !Carry; Negative = false; HalfCarry = false; break;

            case 0x40: BC.H = BC.H; break; //ld r8,r8 operations
            case 0x41: BC.H = BC.L; break;
            case 0x42: BC.H = DE.H; break;
            case 0x43: BC.H = DE.L; break;
            case 0x44: BC.H = HL.H; break;
            case 0x45: BC.H = HL.L; break;
            case 0x46: BC.H = read(HL.W); break;
            case 0x47: BC.H = A; break;
            case 0x48: BC.L = BC.H; break;
            case 0x49: BC.L = BC.L; break;
            case 0x4a: BC.L = DE.H; break;
            case 0x4b: BC.L = DE.L; break;
            case 0x4c: BC.L = HL.H; break;
            case 0x4d: BC.L = HL.L; break;
            case 0x4e: BC.L = read(HL.W); break;
            case 0x4f: BC.L = A; break;
            case 0x50: DE.H = BC.H; break;
            case 0x51: DE.H = BC.L; break;
            case 0x52: DE.H = DE.H; break;
            case 0x53: DE.H = DE.L; break;
            case 0x54: DE.H = HL.H; break;
            case 0x55: DE.H = HL.L; break;
            case 0x56: DE.H = read(HL.W); break;
            case 0x57: DE.H = A; break;
            case 0x58: DE.L = BC.H; break;
            case 0x59: DE.L = BC.L; break;
            case 0x5a: DE.L = DE.H; break;
            case 0x5b: DE.L = DE.L; break;
            case 0x5c: DE.L = HL.H; break;
            case 0x5d: DE.L = HL.L; break;
            case 0x5e: DE.L = read(HL.W); break;
            case 0x5f: DE.L = A; break;

            case 0x60: HL.H = BC.H; break;
            case 0x61: HL.H = BC.L; break;
            case 0x62: HL.H = DE.H; break;
            case 0x63: HL.H = DE.L; break;
            case 0x64: HL.H = HL.H; break;
            case 0x65: HL.H = HL.L; break;
            case 0x66: HL.H = read(HL.W); break;
            case 0x67: HL.H = A; break;
            case 0x68: HL.L = BC.H; break;
            case 0x69: HL.L = BC.L; break;
            case 0x6a: HL.L = DE.H; break;
            case 0x6b: HL.L = DE.L; break;
            case 0x6c: HL.L = HL.H; break;
            case 0x6d: HL.L = HL.L; break;
            case 0x6e: HL.L = read(HL.W); break;
            case 0x6f: HL.L = A; break;
            case 0x70: write(HL.W, BC.H); break;
            case 0x71: write(HL.W, BC.L); break;
            case 0x72: write(HL.W, DE.H); break;
            case 0x73: write(HL.W, DE.L); break;
            case 0x74: write(HL.W, HL.H); break;
            case 0x75: write(HL.W, HL.L); break;
            case 0x76: isHalted = true; break; //Halt instruction
            case 0x77: write(HL.W, A); break;
            case 0x78:   A  = BC.H; break;
            case 0x79:   A  = BC.L; break;
            case 0x7a:   A  = DE.H; break;
            case 0x7b:   A  = DE.L; break;
            case 0x7c:   A  = HL.H; break;
            case 0x7d:   A  = HL.L; break;
            case 0x7e:   A  = read(HL.W); break;
            case 0x7f:   A  = A; break;

            case 0x80: cpu_add_sub(BC.H,false,false); break; //ALU operations
            case 0x81: cpu_add_sub(BC.L,false,false); break;
            case 0x82: cpu_add_sub(DE.H,false,false); break;
            case 0x83: cpu_add_sub(DE.L,false,false); break;
            case 0x84: cpu_add_sub(HL.H,false,false); break;
            case 0x85: cpu_add_sub(HL.L,false,false); break;
            case 0x86: cpu_add_sub(read(HL.W),false,false); break;
            case 0x87: cpu_add_sub(A,false,false); break;
            case 0x88: cpu_add_sub(BC.H,Carry,false); break;
            case 0x89: cpu_add_sub(BC.L,Carry,false); break;
            case 0x8a: cpu_add_sub(DE.H,Carry,false); break;
            case 0x8b: cpu_add_sub(DE.L,Carry,false); break;
            case 0x8c: cpu_add_sub(HL.H,Carry,false); break;
            case 0x8d: cpu_add_sub(HL.L,Carry,false); break;
            case 0x8e: cpu_add_sub(read(HL.W),Carry,false); break;
            case 0x8f: cpu_add_sub(A,Carry,false); break;
            case 0x90: cpu_add_sub(BC.H,true,true); break;
            case 0x91: cpu_add_sub(BC.L,true,true); break;
            case 0x92: cpu_add_sub(DE.H,true,true); break;
            case 0x93: cpu_add_sub(DE.L,true,true); break;
            case 0x94: cpu_add_sub(HL.H,true,true); break;
            case 0x95: cpu_add_sub(HL.L,true,true); break;
            case 0x96: cpu_add_sub(read(HL.W),true,true); break;
            case 0x97: cpu_add_sub(A,true,true); break;
            case 0x98: cpu_add_sub(BC.H,!Carry,true); break;
            case 0x99: cpu_add_sub(BC.L,!Carry,true); break;
            case 0x9a: cpu_add_sub(DE.H,!Carry,true); break;
            case 0x9b: cpu_add_sub(DE.L,!Carry,true); break;
            case 0x9c: cpu_add_sub(HL.H,!Carry,true); break;
            case 0x9d: cpu_add_sub(HL.L,!Carry,true); break;
            case 0x9e: cpu_add_sub(read(HL.W),!Carry,true); break;
            case 0x9f: cpu_add_sub(A,!Carry,true); break;

            case 0xa0: cpu_logic_and(BC.H); break;
            case 0xa1: cpu_logic_and(BC.L); break;
            case 0xa2: cpu_logic_and(DE.H); break;
            case 0xa3: cpu_logic_and(DE.L); break;
            case 0xa4: cpu_logic_and(HL.H); break;
            case 0xa5: cpu_logic_and(HL.L); break;
            case 0xa6: cpu_logic_and(read(HL.W)); break;
            case 0xa7: cpu_logic_and(A); break;
            case 0xa8: cpu_logic_xor(BC.H); break;
            case 0xa9: cpu_logic_xor(BC.L); break;
            case 0xaa: cpu_logic_xor(DE.H); break;
            case 0xab: cpu_logic_xor(DE.L); break;
            case 0xac: cpu_logic_xor(HL.H); break;
            case 0xad: cpu_logic_xor(HL.L); break;
            case 0xae: cpu_logic_xor(read(HL.W)); break;
            case 0xaf: cpu_logic_xor(A); break;
            case 0xb0: cpu_logic_or(BC.H); break;
            case 0xb1: cpu_logic_or(BC.L); break;
            case 0xb2: cpu_logic_or(DE.H); break;
            case 0xb3: cpu_logic_or(DE.L); break;
            case 0xb4: cpu_logic_or(HL.H); break;
            case 0xb5: cpu_logic_or(HL.L); break;
            case 0xb6: cpu_logic_or(read(HL.W)); break;
            case 0xb7: cpu_logic_or(A); break;
            case 0xb8: cpu_compare(BC.H); break;
            case 0xb9: cpu_compare(BC.L); break;
            case 0xba: cpu_compare(DE.H); break;
            case 0xbb: cpu_compare(DE.L); break;
            case 0xbc: cpu_compare(HL.H); break;
            case 0xbd: cpu_compare(HL.L); break;
            case 0xbe: cpu_compare(read(HL.W)); break;
            case 0xbf: cpu_compare(A); break;

            case 0xc0: cpu_return(5, Zero == false); break;
            case 0xc1: BC.W = cpu_stack_pop_word(); break;
            case 0xc2: cpu_jump_a16(4, Zero == false); break;
            case 0xc3: cpu_jump_a16(4); break;
            case 0xc4: cpu_call(6, Zero == false); break;
            case 0xc5: cpu_stack_push_word(BC); break;
            case 0xc6: PC.W++; cpu_add_sub(read(PC.W), false, false); break;
            case 0xc7: cpu_stack_push_word(PC.W+1); PC.W = 0xffff; break;
            case 0xc8: cpu_return(5, Zero == true); break;
            case 0xc9: cpu_return(4); break;
            case 0xca: cpu_jump_a16(4, Zero == true); break;
            case 0xcb: illegalOperation = true; break;
            case 0xcc: cpu_call(6, Zero == true); break;
            case 0xcd: cpu_call(6); break;
            case 0xce: PC.W++; cpu_add_sub(read(PC.W), Carry, false); break;
            case 0xcf: cpu_stack_push_word(PC.W+1); PC.W = 0x0007; break;
            case 0xd0: cpu_return(5, Carry == false); break;
            case 0xd1: DE.W = cpu_stack_pop_word(); break;
            case 0xd2: cpu_jump_a16(4, Carry == false); break;
            case 0xd3: illegalOperation = true; break;
            case 0xd4: cpu_call(6, Carry == false); break;
            case 0xd5: cpu_stack_push_word(DE); break;
            case 0xd6: PC.W++; cpu_add_sub(read(PC.W), true, true); break;
            case 0xd7: cpu_stack_push_word(PC.W+1); PC.W = 0x000f; break;
            case 0xd8: cpu_return(5, Carry == true); break;
            case 0xd9: cpu_return(4); InterruptMasterEnable = true; break;
            case 0xda: cpu_jump_a16(4, Carry == true); break;
            case 0xdb: illegalOperation = true; break;
            case 0xdc: cpu_call(6, Carry == true); break;
            case 0xdd: illegalOperation = true; break;
            case 0xde: PC.W++; cpu_add_sub(read(PC.W), !Carry, true); break;
            case 0xdf: cpu_stack_push_word(PC.W+1); PC.W = 0x0017; break;

            case 0xe0: PC.W++; write((0xff00 + read(PC.W)), A);break;
            case 0xe1: HL.W = cpu_stack_pop_word(); break;
            case 0xe2: write((0xff00 + BC.L), A); break;
            case 0xe3: illegalOperation = true; break;
            case 0xe4: illegalOperation = true; break;
            case 0xe5: cpu_stack_push_word(HL); break;
            case 0xe6: PC.W++; cpu_logic_and(read(PC.W)); break;
            case 0xe7: cpu_stack_push_word(PC.W+1); PC.W = 0x001f; break;
            case 0xe8: SP.W = cpu_add_sp_relative(); break;
            case 0xe9: cpu_jump(HL.W, 1); break;
            case 0xea: write(cpu_get_imm_word(), A); break;
            case 0xeb: illegalOperation = true; break;
            case 0xec: illegalOperation = true; break;
            case 0xed: illegalOperation = true; break;
            case 0xee: PC.W++; cpu_logic_xor(read(PC.W)); break;
            case 0xef: cpu_stack_push_word(PC.W+1); PC.W = 0x0027; break;
            case 0xf0: PC.W++; A = read((0xff00 + read(PC.W)));break;
            case 0xf1: cpu_set_flags(read(SP.W)); SP.W++; A = read(SP.W); SP.W++; break;
            case 0xf2: A = read((0xff00 + BC.L)); break;
            case 0xf3: InterruptMasterEnable = false; interrupt_delay = 0; break;
            case 0xf4: illegalOperation = true; break;
            case 0xf5: SP.W--; write(SP.W,A); SP.W--; write(SP.W,cpu_get_flag_byte()); break;
            case 0xf6: PC.W++; cpu_logic_or(read(PC.W)); break;
            case 0xf7: cpu_stack_push_word(PC.W+1); PC.W = 0x002f; break;
            case 0xf8: HL.W = cpu_add_sp_relative(); break;
            case 0xf9: SP.W = HL.W; break;
            case 0xfa: A = read(cpu_get_imm_word()); break;
            case 0xfb: interrupt_delay = 1; break;
            case 0xfc: illegalOperation = true; break;
            case 0xfd: illegalOperation = true; break;
            case 0xfe: PC.W++; cpu_compare(read(PC.W)); break;
            case 0xff: cpu_stack_push_word(PC.W+1); PC.W = 0x0037; break;

            //CB Prefix operations
            case 0x100: BC.H = cpu_rotate_left(BC.H); break; //Shift and Rotate ops
            case 0x101: BC.L = cpu_rotate_left(BC.L); break;
            case 0x102: DE.H = cpu_rotate_left(DE.H); break;
            case 0x103: DE.L = cpu_rotate_left(DE.L); break;
            case 0x104: HL.H = cpu_rotate_left(HL.H); break;
            case 0x105: HL.L = cpu_rotate_left(HL.L); break;
            case 0x106: write(HL.W, cpu_rotate_left(read(HL.W))); break;
            case 0x107:   A  = cpu_rotate_left(A); break;
            case 0x108: BC.H = cpu_rotate_right(BC.H); break;
            case 0x109: BC.L = cpu_rotate_right(BC.L); break;
            case 0x10a: DE.H = cpu_rotate_right(DE.H); break;
            case 0x10b: DE.L = cpu_rotate_right(DE.L); break;
            case 0x10c: HL.H = cpu_rotate_right(HL.H); break;
            case 0x10d: HL.L = cpu_rotate_right(HL.L); break;
            case 0x10e: write(HL.W, cpu_rotate_right(read(HL.W))); break;
            case 0x10f:   A  = cpu_rotate_right(A); break;
            case 0x110: BC.H = cpu_shift_left(BC.H, Carry); break;
            case 0x111: BC.L = cpu_shift_left(BC.L, Carry); break;
            case 0x112: DE.H = cpu_shift_left(DE.H, Carry); break;
            case 0x113: DE.L = cpu_shift_left(DE.L, Carry); break;
            case 0x114: HL.H = cpu_shift_left(HL.H, Carry); break;
            case 0x115: HL.L = cpu_shift_left(HL.L, Carry); break;
            case 0x116: write(HL.W, cpu_shift_left(read(HL.W), Carry)); break;
            case 0x117:   A  = cpu_shift_left(A, Carry); break;
            case 0x118: BC.H = cpu_shift_right(BC.H, Carry); break;
            case 0x119: BC.L = cpu_shift_right(BC.L, Carry); break;
            case 0x11a: DE.H = cpu_shift_right(DE.H, Carry); break;
            case 0x11b: DE.L = cpu_shift_right(DE.L, Carry); break;
            case 0x11c: HL.H = cpu_shift_right(HL.H, Carry); break;
            case 0x11d: HL.L = cpu_shift_right(HL.L, Carry); break;
            case 0x11e: write(HL.W, cpu_shift_right(read(HL.W), Carry)); break;
            case 0x11f:   A  = cpu_shift_right(A, Carry); break;

            case 0x120: BC.H = cpu_shift_left(BC.H, false); break;
            case 0x121: BC.L = cpu_shift_left(BC.L, false); break;
            case 0x122: DE.H = cpu_shift_left(DE.H, false); break;
            case 0x123: DE.L = cpu_shift_left(DE.L, false); break;
            case 0x124: HL.H = cpu_shift_left(HL.H, false); break;
            case 0x125: HL.L = cpu_shift_left(HL.L, false); break;
            case 0x126: write(HL.W, cpu_shift_left(read(HL.W), false)); break;
            case 0x127:   A  = cpu_shift_left(A, false); break;
            case 0x128: BC.H = cpu_shift_right(BC.H, ((BC.H & 0x80) != 0)); break;
            case 0x129: BC.L = cpu_shift_right(BC.L, ((BC.L & 0x80) != 0)); break;
            case 0x12a: DE.H = cpu_shift_right(DE.H, ((DE.H & 0x80) != 0)); break;
            case 0x12b: DE.L = cpu_shift_right(DE.L, ((DE.L & 0x80) != 0)); break;
            case 0x12c: HL.H = cpu_shift_right(HL.H, ((HL.H & 0x80) != 0)); break;
            case 0x12d: HL.L = cpu_shift_right(HL.L, ((HL.L & 0x80) != 0)); break;
            case 0x12e: write(HL.W, cpu_shift_right(read(HL.W), ((read(HL.W) & 0x80) != 0))); break;
            case 0x12f:   A  = cpu_shift_right(A, ((A & 0x80) != 0)); break;
            case 0x130: BC.H = cpu_swap_nybbles(BC.H); break;
            case 0x131: BC.L = cpu_swap_nybbles(BC.L); break;
            case 0x132: DE.H = cpu_swap_nybbles(DE.H); break;
            case 0x133: DE.L = cpu_swap_nybbles(DE.L); break;
            case 0x134: HL.H = cpu_swap_nybbles(HL.H); break;
            case 0x135: HL.L = cpu_swap_nybbles(HL.L); break;
            case 0x136: write(HL.W, cpu_swap_nybbles(read(HL.W))); break;
            case 0x137:   A  = cpu_swap_nybbles(A); break;
            case 0x138: BC.H = cpu_shift_right(BC.H, false); break;
            case 0x139: BC.L = cpu_shift_right(BC.L, false); break;
            case 0x13a: DE.H = cpu_shift_right(DE.H, false); break;
            case 0x13b: DE.L = cpu_shift_right(DE.L, false); break;
            case 0x13c: HL.H = cpu_shift_right(HL.H, false); break;
            case 0x13d: HL.L = cpu_shift_right(HL.L, false); break;
            case 0x13e: write(HL.W, cpu_shift_right(read(HL.W), false)); break;
            case 0x13f:   A  = cpu_shift_right(A, false); break;

            case 0x140: cpu_test_bit(BC.H, 0b00000001); break;
            case 0x141: cpu_test_bit(BC.L, 0b00000001); break;
            case 0x142: cpu_test_bit(DE.H, 0b00000001); break;
            case 0x143: cpu_test_bit(DE.L, 0b00000001); break;
            case 0x144: cpu_test_bit(HL.H, 0b00000001); break;
            case 0x145: cpu_test_bit(HL.L, 0b00000001); break;
            case 0x146: cpu_test_bit(HL.W, 0b00000001); break;
            case 0x147: cpu_test_bit(  A,  0b00000001); break;
            case 0x148: cpu_test_bit(BC.H, 0b00000010); break;
            case 0x149: cpu_test_bit(BC.L, 0b00000010); break;
            case 0x14a: cpu_test_bit(DE.H, 0b00000010); break;
            case 0x14b: cpu_test_bit(DE.L, 0b00000010); break;
            case 0x14c: cpu_test_bit(HL.H, 0b00000010); break;
            case 0x14d: cpu_test_bit(HL.L, 0b00000010); break;
            case 0x14e: cpu_test_bit(HL.W, 0b00000010); break;
            case 0x14f: cpu_test_bit(  A,  0b00000010); break;
            case 0x150: cpu_test_bit(BC.H, 0b00000100); break;
            case 0x151: cpu_test_bit(BC.L, 0b00000100); break;
            case 0x152: cpu_test_bit(DE.H, 0b00000100); break;
            case 0x153: cpu_test_bit(DE.L, 0b00000100); break;
            case 0x154: cpu_test_bit(HL.H, 0b00000100); break;
            case 0x155: cpu_test_bit(HL.L, 0b00000100); break;
            case 0x156: cpu_test_bit(HL.W, 0b00000100); break;
            case 0x157: cpu_test_bit(  A,  0b00000100); break;
            case 0x158: cpu_test_bit(BC.H, 0b00001000); break;
            case 0x159: cpu_test_bit(BC.L, 0b00001000); break;
            case 0x15a: cpu_test_bit(DE.H, 0b00001000); break;
            case 0x15b: cpu_test_bit(DE.L, 0b00001000); break;
            case 0x15c: cpu_test_bit(HL.H, 0b00001000); break;
            case 0x15d: cpu_test_bit(HL.L, 0b00001000); break;
            case 0x15e: cpu_test_bit(HL.W, 0b00001000); break;
            case 0x15f: cpu_test_bit(  A,  0b00001000); break;

            case 0x160: cpu_test_bit(BC.H, 0b00010000); break;
            case 0x161: cpu_test_bit(BC.L, 0b00010000); break;
            case 0x162: cpu_test_bit(DE.H, 0b00010000); break;
            case 0x163: cpu_test_bit(DE.L, 0b00010000); break;
            case 0x164: cpu_test_bit(HL.H, 0b00010000); break;
            case 0x165: cpu_test_bit(HL.L, 0b00010000); break;
            case 0x166: cpu_test_bit(HL.W, 0b00010000); break;
            case 0x167: cpu_test_bit(  A,  0b00010000); break;
            case 0x168: cpu_test_bit(BC.H, 0b00100000); break;
            case 0x169: cpu_test_bit(BC.L, 0b00100000); break;
            case 0x16a: cpu_test_bit(DE.H, 0b00100000); break;
            case 0x16b: cpu_test_bit(DE.L, 0b00100000); break;
            case 0x16c: cpu_test_bit(HL.H, 0b00100000); break;
            case 0x16d: cpu_test_bit(HL.L, 0b00100000); break;
            case 0x16e: cpu_test_bit(HL.W, 0b00100000); break;
            case 0x16f: cpu_test_bit(  A,  0b00100000); break;
            case 0x170: cpu_test_bit(BC.H, 0b01000000); break;
            case 0x171: cpu_test_bit(BC.L, 0b01000000); break;
            case 0x172: cpu_test_bit(DE.H, 0b01000000); break;
            case 0x173: cpu_test_bit(DE.L, 0b01000000); break;
            case 0x174: cpu_test_bit(HL.H, 0b01000000); break;
            case 0x175: cpu_test_bit(HL.L, 0b01000000); break;
            case 0x176: cpu_test_bit(HL.W, 0b01000000); break;
            case 0x177: cpu_test_bit(  A,  0b01000000); break;
            case 0x178: cpu_test_bit(BC.H, 0b10000000); break;
            case 0x179: cpu_test_bit(BC.L, 0b10000000); break;
            case 0x17a: cpu_test_bit(DE.H, 0b10000000); break;
            case 0x17b: cpu_test_bit(DE.L, 0b10000000); break;
            case 0x17c: cpu_test_bit(HL.H, 0b10000000); break;
            case 0x17d: cpu_test_bit(HL.L, 0b10000000); break;
            case 0x17e: cpu_test_bit(HL.W, 0b10000000); break;
            case 0x17f: cpu_test_bit(  A,  0b10000000); break;

            case 0x180: BC.H = cpu_reset_bit(BC.H, 0b00000001); break;
            case 0x181: BC.L = cpu_reset_bit(BC.L, 0b00000001); break;
            case 0x182: DE.H = cpu_reset_bit(DE.H, 0b00000001); break;
            case 0x183: DE.L = cpu_reset_bit(DE.L, 0b00000001); break;
            case 0x184: HL.H = cpu_reset_bit(HL.H, 0b00000001); break;
            case 0x185: HL.L = cpu_reset_bit(HL.L, 0b00000001); break;
            case 0x186: cpu_reset_bit(HL.W, 0b00000001); break;
            case 0x187: A = cpu_reset_bit(  A,  0b00000001); break;
            case 0x188: BC.H = cpu_reset_bit(BC.H, 0b00000010); break;
            case 0x189: BC.L = cpu_reset_bit(BC.L, 0b00000010); break;
            case 0x18a: DE.H = cpu_reset_bit(DE.H, 0b00000010); break;
            case 0x18b: DE.L = cpu_reset_bit(DE.L, 0b00000010); break;
            case 0x18c: HL.H = cpu_reset_bit(HL.H, 0b00000010); break;
            case 0x18d: HL.L = cpu_reset_bit(HL.L, 0b00000010); break;
            case 0x18e: cpu_reset_bit(HL.W, 0b00000010); break;
            case 0x18f: A = cpu_reset_bit(  A,  0b00000010); break;
            case 0x190: BC.H = cpu_reset_bit(BC.H, 0b00000100); break;
            case 0x191: BC.L = cpu_reset_bit(BC.L, 0b00000100); break;
            case 0x192: DE.H = cpu_reset_bit(DE.H, 0b00000100); break;
            case 0x193: DE.L = cpu_reset_bit(DE.L, 0b00000100); break;
            case 0x194: HL.H = cpu_reset_bit(HL.H, 0b00000100); break;
            case 0x195: HL.L = cpu_reset_bit(HL.L, 0b00000100); break;
            case 0x196: cpu_reset_bit(HL.W, 0b00000100); break;
            case 0x197: A = cpu_reset_bit(  A,  0b00000100); break;
            case 0x198: BC.H = cpu_reset_bit(BC.H, 0b00001000); break;
            case 0x199: BC.L = cpu_reset_bit(BC.L, 0b00001000); break;
            case 0x19a: DE.H = cpu_reset_bit(DE.H, 0b00001000); break;
            case 0x19b: DE.L = cpu_reset_bit(DE.L, 0b00001000); break;
            case 0x19c: HL.H = cpu_reset_bit(HL.H, 0b00001000); break;
            case 0x19d: HL.L = cpu_reset_bit(HL.L, 0b00001000); break;
            case 0x19e: cpu_reset_bit(HL.W, 0b00001000); break;
            case 0x19f: A = cpu_reset_bit(  A,  0b00001000); break;

            case 0x1a0: BC.H = cpu_reset_bit(BC.H, 0b00010000); break;
            case 0x1a1: BC.L = cpu_reset_bit(BC.L, 0b00010000); break;
            case 0x1a2: DE.H = cpu_reset_bit(DE.H, 0b00010000); break;
            case 0x1a3: DE.L = cpu_reset_bit(DE.L, 0b00010000); break;
            case 0x1a4: HL.H = cpu_reset_bit(HL.H, 0b00010000); break;
            case 0x1a5: HL.L = cpu_reset_bit(HL.L, 0b00010000); break;
            case 0x1a6: cpu_reset_bit(HL.W, 0b00010000); break;
            case 0x1a7: A = cpu_reset_bit(  A,  0b00010000); break;
            case 0x1a8: BC.H = cpu_reset_bit(BC.H, 0b00100000); break;
            case 0x1a9: BC.L = cpu_reset_bit(BC.L, 0b00100000); break;
            case 0x1aa: DE.H = cpu_reset_bit(DE.H, 0b00100000); break;
            case 0x1ab: DE.L = cpu_reset_bit(DE.L, 0b00100000); break;
            case 0x1ac: HL.H = cpu_reset_bit(HL.H, 0b00100000); break;
            case 0x1ad: HL.L = cpu_reset_bit(HL.L, 0b00100000); break;
            case 0x1ae: cpu_reset_bit(HL.W, 0b00100000); break;
            case 0x1af: A = cpu_reset_bit(  A,  0b00100000); break;
            case 0x1b0: BC.H = cpu_reset_bit(BC.H, 0b01000000); break;
            case 0x1b1: BC.L = cpu_reset_bit(BC.L, 0b01000000); break;
            case 0x1b2: DE.H = cpu_reset_bit(DE.H, 0b01000000); break;
            case 0x1b3: DE.L = cpu_reset_bit(DE.L, 0b01000000); break;
            case 0x1b4: HL.H = cpu_reset_bit(HL.H, 0b01000000); break;
            case 0x1b5: HL.L = cpu_reset_bit(HL.L, 0b01000000); break;
            case 0x1b6: cpu_reset_bit(HL.W, 0b01000000); break;
            case 0x1b7: A = cpu_reset_bit(  A,  0b01000000); break;
            case 0x1b8: BC.H = cpu_reset_bit(BC.H, 0b10000000); break;
            case 0x1b9: BC.L = cpu_reset_bit(BC.L, 0b10000000); break;
            case 0x1ba: DE.H = cpu_reset_bit(DE.H, 0b10000000); break;
            case 0x1bb: DE.L = cpu_reset_bit(DE.L, 0b10000000); break;
            case 0x1bc: HL.H = cpu_reset_bit(HL.H, 0b10000000); break;
            case 0x1bd: HL.L = cpu_reset_bit(HL.L, 0b10000000); break;
            case 0x1be: cpu_reset_bit(HL.W, 0b10000000); break;
            case 0x1bf: A = cpu_reset_bit(  A,  0b10000000); break;

            case 0x1c0: BC.H = cpu_set_bit(BC.H, 0b00000001); break;
            case 0x1c1: BC.L = cpu_set_bit(BC.L, 0b00000001); break;
            case 0x1c2: DE.H = cpu_set_bit(DE.H, 0b00000001); break;
            case 0x1c3: DE.L = cpu_set_bit(DE.L, 0b00000001); break;
            case 0x1c4: HL.H = cpu_set_bit(HL.H, 0b00000001); break;
            case 0x1c5: HL.L = cpu_set_bit(HL.L, 0b00000001); break;
            case 0x1c6: cpu_set_bit(HL.W, 0b00000001); break;
            case 0x1c7: A = cpu_set_bit(  A,  0b00000001); break;
            case 0x1c8: BC.H = cpu_set_bit(BC.H, 0b00000010); break;
            case 0x1c9: BC.L = cpu_set_bit(BC.L, 0b00000010); break;
            case 0x1ca: DE.H = cpu_set_bit(DE.H, 0b00000010); break;
            case 0x1cb: DE.L = cpu_set_bit(DE.L, 0b00000010); break;
            case 0x1cc: HL.H = cpu_set_bit(HL.H, 0b00000010); break;
            case 0x1cd: HL.L = cpu_set_bit(HL.L, 0b00000010); break;
            case 0x1ce: cpu_set_bit(HL.W, 0b00000010); break;
            case 0x1cf: A = cpu_set_bit(  A,  0b00000010); break;
            case 0x1d0: BC.H = cpu_set_bit(BC.H, 0b00000100); break;
            case 0x1d1: BC.L = cpu_set_bit(BC.L, 0b00000100); break;
            case 0x1d2: DE.H = cpu_set_bit(DE.H, 0b00000100); break;
            case 0x1d3: DE.L = cpu_set_bit(DE.L, 0b00000100); break;
            case 0x1d4: HL.H = cpu_set_bit(HL.H, 0b00000100); break;
            case 0x1d5: HL.L = cpu_set_bit(HL.L, 0b00000100); break;
            case 0x1d6: cpu_set_bit(HL.W, 0b00000100); break;
            case 0x1d7: A = cpu_set_bit(  A,  0b00000100); break;
            case 0x1d8: BC.H = cpu_set_bit(BC.H, 0b00001000); break;
            case 0x1d9: BC.L = cpu_set_bit(BC.L, 0b00001000); break;
            case 0x1da: DE.H = cpu_set_bit(DE.H, 0b00001000); break;
            case 0x1db: DE.L = cpu_set_bit(DE.L, 0b00001000); break;
            case 0x1dc: HL.H = cpu_set_bit(HL.H, 0b00001000); break;
            case 0x1dd: HL.L = cpu_set_bit(HL.L, 0b00001000); break;
            case 0x1de: cpu_set_bit(HL.W, 0b00001000); break;
            case 0x1df: A = cpu_set_bit(  A,  0b00001000); break;

            case 0x1e0: BC.H = cpu_set_bit(BC.H, 0b00010000); break;
            case 0x1e1: BC.L = cpu_set_bit(BC.L, 0b00010000); break;
            case 0x1e2: DE.H = cpu_set_bit(DE.H, 0b00010000); break;
            case 0x1e3: DE.L = cpu_set_bit(DE.L, 0b00010000); break;
            case 0x1e4: HL.H = cpu_set_bit(HL.H, 0b00010000); break;
            case 0x1e5: HL.L = cpu_set_bit(HL.L, 0b00010000); break;
            case 0x1e6: cpu_set_bit(HL.W, 0b00010000); break;
            case 0x1e7: A = cpu_set_bit(  A,  0b00010000); break;
            case 0x1e8: BC.H = cpu_set_bit(BC.H, 0b00100000); break;
            case 0x1e9: BC.L = cpu_set_bit(BC.L, 0b00100000); break;
            case 0x1ea: DE.H = cpu_set_bit(DE.H, 0b00100000); break;
            case 0x1eb: DE.L = cpu_set_bit(DE.L, 0b00100000); break;
            case 0x1ec: HL.H = cpu_set_bit(HL.H, 0b00100000); break;
            case 0x1ed: HL.L = cpu_set_bit(HL.L, 0b00100000); break;
            case 0x1ee: cpu_set_bit(HL.W, 0b00100000); break;
            case 0x1ef: A = cpu_set_bit(  A,  0b00100000); break;
            case 0x1f0: BC.H = cpu_set_bit(BC.H, 0b01000000); break;
            case 0x1f1: BC.L = cpu_set_bit(BC.L, 0b01000000); break;
            case 0x1f2: DE.H = cpu_set_bit(DE.H, 0b01000000); break;
            case 0x1f3: DE.L = cpu_set_bit(DE.L, 0b01000000); break;
            case 0x1f4: HL.H = cpu_set_bit(HL.H, 0b01000000); break;
            case 0x1f5: HL.L = cpu_set_bit(HL.L, 0b01000000); break;
            case 0x1f6: cpu_set_bit(HL.W, 0b01000000); break;
            case 0x1f7: A = cpu_set_bit(  A,  0b01000000); break;
            case 0x1f8: BC.H = cpu_set_bit(BC.H, 0b10000000); break;
            case 0x1f9: BC.L = cpu_set_bit(BC.L, 0b10000000); break;
            case 0x1fa: DE.H = cpu_set_bit(DE.H, 0b10000000); break;
            case 0x1fb: DE.L = cpu_set_bit(DE.L, 0b10000000); break;
            case 0x1fc: HL.H = cpu_set_bit(HL.H, 0b10000000); break;
            case 0x1fd: HL.L = cpu_set_bit(HL.L, 0b10000000); break;
            case 0x1fe: cpu_set_bit(HL.W, 0b10000000); break;
            case 0x1ff: A = cpu_set_bit(  A,  0b10000000); break;
            
            default:
                illegalOperation = true; //undefined opcode behavior is specified here
        }

        PC.W++;
    }

    m_cycle_delay--;
}

