int errno;
int rcpu_error;
typedef long int off_t;
typedef int pid_t;
typedef unsigned short int	uint16_t;
typedef char uint8_t;
typedef char bool;
#define __KERNEL_SYSCALLS__
#include "platform.h"

#define STACKSIZE 256

// Error codes
#define ERR_FILE_ACCESS 1
#define ERR_FILE_SIZE 2

// Emulator error codes
#define ERR_STACK_OVERFLOW 3
#define ERR_STACK_UNDERFLOW 4
#define ERR_ATH_SUBINSTRUCTION 5
#define ERR_SYSCALL_PRINTF_FMT_STRING 6
#define ERR_SYSCALL_UNKNOWN 7
#define ERR_IO 8

// Not implemented error codes
#define ERR_NOT_IMPLEMENTED 101

int total = 0;
char hexstring[] = "0123456789abcdef";

void printChar(char character) {
    write(1, &character, 1);
}

void printHex(uint16_t number) {
    char printbuffer[5];

    printbuffer[4] = '\n';
    printbuffer[3] = hexstring[number & 0xf];
    printbuffer[2] = hexstring[(number >> 4) & 0xf];
    printbuffer[1] = hexstring[(number >> 8) & 0xf];
    printbuffer[0] = hexstring[(number >> 12) & 0xf];
    write(1, printbuffer, 5);
}

void printNumber(uint16_t number) {
    if (number < 10) {
        write(1, hexstring + number, 1);
    } else {
        printNumber(number/10);
        write(1, hexstring + (number % 10), 1);
    }
}

int strlength(char* str) {
    int length = 0;
    while (str[length] != '\0') {
        length++;
    }
    return length;
}

void print(char* str) {
    write(1, str, strlength(str));
}

void divide(uint16_t numerator, uint16_t denominator, uint16_t* quotient, uint16_t* remainder) {
    if (denominator == 0) {
        return;
    }
    *quotient = 0;
    *remainder = 0;
    int i;
    for (i = 15; i >= 0; i--) {
        *remainder = *remainder << 1;
        *remainder |= (numerator >> i) & 1;
        if (*remainder >= denominator) {
            *remainder -= denominator;
            *quotient |= (1 << i);
        }
    }
}

void stack_push(uint16_t* stack, uint16_t* stack_pointer, uint16_t value, int* error) {
  if (*stack_pointer < STACKSIZE) {
    stack[*stack_pointer] = value;
    (*stack_pointer)++;
  }
  else {
    *error = ERR_STACK_OVERFLOW;
  }
}

uint16_t stack_pop(uint16_t* stack, uint16_t* stack_pointer, int* error) {
  if ((*stack_pointer) == 0) {
    *error = ERR_STACK_UNDERFLOW;
    return 0;
  }
  else {
    (*stack_pointer)--;
    return stack[(*stack_pointer)];
  }
}

void syscall_printf(uint16_t* memory, uint16_t* stack, uint16_t* stack_pointer, int* error) {
  uint16_t fmt_pointer = stack_pop(stack, stack_pointer, error);
  if (*error) return;
  while (memory[fmt_pointer] != 0) { // Strings are terminated with 0
    char character = (char) memory[fmt_pointer];
    uint16_t string_address;
    switch (character) {
      case '%':
        ; // dirty hack
        char next_character = (char) memory[++fmt_pointer]; // get next memory cell content and increment at the same time
        switch (next_character) {
          case '%':
            print("%");
            break;
          case 'd':
            ; // dirty hack
            uint16_t number = stack_pop(stack, stack_pointer, error);
            if (*error) return;
            printNumber(number);
            break;
          case 's':
            string_address = stack_pop(stack, stack_pointer, error);
            while (memory[string_address] != 0) {
                printChar((char) memory[string_address]);
                string_address++;
            }
            break;
          default: // this also catches '\0'
            *error = ERR_SYSCALL_PRINTF_FMT_STRING;
            return;
        }
      break;
      default:
        printChar(character);
    }
    fmt_pointer++;
  }
}

void syscall_fgets(uint16_t* memory, uint16_t* stack, uint16_t* stack_pointer, int* error) {
    uint16_t location = stack_pop(stack, stack_pointer, error);
    uint16_t size = stack_pop(stack, stack_pointer, error);
    uint16_t stream_num = stack_pop(stack, stack_pointer, error);
    uint16_t amount_read = 0;
    char character_read;
    while (amount_read < size) {
        int succes_read = read(stream_num, &character_read, 1);
        if (succes_read != 1) {
            *error = ERR_IO;
            return;
        }
        if (character_read == '\n' || character_read == '\0') {
            break;
        }
        memory[(location + amount_read) & 0xfff] = character_read;
        amount_read++;
    }
    memory[(location + amount_read) & 0xfff] = 0;
    stack_push(stack, stack_pointer, amount_read, error);
}

void syscall_fgetc(uint16_t* memory, uint16_t* stack, uint16_t* stack_pointer, int* error) {
    uint16_t stream_num = stack_pop(stack, stack_pointer, error);
    char character_read;
    int amount_read = read(stream_num, &character_read, 1);
    if (amount_read != 1) {
        *error = ERR_IO;
    }
    stack_push(stack, stack_pointer, character_read, error);
}

void syscall(uint16_t* memory, uint16_t* stack, uint16_t* stack_pointer, int* error) {
  uint16_t syscall_number = stack_pop(stack, stack_pointer, error);
  if (*error) {
    return;
  }
  switch (syscall_number) {
    case 0:
      syscall_printf(memory, stack, stack_pointer, error);
      break;
    case 1:
      syscall_fgets(memory, stack, stack_pointer, error);
      break;
    case 2:
      syscall_fgetc(memory, stack, stack_pointer, error);
      break;
    default:
      *error = ERR_SYSCALL_UNKNOWN;
  }
}

uint16_t ath(uint8_t ath_operation, uint8_t ath_shift_amount, uint16_t source, uint16_t destination) {
  uint16_t quotient, remainder;
  switch (ath_operation) {
    case 0x0: // ADD
      return destination + source;
    break;

    case 0x1: // SUB
      return destination - source;
    break;

    case 0x2: // MUL
      return destination * source;
    break;

    case 0x3: // DIV
      divide(destination, source, &quotient, &remainder);
      return quotient;
    break;

    case 0x4: // LSH
      return source << ath_shift_amount;
    break;

    case 0x5: // RSH
      return source >> ath_shift_amount;
    break;

    case 0x6: // AND
      return destination & source;
    break;

    case 0x7: // OR
      return destination | source;
    break;

    case 0x8: // XOR
      return destination ^ source;
    break;

    case 0x9: // NOT
      return ~source;
    break;

    case 0xa: // INC
      return destination + 1;
    break;

    case 0xb: // DEC
      return destination - 1;
    break;

    default:
      print("ATH subinstruction not implemented.\n");
      _exit(1);
      return 0;
  }
}

int emulate(uint16_t* memory) {
  uint16_t registers[1 << 2];
  registers[0] = 0;
  registers[1] = 0;
  registers[2] = 0;
  registers[3] = 0;
  uint16_t instruction_pointer = 0;
  uint16_t stack_pointer = 0;
  uint16_t stack[STACKSIZE];

  print("Starting emulation\n");
  for(;;) {
    // Get current instruction
    uint16_t current_instruction = memory[instruction_pointer];
    // Decode instruction into parts
    uint8_t opcode = current_instruction & 0xf;
    uint8_t destination = (current_instruction >> 4) & 0x3;
    uint8_t source = (current_instruction >> 6) & 0x3;
    uint16_t large_address = current_instruction >> 6;
    uint8_t ath_operation = (current_instruction >> 8) & 0xf;
    bool ath_store_in_src = (current_instruction >> 12) & 0x1;
    uint8_t ath_shift_amount = (current_instruction >> 13) & 0x7;
    instruction_pointer++;
    // Execute instruction
    switch (opcode) {
      case 0x0: // MOV
        registers[destination] = registers[source];
      break;

      case 0x1: // LDV
        registers[destination] = large_address;
      break;

      case 0x2: // LDA
        registers[destination] = memory[large_address];
      break;

      case 0x3: // LDM
        memory[large_address] = registers[destination];
      break;

      case 0x4: // LDR
        registers[destination] = memory[registers[source]];
      break;

      case 0x5: // LDP
         memory[registers[destination]] = registers[source];
      break;

      case 0x6: // ATH
        ; // because C... https://stackoverflow.com/a/46341408/5431090
        uint16_t result = ath(ath_operation, ath_shift_amount, registers[source], registers[destination]);
        if (ath_store_in_src) {
          registers[source] = result;
        } else {
          registers[destination] = result;
        }
      break;

      case 0x7: // CAL
        // Push instruction pointer onto the stack, so we can return to it later on
        stack_push(stack, &stack_pointer, instruction_pointer, &rcpu_error);
        instruction_pointer = registers[destination];
      break;

      case 0x8: // RET
        // Pop return address from stack
        instruction_pointer = stack_pop(stack, &stack_pointer, &rcpu_error);
      break;

      case 0x9: // JLT
        if (registers[0] < registers[destination]) {
          instruction_pointer = registers[source];
        }
      break;

      case 0xa: // PSH
        stack_push(stack, &stack_pointer, registers[source], &rcpu_error);
      break;

      case 0xb: // POP
        registers[destination] = stack_pop(stack, &stack_pointer, &rcpu_error);
      break;

      case 0xc: // SYS
        syscall(memory, stack, &stack_pointer, &rcpu_error);
      break;

      case 0xd: // HLT
        print("Terminated at 0x");
        printHex(instruction_pointer);
        return 0;
      break;

      case 0xe: // JMP
        instruction_pointer = large_address;
      break;

      case 0xf: // JMR
        instruction_pointer = registers[source];
      break;

    }
    // Error checking
    if (rcpu_error != 0) {
      return rcpu_error;
    }
    // printHex(instruction_pointer);
    // int register_idx;
    // for (register_idx = 0; register_idx < 4; register_idx++) {
    //     printChar('A' + register_idx);
    //     printHex(registers[register_idx]);
    // }
    // printChar('\n');

  }
  return 0;
}

int main(void)
{
    int fd = open("/tmp/rcpu.bin", O_RDONLY, 0);
    uint16_t buffer[1<<16];
    uint16_t amount = read(fd, (void*) buffer, 1<<16);
    // Convert big endian to little endian
    int i;
    for (i = 0; i < (amount / 2); i++) {
        buffer[i] = (buffer[i]>>8) | (buffer[i]<<8);
    }
    print("size=");
    printHex(amount);
    int result = emulate(buffer);
    if (result != 0) {
        print("Emulation failed\n");
    }
    _exit(result);
}
