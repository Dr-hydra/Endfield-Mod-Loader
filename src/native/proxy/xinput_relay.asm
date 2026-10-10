; Forward every Win64 argument and the original caller's stack unchanged.
; Undocumented XInput ordinals use this relay without guessing their layouts.
EXTERN EMLProxyTarget:PROC
.code
RELAY MACRO symbol, slot
symbol PROC FRAME
    sub rsp, 0A8h
    .allocstack 0A8h
    .endprolog
    mov [rsp+20h], rcx
    mov [rsp+28h], rdx
    mov [rsp+30h], r8
    mov [rsp+38h], r9
    movdqu [rsp+40h], xmm0
    movdqu [rsp+50h], xmm1
    movdqu [rsp+60h], xmm2
    movdqu [rsp+70h], xmm3
    mov [rsp+80h], rax
    mov ecx, slot
    call EMLProxyTarget
    mov r11, rax
    mov rcx, [rsp+20h]
    mov rdx, [rsp+28h]
    mov r8, [rsp+30h]
    mov r9, [rsp+38h]
    movdqu xmm0, [rsp+40h]
    movdqu xmm1, [rsp+50h]
    movdqu xmm2, [rsp+60h]
    movdqu xmm3, [rsp+70h]
    mov rax, [rsp+80h]
    add rsp, 0A8h
    jmp r11
symbol ENDP
ENDM
RELAY EMLRelay0, 0
RELAY EMLRelay1, 1
RELAY EMLRelay2, 2
RELAY EMLRelay3, 3
RELAY EMLRelay4, 4
RELAY EMLRelay5, 5
RELAY EMLRelay6, 6
RELAY EMLRelay7, 7
RELAY EMLRelay8, 8
RELAY EMLRelay9, 9
RELAY EMLRelay10, 10
RELAY EMLRelay11, 11
RELAY EMLRelay12, 12
RELAY EMLRelay13, 13
RELAY EMLRelay14, 14
END
