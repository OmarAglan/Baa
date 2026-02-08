.section .rdata,"dr"
fmt_int: .asciz "%d\n"
fmt_str: .asciz "%s\n"
fmt_scan_int: .asciz "%d"

.data
حد: .quad 3

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
    leaq -8(%rbp), %r10
    movq $10, (%r10)
    leaq -16(%rbp), %r11
    movq $20, (%r11)
    leaq -24(%rbp), %rsi
    movq $0, (%rsi)
    movq (%r10), %rdi
    movq (%r11), %rbx
    cmpq %rbx, %rdi
    setl %dil
    movzbq %dil, %rdi
    testb %dil, %dil
    jne .LBB_1_1
    jmp .LBB_1_3
.LBB_1_1:
    movq (%r10), %rdi
    movq %rdi, %rbx
    addq $1, %rbx
    movq %rbx, (%r10)
    jmp .LBB_1_2
.LBB_1_2:
    jmp .LBB_1_4
.LBB_1_3:
    movq (%r10), %rdi
    movq %rdi, %rbx
    subq $1, %rbx
    movq %rbx, (%r10)
    jmp .LBB_1_2
.LBB_1_4:
    movq (%rsi), %rdi
    movq حد(%rip), %rbx
    cmpq %rbx, %rdi
    setl %dil
    movzbq %dil, %rdi
    testb %dil, %dil
    jne .LBB_1_5
    jmp .LBB_1_6
.LBB_1_5:
    movq (%rsi), %rdi
    movq %rdi, %rbx
    addq $1, %rbx
    movq %rbx, (%rsi)
    jmp .LBB_1_4
.LBB_1_6:
    leaq -32(%rbp), %rdi
    movq $0, (%rdi)
    jmp .LBB_1_7
.LBB_1_7:
    movq (%rdi), %rbx
    cmpq $3, %rbx
    setl %bl
    movzbq %bl, %rbx
    testb %bl, %bl
    jne .LBB_1_8
    jmp .LBB_1_10
.LBB_1_8:
    movq (%r10), %rbx
    movq (%rdi), %r12
    movq %rbx, %r13
    addq %r12, %r13
    movq %r13, (%r10)
    jmp .LBB_1_9
.LBB_1_9:
    movq (%rdi), %rbx
    movq %rbx, %r12
    addq $1, %r12
    movq %r12, (%rdi)
    jmp .LBB_1_7
.LBB_1_10:
    movq (%rsi), %rdi
    cmpq $0, %rdi
    sete %sil
    movzbq %sil, %rsi
    testb %sil, %sil
    jne .LBB_1_12
    jmp .LBB_1_15
.LBB_1_11:
    movq (%r10), %rsi
    movq (%r11), %rbx
    movq %rsi, %rcx
    movq %rbx, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call جمع_اثنين
    add $32, %rsp
    movq %rax, %rsi
    movq %rsi, (%r11)
    movq (%r11), %rsi
    leaq .Lstr_0(%rip), %rbx
    movq %rbx, %rcx
    movq %rsi, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    movq (%r11), %rsi
    movq %rsi, %rax
    mov -104(%rbp), %r13
    mov -96(%rbp), %r12
    mov -88(%rbp), %rdi
    mov -80(%rbp), %rsi
    mov -72(%rbp), %rbx
    leave
    ret
.LBB_1_12:
    movq (%r10), %r11
    movq %r11, %rsi
    addq $1, %rsi
    movq %rsi, (%r10)
    jmp .LBB_1_11
.LBB_1_13:
    movq (%r10), %r11
    movq %r11, %rsi
    addq $3, %rsi
    movq %rsi, (%r10)
    jmp .LBB_1_11
.LBB_1_14:
    movq (%r10), %r11
    movq %r11, %rsi
    addq $9, %rsi
    movq %rsi, (%r10)
    jmp .LBB_1_11
.LBB_1_15:
    cmpq $3, %rdi
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_1_13
    jmp .LBB_1_14

.section .rdata,"dr"
.Lstr_0: .asciz "%d\n"
