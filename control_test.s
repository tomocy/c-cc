.intel_syntax noprefix
.file 2 "test/adapter.h"
.file 1 "test/control_test.c"
.local .L2
.data
.type .L2, @object
.size .L2, 92
.align 16
.L2:
  .byte 40
  .byte 123
  .byte 32
  .byte 105
  .byte 110
  .byte 116
  .byte 32
  .byte 105
  .byte 32
  .byte 61
  .byte 32
  .byte 48
  .byte 59
  .byte 32
  .byte 115
  .byte 119
  .byte 105
  .byte 116
  .byte 99
  .byte 104
  .byte 32
  .byte 40
  .byte 55
  .byte 41
  .byte 32
  .byte 123
  .byte 32
  .byte 99
  .byte 97
  .byte 115
  .byte 101
  .byte 32
  .byte 48
  .byte 32
  .byte 46
  .byte 46
  .byte 46
  .byte 32
  .byte 53
  .byte 58
  .byte 32
  .byte 105
  .byte 32
  .byte 61
  .byte 32
  .byte 49
  .byte 59
  .byte 32
  .byte 98
  .byte 114
  .byte 101
  .byte 97
  .byte 107
  .byte 59
  .byte 32
  .byte 99
  .byte 97
  .byte 115
  .byte 101
  .byte 32
  .byte 54
  .byte 32
  .byte 46
  .byte 46
  .byte 46
  .byte 32
  .byte 50
  .byte 48
  .byte 58
  .byte 32
  .byte 105
  .byte 32
  .byte 61
  .byte 32
  .byte 50
  .byte 59
  .byte 32
  .byte 98
  .byte 114
  .byte 101
  .byte 97
  .byte 107
  .byte 59
  .byte 32
  .byte 125
  .byte 32
  .byte 105
  .byte 59
  .byte 32
  .byte 125
  .byte 41
  .byte 0
.local .L1
.data
.type .L1, @object
.size .L1, 5
.align 1
.L1:
  .byte 109
  .byte 97
  .byte 105
  .byte 110
  .byte 0
.local .L0
.data
.type .L0, @object
.size .L0, 5
.align 1
.L0:
  .byte 109
  .byte 97
  .byte 105
  .byte 110
  .byte 0
.global main
.text
.type main, @function
main:
  push rbp
  mov rbp, rsp
  sub rsp, 16
  mov [rbp-8], rsp
  .loc 1 15
  .loc 2 1
  .loc 2 1
  .loc 1 567
  .loc 1 567
  .loc 1 568
  .loc 1 568
  .loc 1 568
  .loc 1 568
  .loc 1 568
  .loc 1 568
  mov rcx, 4
  lea rdi, [rbp-12]
  mov al, 0
  rep stosb
  .loc 1 568
  lea rax, [rbp-12]
  push rax
  .loc 1 568
  .loc 1 568
  mov rax, 0
  pop rdi
  mov [rdi], eax
  .loc 1 569
  .loc 1 569
  mov rax, 7
  cmp eax, 20
  je (null)
  cmp eax, 19
  je (null)
  cmp eax, 18
  je (null)
  cmp eax, 17
  je (null)
  cmp eax, 16
  je (null)
  cmp eax, 15
  je (null)
  cmp eax, 14
  je (null)
  cmp eax, 13
  je (null)
  cmp eax, 12
  je (null)
  cmp eax, 11
  je (null)
  cmp eax, 10
  je (null)
  cmp eax, 9
  je (null)
  cmp eax, 8
  je (null)
  cmp eax, 7
  je (null)
  cmp eax, 6
  je (null)
  cmp eax, 5
  je (null)
  cmp eax, 4
  je (null)
  cmp eax, 3
  je (null)
  cmp eax, 2
  je (null)
  cmp eax, 1
  je (null)
  cmp eax, 0
  je (null)
  jmp .L3
  .loc 1 569
  .loc 1 570
(null):
  .loc 1 571
  .loc 1 571
  lea rax, [rbp-12]
  push rax
  .loc 1 571
  .loc 1 571
  mov rax, 1
  pop rdi
  mov [rdi], eax
  .loc 1 572
  jmp .L3
  .loc 1 572
  .loc 1 573
(null):
  .loc 1 574
  .loc 1 574
  lea rax, [rbp-12]
  push rax
  .loc 1 574
  .loc 1 574
  mov rax, 2
  pop rdi
  mov [rdi], eax
  .loc 1 575
  jmp .L3
  .loc 1 575
.L3:
  .loc 1 577
  .loc 1 577
  lea rax, [rbp-12]
  movsx rax, DWORD PTR [rax]
  push rax
  .loc 1 567
  .loc 1 567
  mov rax, 2
  push rax
  .loc 1 567
  .loc 1 567
  lea rax, .L2[rip]
  push rax
  .loc 2 1
  mov rax, assert@GOTPCREL[rip]
  pop rdi
  pop rsi
  pop rdx
  mov r10, rax
  mov rax, 0
  call r10
  add rsp, 0
  .loc 1 604
  .loc 1 604
  .loc 1 604
  mov rax, ok@GOTPCREL[rip]
  mov r10, rax
  mov rax, 0
  call r10
  add rsp, 0
  mov rax, 0
.Lreturn0:
  mov rsp, rbp
  pop rbp
  ret
