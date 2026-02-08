.section .rdata,"dr"
fmt_int: .asciz "%d\n"
fmt_str: .asciz "%s\n"
fmt_scan_int: .asciz "%d"

.data
حد: .quad 5

.text

.globl جمع_اثنين
جمع_اثنين:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -56(%rbp)
.LBB_0_0:
    leaq -8(%rbp), %rsi
    movq %r10, (%rsi)
    leaq -16(%rbp), %r10
    movq %r11, (%r10)
    movq (%rsi), %r11
    movq (%r10), %rsi
    movq %r11, %r10
    addq %rsi, %r10
    movq %r10, %rax
    mov -56(%rbp), %rsi
    leave
    ret

.globl main
main:
    push %rbp
    mov %rsp, %rbp
    sub $112, %rsp
    mov %rbx, -72(%rbp)
    mov %rsi, -80(%rbp)
    mov %rdi, -88(%rbp)
    mov %r12, -96(%rbp)
    mov %r13, -104(%rbp)
.LBB_1_0:
    leaq -8(%rbp), %rsi
    movq $10, (%rsi)
    leaq -16(%rbp), %rdi
    movq $20, (%rdi)
    leaq -24(%rbp), %r10
    movq $0, (%r10)
    movq (%rsi), %r11
    movq (%rdi), %rbx
    cmpq %rbx, %r11
    setl %r11b
    movzbq %r11b, %r11
    testb %r11b, %r11b
    jne .LBB_1_1
    jmp .LBB_1_3
.LBB_1_1:
    movq (%rsi), %r11
    movq %r11, %rbx
    addq $1, %rbx
    movq %rbx, (%rsi)
    jmp .LBB_1_2
.LBB_1_2:
    jmp .LBB_1_4
.LBB_1_3:
    movq (%rsi), %r11
    movq %r11, %rbx
    subq $1, %rbx
    movq %rbx, (%rsi)
    jmp .LBB_1_2
.LBB_1_4:
    movq (%r10), %r11
    movq حد(%rip), %rbx
    cmpq %rbx, %r11
    setl %r11b
    movzbq %r11b, %r11
    testb %r11b, %r11b
    jne .LBB_1_5
    jmp .LBB_1_6
.LBB_1_5:
    movq (%r10), %r11
    movq %r11, %rbx
    addq $1, %rbx
    movq %rbx, (%r10)
    jmp .LBB_1_4
.LBB_1_6:
    leaq -32(%rbp), %r11
    movq $0, (%r11)
    jmp .LBB_1_7
.LBB_1_7:
    movq (%r11), %rbx
    cmpq $3, %rbx
    setl %bl
    movzbq %bl, %rbx
    testb %bl, %bl
    jne .LBB_1_8
    jmp .LBB_1_10
.LBB_1_8:
    movq (%rsi), %rbx
    movq (%r11), %r12
    movq %rbx, %r13
    addq %r12, %r13
    movq %r13, (%rsi)
    jmp .LBB_1_9
.LBB_1_9:
    movq (%r11), %rbx
    movq %rbx, %r12
    addq $1, %r12
    movq %r12, (%r11)
    jmp .LBB_1_7
.LBB_1_10:
    movq (%r10), %rbx
    cmpq $0, %rbx
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_1_12
    jmp .LBB_1_15
.LBB_1_11:
    movq (%rsi), %r10
    movq (%rdi), %r11
    movq %r10, %rcx
    movq %r11, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call جمع_اثنين
    add $32, %rsp
    movq %rax, %r10
    movq %r10, (%rdi)
    movq (%rdi), %r10
    leaq .Lstr_0(%rip), %r11
    movq %r11, %rcx
    movq %r10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    movq (%rdi), %r10
    movq %r10, %rax
    mov -104(%rbp), %r13
    mov -96(%rbp), %r12
    mov -88(%rbp), %rdi
    mov -80(%rbp), %rsi
    mov -72(%rbp), %rbx
    leave
    ret
.LBB_1_12:
    movq (%rsi), %r10
    movq %r10, %r11
    addq $1, %r11
    movq %r11, (%rsi)
    jmp .LBB_1_11
.LBB_1_13:
    movq (%rsi), %r10
    movq %r10, %r11
    addq $3, %r11
    movq %r11, (%rsi)
    jmp .LBB_1_11
.LBB_1_14:
    movq (%rsi), %r10
    movq %r10, %r11
    addq $9, %r11
    movq %r11, (%rsi)
    jmp .LBB_1_11
.LBB_1_15:
    cmpq $3, %rbx
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_1_13
    jmp .LBB_1_14

.section .rdata,"dr"
.Lstr_0: .asciz "%d\n"
