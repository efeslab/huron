#ifndef __AE_TRAMPS__H
#define __AE_TRAMPS__H

#ifdef __cplusplus
extern "C" {
#endif

#define PUSH_RAX 0x50
#define PUSH_RCX 0x51
#define PUSH_RDX 0x52
#define PUSH_RSI 0x56
#define PUSH_RDI 0x57
#define PUSH_R8  0x5041
#define PUSH_R9  0x5141
#define PUSH_R10 0x5241
#define PUSH_R11 0x5341
#define POP_RAX 0x58
#define POP_RCX 0x59
#define POP_RDX 0x5a
#define POP_RSI 0x5e
#define POP_RDI 0x5f
#define POP_R8  0x5841
#define POP_R9  0x5941
#define POP_R10 0x5a41
#define POP_R11 0x5b41

#define MOV_RAX_IMM64 0xb848
#define CALL_RAX 0xd0ff
#define JMP_RAX  0xe0ff

#define PLACEHOLDER 0x0

#define packed_struct struct __attribute__((packed))

typedef packed_struct
{
    uint8_t push_rax;
    uint8_t push_rcx;
    uint8_t push_rdx;
    uint8_t push_rsi;
    uint8_t push_rdi;
    uint16_t push_r8;   
    uint16_t push_r9;
    uint16_t push_r10;
    uint16_t push_r11;
    uint16_t mov_rax_imm64_1;
    void*    address_of_tramp;
    uint16_t call_rax_1;
    uint16_t pop_r11;
    uint16_t pop_r10;
    uint16_t pop_r9;
    uint16_t pop_r8;
    uint8_t pop_rdi;
    uint8_t pop_rsi;
    uint8_t pop_rdx; 
    uint8_t pop_rcx; 
    uint8_t pop_rax; 
    uint16_t mov_rax_imm64_2;
    void*    return_address;
    uint16_t jmp_rax;
} INJECT_TRAMPOLINE;

static const INJECT_TRAMPOLINE BASE_INJECT_TRAMPOLINE =
    { PUSH_RAX, PUSH_RCX, PUSH_RDX, PUSH_RSI, PUSH_RDI, PUSH_R8, PUSH_R9, PUSH_R10, PUSH_R11,
      MOV_RAX_IMM64, PLACEHOLDER, CALL_RAX,
      POP_R11, POP_R10, POP_R9, POP_R8, POP_RDI, POP_RSI, POP_RDX, POP_RCX, POP_RAX,
      MOV_RAX_IMM64, PLACEHOLDER, JMP_RAX
    };

typedef packed_struct {
    uint16_t mov_rax_imm64;
    void*    target_address;
    uint16_t jmp_rax;
} JUMP_TRAMPOLINE;

static const JUMP_TRAMPOLINE BASE_JUMP_TRAMPOLINE =
    { MOV_RAX_IMM64, PLACEHOLDER, JMP_RAX };
#ifdef __cplusplus
}
#endif

#endif // __AE_TRAMPS__H
