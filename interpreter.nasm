section     .text
global      _start                              ;must be declared for linker (ld)

%define opreg  r10d ; Operator for current instruction, 
%define valreg r11d ; Value for current instruction
%define ictr   r12  ; Current bytecode instruction idx, offset into the instruction stream
%define ictrd  r12d ; Current bytecode instruction idx, offset into the instruction stream
%define sptr   r13  ; Data Stack pointer
%define lptr   r14  ; Loop Stack pointer

%define stk(idx)  dword [sptr-idx*4]

fop_iadd:
    sub     sptr, 4
    mov     eax, stk(0)
    add     eax, stk(1)
    mov     stk(1), eax
    jmp     interpreter_loop

fop_isub:
    sub     sptr, 4
    mov     eax, stk(1)
    sub     eax, stk(0)
    mov     stk(1), eax
    jmp     interpreter_loop

fop_imul:
    sub     sptr, 4
    mov     eax, stk(1)
    imul    eax, stk(0)
    mov     stk(1), eax
    jmp     interpreter_loop

fop_idiv:
    sub     sptr, 4
    mov     eax, stk(1)
    mov     edx, 0
    idiv    dword stk(0)
    mov     stk(1), eax
    jmp     interpreter_loop

fop_pushint:
    mov     stk(0), valreg
    add     sptr, 4
    jmp     interpreter_loop

fop_dup:
    mov     eax, stk(1)
    mov     stk(0), eax
    add     sptr, 4
    jmp     interpreter_loop

fop_swap:
    mov     eax, stk(1)
    mov     ebx, stk(2)
    mov     stk(1), ebx
    mov     stk(2), eax
    jmp     interpreter_loop

fop_drop:
    sub     sptr, 4
    jmp     interpreter_loop

fop_over:
    mov     eax, stk(2)
    mov     stk(0), eax
    add     sptr, 4
    jmp     interpreter_loop

fop_do:
    mov     eax, stk(1)     ; eax=count
    mov     ebx, stk(2)     ; ebx=start
    sub     sptr, 8
    mov     ecx, eax
    add     ecx, ebx        ; ecx=end
    mov     dword [lptr + Loop.instr], r12d
    mov     dword [lptr + Loop.ctr]  , ebx
    mov     dword [lptr + Loop.end]  , ecx
    add     lptr, Loop.size
    jmp     interpreter_loop

fop_range:
    mov     eax, stk(2)     ; eax=start
    mov     ebx, stk(1)     ; ebx=end
    sub     sptr, 8
    mov     dword [lptr + Loop.instr], r12d
    mov     dword [lptr + Loop.ctr]  , eax
    mov     dword [lptr + Loop.end]  , ebx
    add     lptr, Loop.size
    jmp     interpreter_loop

fop_for:
    mov     eax, 0          ; eax=start
    mov     ebx, stk(1)     ; ebx=end
    sub     sptr, 4
    mov     dword [lptr + Loop.instr], r12d
    mov     dword [lptr + Loop.ctr]  , eax
    mov     dword [lptr + Loop.end]  , ebx
    add     lptr, Loop.size
    jmp     interpreter_loop

fop_loop:
    lea     r10, [lptr - Loop.size]     ; r10 = base address for current Loop structure
    mov     eax, dword [r10 + Loop.ctr]
    mov     ebx, dword [r10 + Loop.end]
    sub     ebx, 1
    cmp     eax, ebx
    jge     endloop                     ; Instruction counter was already discarded, jump right away
    inc     eax                         ; Increment loop counter
    mov     dword [r10 + Loop.ctr], eax 
    mov     eax,  dword [r10 + Loop.instr]
    pop     ictr                         ; Discard current instruction counter
    push    rax                         ; Reset instruction counter to the loop beginning
    jmp     interpreter_loop
endloop:
    sub     lptr, Loop.size
    jmp     interpreter_loop
    
fop_lctr:
    lea     rbx, [lptr - Loop.size]
    mov     eax, dword [rbx + Loop.ctr]
    mov     stk(0), eax
    add     sptr, 4
    jmp     interpreter_loop

fop_callw:  
    push    r11
    jmp     interpreter_loop
    
fop_retw:
    pop     ictr
    jmp     interpreter_loop

fop_eof:
    ; syscall sys_exit
    ; Print the stack
.loop:
    sub     sptr, 4
    mov     eax, stk(0)
    call    print_hex
    cmp     sptr, data_stack
    jne     .loop

    mov     rax, 60
    mov     rdi, 0
    syscall

fop_load:
    mov     eax, stk(1) ; Get the load-idx from the stack
    mov     ebx, dword [var_memory + rax * 4]
    mov     stk(1), ebx
    jmp     interpreter_loop

fop_store:
    mov     eax, stk(1) ; Get the store-idx from the stack
    mov     ebx, stk(2) ; Get the value to store from the stack
    mov     dword [var_memory + rax * 4], ebx
    sub     sptr, 8
    jmp     interpreter_loop

fop_pick:
    mov     eax, stk(1) ; Stack index to pick
    add     eax, 2
    imul    rax, 4
    mov     rcx, sptr
    sub     rcx, rax
    mov     ebx, dword [ecx]
    mov     stk(1), ebx
    jmp     interpreter_loop


fop_ifthen:
    mov     eax, stk(1)
    mov     ebx, valreg
    sub     sptr, 4
    cmp     eax, 0
    jne     .branch          
    pop     ictr
    push    rbx
.branch:
    jmp     interpreter_loop

%macro cmpop 2
fop_%1:
    mov     ebx, stk(1)
    mov     eax, stk(2)
    sub     sptr, 4
    cmp     eax, ebx     
    mov     stk(1), 0
    %2      .branch        
    mov     stk(1), 1
.branch:
    jmp     interpreter_loop
%endmacro

; inverse logic
cmpop beq , jne  ; branch-equal
cmpop ble , jg   ; branch-less-equal
cmpop bge , jl   ; branch-greater-equal
cmpop blt , jge  ; branch-less-than
cmpop bgt , jle  ; branch-greater-than
    
    
print_hex: ; rax = 32-bit integer to print
    mov     rdi, rax
    mov     rcx, 0
.loop:
    mov     rbx, rdi
    and     rbx, 0xf
    mov     dl, byte [hexlookup + rbx]
    mov     rbp, 7
    sub     rbp, rcx
    mov     byte [printbuf+rbp], dl
    shr     rdi, 4
    inc     rcx
    cmp     rcx, 8
    jl      .loop

    mov     byte [printbuf+8], 10
    ; sys_write 0, printbuf, 8
    mov     rax, 1
    mov     rdi, 1
    mov     rsi, printbuf
    mov     rdx, 9
    syscall
    ret

struc Loop
  .instr: resd 1        ; Loop jump target, (Forth instruction idx, to load into ictr)
  .ctr  : resd 1        ; Current counter
  .end  : resd 1        ; When the counter ends
  .size :
endstruc

struc Instruction
   .op    : resd 1     
   .val   : resd 1     
endstruc

_start:                                         ;tell linker entry point
    mov     r8,   data_stack                    ; for voltron
    mov     sptr, data_stack                    ; sptr == addr of top of data stack
    mov     lptr, loop_stack                    ; lptr == addr of top of loop stack
    push    0
interpreter_loop:
    pop     ictr                               ; ictr == current Forth instruction IDX in stream
    lea     rax, [bytecode+ictr*8]

    mov     opreg,  dword [rax + Instruction.op]
    mov     valreg, dword [rax + Instruction.val]

    inc     ictr                                      ; Increment instruction IDX
    push    ictr                                      ; Push next instruction id for popping later @ interpreter_loop
    
    mov     rax, [op_jumptable + opreg * 8]
    jmp     rax          

section     .bss

data_stack    resq 8
loop_stack    resq 8
var_memory    resd 64
printbuf      resb 32

%macro jmpop 1
    dq  fop_%1
%endmacro

section .data
op_jumptable:
jmpop pushint
jmpop callw
jmpop iadd
jmpop isub
jmpop imul
jmpop idiv
jmpop dup
jmpop swap
jmpop drop
jmpop over
jmpop do
jmpop loop
jmpop lctr
jmpop load
jmpop store
jmpop retw
jmpop eof
jmpop range
jmpop for
jmpop pick
jmpop ifthen
jmpop beq
jmpop ble
jmpop bge
jmpop blt
jmpop bgt

bytecode:
incbin  "bytecode"

hexlookup   db  '0123456789abcdef'
