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
    sub $272, %rsp
    mov %rcx, -8(%rbp)
    mov %rdx, -16(%rbp)
    mov -16(%rbp), %rax
    push %rax
    mov -8(%rbp), %rax
    pop %rbx
    add %rbx, %rax
    leave
    ret
    mov $0, %rax
    leave
    ret
.globl main
main:
    push %rbp
    mov %rsp, %rbp
    sub $272, %rsp
    mov $10, %rax
    mov %rax, -8(%rbp)
    mov $20, %rax
    mov %rax, -16(%rbp)
    mov $0, %rax
    mov %rax, -24(%rbp)
    mov -16(%rbp), %rax
    push %rax
    mov -8(%rbp), %rax
    pop %rbx
    cmp %rbx, %rax
    setl %al
    movzbq %al, %rax
    cmp $0, %rax
    je .Lelse_0
    mov $1, %rax
    push %rax
    mov -8(%rbp), %rax
    pop %rbx
    add %rbx, %rax
    mov %rax, -8(%rbp)
    jmp .Lend_1
.Lelse_0:
    mov $1, %rax
    push %rax
    mov -8(%rbp), %rax
    pop %rbx
    sub %rbx, %rax
    mov %rax, -8(%rbp)
.Lend_1:
.Lcontinue_2:
.Lstart_2:
    mov حد(%rip), %rax
    push %rax
    mov -24(%rbp), %rax
    pop %rbx
    cmp %rbx, %rax
    setl %al
    movzbq %al, %rax
    cmp $0, %rax
    je .Lend_3
    mov $1, %rax
    push %rax
    mov -24(%rbp), %rax
    pop %rbx
    add %rbx, %rax
    mov %rax, -24(%rbp)
    jmp .Lstart_2
.Lend_3:
    mov $0, %rax
    mov %rax, -32(%rbp)
.Lstart_4:
    mov $3, %rax
    push %rax
    mov -32(%rbp), %rax
    pop %rbx
    cmp %rbx, %rax
    setl %al
    movzbq %al, %rax
    cmp $0, %rax
    je .Lend_5
    mov -32(%rbp), %rax
    push %rax
    mov -8(%rbp), %rax
    pop %rbx
    add %rbx, %rax
    mov %rax, -8(%rbp)
.Lcontinue_6:
    mov $1, %rax
    push %rax
    mov -32(%rbp), %rax
    pop %rbx
    add %rbx, %rax
    mov %rax, -32(%rbp)
    jmp .Lstart_4
.Lend_5:
    mov -24(%rbp), %rax
    cmp $0, %rax
    je .Lcase_8
    cmp $3, %rax
    je .Lcase_9
    jmp .Lcase_10
.Lcase_8:
    mov $1, %rax
    push %rax
    mov -8(%rbp), %rax
    pop %rbx
    add %rbx, %rax
    mov %rax, -8(%rbp)
    jmp .Lend_7
.Lcase_9:
    mov $3, %rax
    push %rax
    mov -8(%rbp), %rax
    pop %rbx
    add %rbx, %rax
    mov %rax, -8(%rbp)
    jmp .Lend_7
.Lcase_10:
    mov $9, %rax
    push %rax
    mov -8(%rbp), %rax
    pop %rbx
    add %rbx, %rax
    mov %rax, -8(%rbp)
    jmp .Lend_7
.Lend_7:
    mov -8(%rbp), %rax
    push %rax
    mov -16(%rbp), %rax
    push %rax
    pop %rdx
    pop %rcx
    sub $32, %rsp
    call جمع_اثنين
    add $32, %rsp
    mov %rax, -16(%rbp)
    mov -16(%rbp), %rax
    mov %rax, %rdx
    lea fmt_int(%rip), %rcx
    sub $32, %rsp
    call printf
    add $32, %rsp
    mov -16(%rbp), %rax
    leave
    ret
    mov $0, %rax
    leave
    ret

.section .rdata,"dr"
