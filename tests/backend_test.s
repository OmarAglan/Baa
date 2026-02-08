.section .rdata,"dr"
fmt_int: .asciz "%d\n"
fmt_str: .asciz "%s\n"
fmt_scan_int: .asciz "%d"

.data
عد: .quad 0
متغير_عام: .quad 0
الحد: .quad 10
المعامل: .quad 3

.text

.globl تحقق
تحقق:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -56(%rbp)
    mov %rdi, -64(%rbp)
    movq %rcx, %r10
    movq %rdx, %r11
.LBB_0_0:
    leaq -8(%rbp), %rsi
    movq %r10, (%rsi)
    leaq -16(%rbp), %r10
    movq %r11, (%r10)
    movq عد(%rip), %r11
    movq %r11, %rdi
    addq $1, %rdi
    movq %rdi, عد(%rip)
    movq (%rsi), %r11
    movq (%r10), %rsi
    cmpq %rsi, %r11
    setne %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_0_1
    jmp .LBB_0_2
.LBB_0_1:
    movq $1, %rax
    mov -64(%rbp), %rdi
    mov -56(%rbp), %rsi
    leave
    ret
.LBB_0_2:
    movq $0, %rax
    mov -64(%rbp), %rdi
    mov -56(%rbp), %rsi
    leave
    ret

.globl اختبار_جمع_طرح
اختبار_جمع_طرح:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -48(%rbp)
    mov %rdi, -56(%rbp)
.LBB_1_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    movq (%rsi), %rdi
    movq $42, %rcx
    movq $42, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $18, %rcx
    movq $18, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $0, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $0, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret

.globl اختبار_ضرب
اختبار_ضرب:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -48(%rbp)
    mov %rdi, -56(%rbp)
.LBB_2_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    movq (%rsi), %rdi
    movq $360, %rcx
    movq $360, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $0, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $7, %rcx
    movq $7, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $14, %rcx
    movq $14, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $20, %rcx
    movq $20, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret

.globl اختبار_قسمة
اختبار_قسمة:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -48(%rbp)
    mov %rdi, -56(%rbp)
.LBB_3_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    movq (%rsi), %rdi
    movq $2, %rcx
    movq $2, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $10, %rcx
    movq $10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $7, %rcx
    movq $7, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $0, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret

.globl اختبار_سالب
اختبار_سالب:
    push %rbp
    mov %rsp, %rbp
    sub $80, %rsp
    mov %rbx, -56(%rbp)
    mov %rsi, -64(%rbp)
    mov %rdi, -72(%rbp)
.LBB_4_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $5, %r10
    negq %r10
    movq %r10, (%rdi)
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq $5, %r11
    negq %r11
    movq %r10, %rcx
    movq %r11, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq %r10, %r11
    negq %r11
    movq %r11, %rcx
    movq $5, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $0, %r10
    negq %r10
    movq %r10, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -72(%rbp), %rdi
    mov -64(%rbp), %rsi
    mov -56(%rbp), %rbx
    leave
    ret

.globl اختبار_مقارنات
اختبار_مقارنات:
    push %rbp
    mov %rsp, %rbp
    sub $128, %rsp
    mov %rbx, -96(%rbp)
    mov %rsi, -104(%rbp)
    mov %rdi, -112(%rbp)
    mov %r12, -120(%rbp)
.LBB_5_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $10, (%rdi)
    leaq -24(%rbp), %rbx
    movq $20, (%rbx)
    leaq -32(%rbp), %r10
    movq $0, (%r10)
    movq (%rdi), %r11
    cmpq $10, %r11
    sete %r11b
    movzbq %r11b, %r11
    testb %r11b, %r11b
    jne .LBB_5_1
    jmp .LBB_5_2
.LBB_5_1:
    movq $1, (%r10)
    jmp .LBB_5_2
.LBB_5_2:
    movq (%rsi), %r12
    movq (%r10), %r11
    movq %r11, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %r12, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    leaq -40(%rbp), %r10
    movq $0, (%r10)
    movq (%rdi), %r11
    movq (%rbx), %r12
    cmpq %r12, %r11
    setne %r11b
    movzbq %r11b, %r11
    testb %r11b, %r11b
    jne .LBB_5_3
    jmp .LBB_5_4
.LBB_5_3:
    movq $1, (%r10)
    jmp .LBB_5_4
.LBB_5_4:
    movq (%rsi), %r12
    movq (%r10), %r11
    movq %r11, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %r12, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    leaq -48(%rbp), %r10
    movq $0, (%r10)
    movq (%rbx), %r11
    movq (%rdi), %r12
    cmpq %r12, %r11
    setg %r11b
    movzbq %r11b, %r11
    testb %r11b, %r11b
    jne .LBB_5_5
    jmp .LBB_5_6
.LBB_5_5:
    movq $1, (%r10)
    jmp .LBB_5_6
.LBB_5_6:
    movq (%rsi), %r12
    movq (%r10), %r11
    movq %r11, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %r12, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    leaq -56(%rbp), %r10
    movq $0, (%r10)
    movq (%rdi), %r11
    movq (%rbx), %rdi
    cmpq %rdi, %r11
    setl %r11b
    movzbq %r11b, %r11
    testb %r11b, %r11b
    jne .LBB_5_7
    jmp .LBB_5_8
.LBB_5_7:
    movq $1, (%r10)
    jmp .LBB_5_8
.LBB_5_8:
    movq (%rsi), %rdi
    movq (%r10), %r11
    movq %r11, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -120(%rbp), %r12
    mov -112(%rbp), %rdi
    mov -104(%rbp), %rsi
    mov -96(%rbp), %rbx
    leave
    ret

.globl اختبار_مقارنات٢
اختبار_مقارنات٢:
    push %rbp
    mov %rsp, %rbp
    sub $80, %rsp
    mov %rsi, -72(%rbp)
    mov %rdi, -80(%rbp)
.LBB_6_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %r10
    movq $0, (%r10)
    movq $1, %r11
    testq %r11, %r11
    jne .LBB_6_1
    jmp .LBB_6_2
.LBB_6_1:
    movq $1, (%r10)
    jmp .LBB_6_2
.LBB_6_2:
    movq (%rsi), %rdi
    movq (%r10), %r11
    movq %r11, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    leaq -24(%rbp), %r10
    movq $0, (%r10)
    movq $1, %r11
    testq %r11, %r11
    jne .LBB_6_3
    jmp .LBB_6_4
.LBB_6_3:
    movq $1, (%r10)
    jmp .LBB_6_4
.LBB_6_4:
    movq (%rsi), %rdi
    movq (%r10), %r11
    movq %r11, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    leaq -32(%rbp), %r10
    movq $0, (%r10)
    movq $0, %r11
    testq %r11, %r11
    jne .LBB_6_5
    jmp .LBB_6_6
.LBB_6_5:
    movq $1, (%r10)
    jmp .LBB_6_6
.LBB_6_6:
    movq (%rsi), %rdi
    movq (%r10), %r11
    movq %r11, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -80(%rbp), %rdi
    mov -72(%rbp), %rsi
    leave
    ret

.globl اختبار_متغيرات
اختبار_متغيرات:
    push %rbp
    mov %rsp, %rbp
    sub $80, %rsp
    mov %rbx, -56(%rbp)
    mov %rsi, -64(%rbp)
    mov %rdi, -72(%rbp)
.LBB_7_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $10, (%rdi)
    movq (%rdi), %r10
    movq %r10, %r11
    addq $5, %r11
    movq %r11, (%rdi)
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $15, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rdi), %r10
    movq %r10, %r11
    imulq $2, %r11
    movq %r11, (%rdi)
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $30, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -72(%rbp), %rdi
    mov -64(%rbp), %rsi
    mov -56(%rbp), %rbx
    leave
    ret

.globl اختبار_ثوابت
اختبار_ثوابت:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rbx, -48(%rbp)
    mov %rsi, -56(%rbp)
    mov %rdi, -64(%rbp)
.LBB_8_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    movq (%rsi), %rdi
    movq الحد(%rip), %r10
    movq %r10, %rcx
    movq $10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq المعامل(%rip), %r10
    movq %r10, %rcx
    movq $3, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq الحد(%rip), %r10
    movq المعامل(%rip), %r11
    movq %r10, %rbx
    imulq %r11, %rbx
    movq %rbx, %rcx
    movq $30, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -64(%rbp), %rdi
    mov -56(%rbp), %rsi
    mov -48(%rbp), %rbx
    leave
    ret

.globl اختبار_إذا
اختبار_إذا:
    push %rbp
    mov %rsp, %rbp
    sub $80, %rsp
    mov %rbx, -64(%rbp)
    mov %rsi, -72(%rbp)
    mov %rdi, -80(%rbp)
.LBB_9_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %r10
    movq $0, (%r10)
    movq $1, %r11
    testq %r11, %r11
    jne .LBB_9_1
    jmp .LBB_9_2
.LBB_9_1:
    movq $42, (%r10)
    jmp .LBB_9_2
.LBB_9_2:
    movq (%rsi), %rdi
    movq (%r10), %r11
    movq %r11, %rcx
    movq $42, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    leaq -24(%rbp), %rdi
    movq $10, (%rdi)
    movq (%rdi), %r10
    cmpq $5, %r10
    setg %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_9_3
    jmp .LBB_9_5
.LBB_9_3:
    movq $20, (%rdi)
    jmp .LBB_9_4
.LBB_9_4:
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $20, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -80(%rbp), %rdi
    mov -72(%rbp), %rsi
    mov -64(%rbp), %rbx
    leave
    ret
.LBB_9_5:
    movq $30, (%rdi)
    jmp .LBB_9_4

.globl اختبار_إذا_متعدد
اختبار_إذا_متعدد:
    push %rbp
    mov %rsp, %rbp
    sub $96, %rsp
    mov %rbx, -64(%rbp)
    mov %rsi, -72(%rbp)
    mov %rdi, -80(%rbp)
    mov %r12, -88(%rbp)
.LBB_10_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $85, (%rdi)
    leaq -24(%rbp), %rbx
    movq $0, (%rbx)
    movq (%rdi), %r10
    cmpq $90, %r10
    setge %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_10_1
    jmp .LBB_10_3
.LBB_10_1:
    movq $1, (%rbx)
    jmp .LBB_10_2
.LBB_10_2:
    movq (%rsi), %r12
    movq (%rbx), %r10
    movq %r10, %rcx
    movq $2, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %r12, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -88(%rbp), %r12
    mov -80(%rbp), %rdi
    mov -72(%rbp), %rsi
    mov -64(%rbp), %rbx
    leave
    ret
.LBB_10_3:
    movq (%rdi), %r10
    cmpq $80, %r10
    setge %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_10_4
    jmp .LBB_10_6
.LBB_10_4:
    movq $2, (%rbx)
    jmp .LBB_10_5
.LBB_10_5:
    jmp .LBB_10_2
.LBB_10_6:
    movq (%rdi), %r10
    cmpq $70, %r10
    setge %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_10_7
    jmp .LBB_10_9
.LBB_10_7:
    movq $3, (%rbx)
    jmp .LBB_10_8
.LBB_10_8:
    jmp .LBB_10_5
.LBB_10_9:
    movq $4, (%rbx)
    jmp .LBB_10_8

.globl اختبار_طالما_بسيط
اختبار_طالما_بسيط:
    push %rbp
    mov %rsp, %rbp
    sub $96, %rsp
    mov %rbx, -64(%rbp)
    mov %rsi, -72(%rbp)
    mov %rdi, -80(%rbp)
    mov %r12, -88(%rbp)
.LBB_11_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %r10
    movq $0, (%r10)
    leaq -24(%rbp), %r11
    movq $1, (%r11)
    jmp .LBB_11_1
.LBB_11_1:
    movq (%r11), %rdi
    cmpq $10, %rdi
    setle %dil
    movzbq %dil, %rdi
    testb %dil, %dil
    jne .LBB_11_2
    jmp .LBB_11_3
.LBB_11_2:
    movq (%r10), %rdi
    movq (%r11), %rbx
    movq %rdi, %r12
    addq %rbx, %r12
    movq %r12, (%r10)
    movq (%r11), %rdi
    movq %rdi, %rbx
    addq $1, %rbx
    movq %rbx, (%r11)
    jmp .LBB_11_1
.LBB_11_3:
    movq (%rsi), %rdi
    movq (%r10), %r11
    movq %r11, %rcx
    movq $55, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -88(%rbp), %r12
    mov -80(%rbp), %rdi
    mov -72(%rbp), %rsi
    mov -64(%rbp), %rbx
    leave
    ret

.globl اختبار_طالما_توقف
اختبار_طالما_توقف:
    push %rbp
    mov %rsp, %rbp
    sub $80, %rsp
    mov %rbx, -56(%rbp)
    mov %rsi, -64(%rbp)
    mov %rdi, -72(%rbp)
.LBB_12_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $0, (%rdi)
    jmp .LBB_12_1
.LBB_12_1:
    movq (%rdi), %r10
    cmpq $100, %r10
    setl %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_12_2
    jmp .LBB_12_3
.LBB_12_2:
    movq (%rdi), %r10
    cmpq $7, %r10
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_12_4
    jmp .LBB_12_5
.LBB_12_3:
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $7, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -72(%rbp), %rdi
    mov -64(%rbp), %rsi
    mov -56(%rbp), %rbx
    leave
    ret
.LBB_12_4:
    jmp .LBB_12_3
.LBB_12_5:
    movq (%rdi), %r10
    movq %r10, %r11
    addq $1, %r11
    movq %r11, (%rdi)
    jmp .LBB_12_1

.globl اختبار_لكل_بسيط
اختبار_لكل_بسيط:
    push %rbp
    mov %rsp, %rbp
    sub $96, %rsp
    mov %rbx, -64(%rbp)
    mov %rsi, -72(%rbp)
    mov %rdi, -80(%rbp)
    mov %r12, -88(%rbp)
.LBB_13_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %r10
    movq $0, (%r10)
    leaq -24(%rbp), %r11
    movq $0, (%r11)
    jmp .LBB_13_1
.LBB_13_1:
    movq (%r11), %rdi
    cmpq $5, %rdi
    setl %dil
    movzbq %dil, %rdi
    testb %dil, %dil
    jne .LBB_13_2
    jmp .LBB_13_4
.LBB_13_2:
    movq (%r10), %rdi
    movq (%r11), %rbx
    movq %rdi, %r12
    addq %rbx, %r12
    movq %r12, (%r10)
    jmp .LBB_13_3
.LBB_13_3:
    movq (%r11), %rdi
    movq %rdi, %rbx
    addq $1, %rbx
    movq %rbx, (%r11)
    jmp .LBB_13_1
.LBB_13_4:
    movq (%rsi), %rdi
    movq (%r10), %r11
    movq %r11, %rcx
    movq $10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -88(%rbp), %r12
    mov -80(%rbp), %rdi
    mov -72(%rbp), %rsi
    mov -64(%rbp), %rbx
    leave
    ret

.globl اختبار_لكل_توقف
اختبار_لكل_توقف:
    push %rbp
    mov %rsp, %rbp
    sub $96, %rsp
    mov %rbx, -64(%rbp)
    mov %rsi, -72(%rbp)
    mov %rdi, -80(%rbp)
    mov %r12, -88(%rbp)
.LBB_14_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $0, (%rdi)
    leaq -24(%rbp), %rbx
    movq $0, (%rbx)
    jmp .LBB_14_1
.LBB_14_1:
    movq (%rbx), %r10
    cmpq $100, %r10
    setl %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_14_2
    jmp .LBB_14_4
.LBB_14_2:
    movq (%rbx), %r10
    cmpq $5, %r10
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_14_5
    jmp .LBB_14_6
.LBB_14_3:
    movq (%rbx), %r10
    movq %r10, %r11
    addq $1, %r11
    movq %r11, (%rbx)
    jmp .LBB_14_1
.LBB_14_4:
    movq (%rsi), %r12
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $4, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %r12, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -88(%rbp), %r12
    mov -80(%rbp), %rdi
    mov -72(%rbp), %rsi
    mov -64(%rbp), %rbx
    leave
    ret
.LBB_14_5:
    jmp .LBB_14_4
.LBB_14_6:
    movq (%rbx), %r10
    movq %r10, (%rdi)
    jmp .LBB_14_3

.globl اختبار_اختر
اختبار_اختر:
    push %rbp
    mov %rsp, %rbp
    sub $96, %rsp
    mov %rbx, -64(%rbp)
    mov %rsi, -72(%rbp)
    mov %rdi, -80(%rbp)
    mov %r12, -88(%rbp)
.LBB_15_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $0, (%rdi)
    leaq -24(%rbp), %r10
    movq $2, (%r10)
    movq (%r10), %rbx
    cmpq $1, %rbx
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_15_2
    jmp .LBB_15_6
.LBB_15_1:
    movq (%rsi), %r12
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $20, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %r12, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -88(%rbp), %r12
    mov -80(%rbp), %rdi
    mov -72(%rbp), %rsi
    mov -64(%rbp), %rbx
    leave
    ret
.LBB_15_2:
    movq $10, (%rdi)
    jmp .LBB_15_1
.LBB_15_3:
    movq $20, (%rdi)
    jmp .LBB_15_1
.LBB_15_4:
    movq $30, (%rdi)
    jmp .LBB_15_1
.LBB_15_5:
    movq $1, %r10
    negq %r10
    movq %r10, (%rdi)
    jmp .LBB_15_1
.LBB_15_6:
    cmpq $2, %rbx
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_15_3
    jmp .LBB_15_7
.LBB_15_7:
    cmpq $3, %rbx
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_15_4
    jmp .LBB_15_5

.globl اختبار_اختر_افتراضي
اختبار_اختر_افتراضي:
    push %rbp
    mov %rsp, %rbp
    sub $80, %rsp
    mov %rbx, -64(%rbp)
    mov %rsi, -72(%rbp)
    mov %rdi, -80(%rbp)
.LBB_16_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $0, (%rdi)
    leaq -24(%rbp), %r10
    movq $99, (%r10)
    movq (%r10), %r11
    cmpq $1, %r11
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_16_2
    jmp .LBB_16_3
.LBB_16_1:
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq $1, %r11
    negq %r11
    movq %r10, %rcx
    movq %r11, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -80(%rbp), %rdi
    mov -72(%rbp), %rsi
    mov -64(%rbp), %rbx
    leave
    ret
.LBB_16_2:
    movq $10, (%rdi)
    jmp .LBB_16_1
.LBB_16_3:
    movq $1, %r10
    negq %r10
    movq %r10, (%rdi)
    jmp .LBB_16_1

.globl ثابت_خمسة
ثابت_خمسة:
    push %rbp
    mov %rsp, %rbp
    sub $32, %rsp
.LBB_17_0:
    movq $5, %rax
    leave
    ret

.globl مربع
مربع:
    push %rbp
    mov %rsp, %rbp
    sub $48, %rsp
    mov %rsi, -48(%rbp)
    movq %rcx, %r10
.LBB_18_0:
    leaq -8(%rbp), %r11
    movq %r10, (%r11)
    movq (%r11), %r10
    movq (%r11), %rsi
    movq %r10, %r11
    imulq %rsi, %r11
    movq %r11, %rax
    mov -48(%rbp), %rsi
    leave
    ret

.globl جمع
جمع:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -56(%rbp)
    movq %rcx, %r10
    movq %rdx, %r11
.LBB_19_0:
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

.globl مجموع_ثلاث
مجموع_ثلاث:
    push %rbp
    mov %rsp, %rbp
    sub $80, %rsp
    mov %rsi, -64(%rbp)
    mov %rdi, -72(%rbp)
    movq %rcx, %r10
    movq %rdx, %r11
    movq %r8, %rsi
.LBB_20_0:
    leaq -8(%rbp), %rdi
    movq %r10, (%rdi)
    leaq -16(%rbp), %r10
    movq %r11, (%r10)
    leaq -24(%rbp), %r11
    movq %rsi, (%r11)
    movq (%rdi), %rsi
    movq (%r10), %rdi
    movq %rsi, %r10
    addq %rdi, %r10
    movq (%r11), %rsi
    movq %r10, %r11
    addq %rsi, %r11
    movq %r11, %rax
    mov -72(%rbp), %rdi
    mov -64(%rbp), %rsi
    leave
    ret

.globl مجموع_أربع
مجموع_أربع:
    push %rbp
    mov %rsp, %rbp
    sub $96, %rsp
    mov %rbx, -72(%rbp)
    mov %rsi, -80(%rbp)
    mov %rdi, -88(%rbp)
    movq %rcx, %r10
    movq %rdx, %r11
    movq %r8, %rsi
    movq %r9, %rdi
.LBB_21_0:
    leaq -8(%rbp), %rbx
    movq %r10, (%rbx)
    leaq -16(%rbp), %r10
    movq %r11, (%r10)
    leaq -24(%rbp), %r11
    movq %rsi, (%r11)
    leaq -32(%rbp), %rsi
    movq %rdi, (%rsi)
    movq (%rbx), %rdi
    movq (%r10), %rbx
    movq %rdi, %r10
    addq %rbx, %r10
    movq (%r11), %rdi
    movq %r10, %r11
    addq %rdi, %r11
    movq (%rsi), %r10
    movq %r11, %rsi
    addq %r10, %rsi
    movq %rsi, %rax
    mov -88(%rbp), %rdi
    mov -80(%rbp), %rsi
    mov -72(%rbp), %rbx
    leave
    ret

.globl اختبار_دوال
اختبار_دوال:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -48(%rbp)
    mov %rdi, -56(%rbp)
.LBB_22_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call ثابت_خمسة
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $5, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $7, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call مربع
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $49, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $3, %rcx
    movq $4, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call جمع
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $7, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $1, %rcx
    movq $2, %rdx
    movq $3, %r8
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call مجموع_ثلاث
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $6, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $1, %rcx
    movq $2, %rdx
    movq $3, %r8
    movq $4, %r9
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call مجموع_أربع
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret

.globl مضروب
مضروب:
    push %rbp
    mov %rsp, %rbp
    sub $48, %rsp
    mov %rsi, -48(%rbp)
    movq %rcx, %r10
.LBB_23_0:
    leaq -8(%rbp), %r11
    movq %r10, (%r11)
    movq (%r11), %r10
    cmpq $1, %r10
    setle %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_23_1
    jmp .LBB_23_2
.LBB_23_1:
    movq $1, %rax
    mov -48(%rbp), %rsi
    leave
    ret
.LBB_23_2:
    movq (%r11), %rsi
    movq (%r11), %r10
    movq %r10, %r11
    subq $1, %r11
    movq %r11, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call مضروب
    add $32, %rsp
    movq %rax, %r10
    movq %rsi, %r11
    imulq %r10, %r11
    movq %r11, %rax
    mov -48(%rbp), %rsi
    leave
    ret

.globl اختبار_مضروب
اختبار_مضروب:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -48(%rbp)
    mov %rdi, -56(%rbp)
.LBB_24_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    movq (%rsi), %rdi
    movq $0, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call مضروب
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $1, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call مضروب
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $5, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call مضروب
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $120, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret

.globl فيبوناتشي
فيبوناتشي:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -48(%rbp)
    mov %rdi, -56(%rbp)
    movq %rcx, %r10
.LBB_25_0:
    leaq -8(%rbp), %rsi
    movq %r10, (%rsi)
    movq (%rsi), %r10
    cmpq $0, %r10
    setle %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_25_1
    jmp .LBB_25_2
.LBB_25_1:
    movq $0, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret
.LBB_25_2:
    movq (%rsi), %r10
    cmpq $1, %r10
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_25_3
    jmp .LBB_25_4
.LBB_25_3:
    movq $1, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret
.LBB_25_4:
    movq (%rsi), %r10
    movq %r10, %r11
    subq $1, %r11
    movq %r11, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call فيبوناتشي
    add $32, %rsp
    movq %rax, %rdi
    movq (%rsi), %r10
    movq %r10, %r11
    subq $2, %r11
    movq %r11, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call فيبوناتشي
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret

.globl اختبار_فيبوناتشي
اختبار_فيبوناتشي:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -48(%rbp)
    mov %rdi, -56(%rbp)
.LBB_26_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    movq (%rsi), %rdi
    movq $0, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call فيبوناتشي
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $1, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call فيبوناتشي
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $7, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call فيبوناتشي
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $13, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret

.globl اختبار_استدعاء_متداخل
اختبار_استدعاء_متداخل:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rbx, -48(%rbp)
    mov %rsi, -56(%rbp)
    mov %rdi, -64(%rbp)
.LBB_27_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    movq (%rsi), %rdi
    movq $3, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call مربع
    add $32, %rsp
    movq %rax, %rbx
    movq $4, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call مربع
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %rcx
    movq %r10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call جمع
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $25, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $2, %rcx
    movq $3, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call جمع
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call مربع
    add $32, %rsp
    movq %rax, %r10
    movq %r10, %rcx
    movq $25, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -64(%rbp), %rdi
    mov -56(%rbp), %rsi
    mov -48(%rbp), %rbx
    leave
    ret

.globl اختبار_و
اختبار_و:
    push %rbp
    mov %rsp, %rbp
    sub $80, %rsp
    mov %rbx, -56(%rbp)
    mov %rsi, -64(%rbp)
    mov %rdi, -72(%rbp)
.LBB_28_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $0, (%rdi)
    movq $1, %r10
    andq $1, %r10
    testb %r10b, %r10b
    jne .LBB_28_1
    jmp .LBB_28_3
.LBB_28_1:
    movq $1, (%rdi)
    jmp .LBB_28_2
.LBB_28_2:
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq $1, %r10
    andq $0, %r10
    testb %r10b, %r10b
    jne .LBB_28_4
    jmp .LBB_28_6
.LBB_28_3:
    movq $0, (%rdi)
    jmp .LBB_28_2
.LBB_28_4:
    movq $1, (%rdi)
    jmp .LBB_28_5
.LBB_28_5:
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -72(%rbp), %rdi
    mov -64(%rbp), %rsi
    mov -56(%rbp), %rbx
    leave
    ret
.LBB_28_6:
    movq $0, (%rdi)
    jmp .LBB_28_5

.globl اختبار_أو
اختبار_أو:
    push %rbp
    mov %rsp, %rbp
    sub $80, %rsp
    mov %rbx, -56(%rbp)
    mov %rsi, -64(%rbp)
    mov %rdi, -72(%rbp)
.LBB_29_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $0, (%rdi)
    movq $0, %r10
    orq $1, %r10
    testb %r10b, %r10b
    jne .LBB_29_1
    jmp .LBB_29_3
.LBB_29_1:
    movq $1, (%rdi)
    jmp .LBB_29_2
.LBB_29_2:
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq $0, %r10
    orq $0, %r10
    testb %r10b, %r10b
    jne .LBB_29_4
    jmp .LBB_29_6
.LBB_29_3:
    movq $0, (%rdi)
    jmp .LBB_29_2
.LBB_29_4:
    movq $1, (%rdi)
    jmp .LBB_29_5
.LBB_29_5:
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -72(%rbp), %rdi
    mov -64(%rbp), %rsi
    mov -56(%rbp), %rbx
    leave
    ret
.LBB_29_6:
    movq $0, (%rdi)
    jmp .LBB_29_5

.globl اختبار_مقارنة_مركبة
اختبار_مقارنة_مركبة:
    push %rbp
    mov %rsp, %rbp
    sub $80, %rsp
    mov %rbx, -64(%rbp)
    mov %rsi, -72(%rbp)
    mov %rdi, -80(%rbp)
.LBB_30_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %r10
    movq $5, (%r10)
    leaq -24(%rbp), %rdi
    movq $0, (%rdi)
    movq (%r10), %r11
    cmpq $0, %r11
    setg %r11b
    movzbq %r11b, %r11
    movq (%r10), %rbx
    cmpq $10, %rbx
    setl %r10b
    movzbq %r10b, %r10
    movq %r11, %rbx
    andq %r10, %rbx
    testb %bl, %bl
    jne .LBB_30_1
    jmp .LBB_30_3
.LBB_30_1:
    movq $1, (%rdi)
    jmp .LBB_30_2
.LBB_30_2:
    movq (%rsi), %rbx
    movq (%rdi), %r10
    movq %r10, %rcx
    movq $1, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rbx, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -80(%rbp), %rdi
    mov -72(%rbp), %rsi
    mov -64(%rbp), %rbx
    leave
    ret
.LBB_30_3:
    movq $0, (%rdi)
    jmp .LBB_30_2

.globl اختبار_طباعة
اختبار_طباعة:
    push %rbp
    mov %rsp, %rbp
    sub $48, %rsp
.LBB_31_0:
    leaq .Lstr_0(%rip), %r10
    movq %r10, %rcx
    movq $42, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    leaq .Lstr_2(%rip), %r10
    movq %r10, %rcx
    leaq .Lstr_1(%rip), %r10
    movq %r10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    leaq -8(%rbp), %r10
    movq $123, (%r10)
    movq (%r10), %r11
    leaq .Lstr_0(%rip), %r10
    movq %r10, %rcx
    movq %r11, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    movq $0, %rax
    leave
    ret

.globl اختبار_متعدد
اختبار_متعدد:
    push %rbp
    mov %rsp, %rbp
    sub $96, %rsp
    mov %rbx, -64(%rbp)
    mov %rsi, -72(%rbp)
    mov %rdi, -80(%rbp)
    mov %r12, -88(%rbp)
.LBB_32_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq -16(%rbp), %rdi
    movq $50, (%rdi)
    leaq -24(%rbp), %rbx
    movq $0, (%rbx)
    movq (%rdi), %r10
    cmpq $90, %r10
    setge %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_32_1
    jmp .LBB_32_3
.LBB_32_1:
    movq $1, (%rbx)
    jmp .LBB_32_2
.LBB_32_2:
    movq (%rsi), %r12
    movq (%rbx), %r10
    movq %r10, %rcx
    movq $3, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %r12, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -88(%rbp), %r12
    mov -80(%rbp), %rdi
    mov -72(%rbp), %rsi
    mov -64(%rbp), %rbx
    leave
    ret
.LBB_32_3:
    movq (%rdi), %r10
    cmpq $70, %r10
    setge %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_32_4
    jmp .LBB_32_6
.LBB_32_4:
    movq $2, (%rbx)
    jmp .LBB_32_5
.LBB_32_5:
    jmp .LBB_32_2
.LBB_32_6:
    movq (%rdi), %r10
    cmpq $50, %r10
    setge %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_32_7
    jmp .LBB_32_9
.LBB_32_7:
    movq $3, (%rbx)
    jmp .LBB_32_8
.LBB_32_8:
    jmp .LBB_32_5
.LBB_32_9:
    movq $4, (%rbx)
    jmp .LBB_32_8

.globl اختبار_حدود
اختبار_حدود:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -48(%rbp)
    mov %rdi, -56(%rbp)
.LBB_33_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    movq (%rsi), %rdi
    movq $0, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $1, %r10
    negq %r10
    movq %r10, %r11
    addq $1, %r11
    movq %r11, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    movq $0, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret

.globl زد_المتغير
زد_المتغير:
    push %rbp
    mov %rsp, %rbp
    sub $48, %rsp
    mov %rsi, -48(%rbp)
    movq %rcx, %r10
.LBB_34_0:
    leaq -8(%rbp), %r11
    movq %r10, (%r11)
    movq متغير_عام(%rip), %r10
    movq (%r11), %rsi
    movq %r10, %r11
    addq %rsi, %r11
    movq %r11, متغير_عام(%rip)
    movq متغير_عام(%rip), %r10
    movq %r10, %rax
    mov -48(%rbp), %rsi
    leave
    ret

.globl اختبار_عامة
اختبار_عامة:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -48(%rbp)
    mov %rdi, -56(%rbp)
.LBB_35_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    movq (%rsi), %rdi
    movq متغير_عام(%rip), %r10
    movq %r10, %rcx
    movq $0, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq $10, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call زد_المتغير
    add $32, %rsp
    movq %rax, %r10
    movq (%rsi), %rdi
    movq متغير_عام(%rip), %r10
    movq %r10, %rcx
    movq $10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq $5, %rcx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call زد_المتغير
    add $32, %rsp
    movq %rax, %r10
    movq (%rsi), %rdi
    movq متغير_عام(%rip), %r10
    movq %r10, %rcx
    movq $15, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq $100, متغير_عام(%rip)
    movq (%rsi), %rdi
    movq متغير_عام(%rip), %r10
    movq %r10, %rcx
    movq $100, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call تحقق
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %r10
    movq %r10, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret

.globl main
main:
    push %rbp
    mov %rsp, %rbp
    sub $64, %rsp
    mov %rsi, -48(%rbp)
    mov %rdi, -56(%rbp)
.LBB_36_0:
    leaq -8(%rbp), %rsi
    movq $0, (%rsi)
    leaq .Lstr_2(%rip), %r10
    movq %r10, %rcx
    leaq .Lstr_3(%rip), %r10
    movq %r10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_جمع_طرح
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_ضرب
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_قسمة
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_سالب
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_مقارنات
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_مقارنات٢
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_متغيرات
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_ثوابت
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_إذا
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_إذا_متعدد
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_طالما_بسيط
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_طالما_توقف
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_لكل_بسيط
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_لكل_توقف
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_اختر
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_اختر_افتراضي
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_دوال
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_مضروب
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_فيبوناتشي
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_استدعاء_متداخل
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_و
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_أو
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_مقارنة_مركبة
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_طباعة
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_متعدد
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_حدود
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    movq (%rsi), %rdi
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call اختبار_عامة
    add $32, %rsp
    movq %rax, %r10
    movq %rdi, %r11
    addq %r10, %r11
    movq %r11, (%rsi)
    leaq .Lstr_2(%rip), %r10
    movq %r10, %rcx
    leaq .Lstr_4(%rip), %r10
    movq %r10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    leaq .Lstr_2(%rip), %r10
    movq %r10, %rcx
    leaq .Lstr_5(%rip), %r10
    movq %r10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    movq عد(%rip), %r10
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
    leaq .Lstr_2(%rip), %r10
    movq %r10, %rcx
    leaq .Lstr_6(%rip), %r10
    movq %r10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    movq (%rsi), %r10
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
    movq (%rsi), %r10
    cmpq $0, %r10
    sete %r10b
    movzbq %r10b, %r10
    testb %r10b, %r10b
    jne .LBB_36_1
    jmp .LBB_36_3
.LBB_36_1:
    leaq .Lstr_2(%rip), %r10
    movq %r10, %rcx
    leaq .Lstr_7(%rip), %r10
    movq %r10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    jmp .LBB_36_2
.LBB_36_2:
    movq (%rsi), %r10
    movq %r10, %rax
    mov -56(%rbp), %rdi
    mov -48(%rbp), %rsi
    leave
    ret
.LBB_36_3:
    leaq .Lstr_2(%rip), %r10
    movq %r10, %rcx
    leaq .Lstr_8(%rip), %r10
    movq %r10, %rdx
    sub $32, %rsp
    movq %rcx, 0(%rsp)
    movq %rdx, 8(%rsp)
    movq %r8, 16(%rsp)
    movq %r9, 24(%rsp)
    call printf
    add $32, %rsp
    jmp .LBB_36_2

.section .rdata,"dr"
.Lstr_8: .asciz "FAIL"
.Lstr_7: .asciz "PASS"
.Lstr_6: .asciz "عدد الأخطاء:"
.Lstr_5: .asciz "عدد الاختبارات:"
.Lstr_4: .asciz "=== النتائج ==="
.Lstr_3: .asciz "=== اختبار الخلفية الشامل ==="
.Lstr_2: .asciz "%s\n"
.Lstr_1: .asciz "مرحبا"
.Lstr_0: .asciz "%d\n"
