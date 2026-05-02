cpu 8086
org 0x0100
calc_loop:
    mov dx, msg_sep
    mov ah, 09h
    int 21h
    mov dx, msg_A
    mov ah, 09h
    int 21h
    call read_num
    mov [val_A], ax
    mov dx, msg_op
    mov ah, 09h
    int 21h
    call read_op
    mov [op], al
    cmp al, 'q'
    je exit
    cmp al, 'Q'
    je exit
    cmp al, '~'
    je .do_calc
    mov dx, msg_B
    mov ah, 09h
    int 21h
    call read_num
    mov [val_B], ax
.do_calc:
    mov ax, [val_A]
    mov bx, [val_B]
    mov cl, [op]
    cmp cl, '+'
    je .add
    cmp cl, '-'
    je .sub
    cmp cl, '*'
    je .mul
    cmp cl, '/'
    je .div
    cmp cl, '&'
    je .and
    cmp cl, '|'
    je .or
    cmp cl, '^'
    je .xor
    cmp cl, '~'
    je .not
    mov dx, msg_err
    mov ah, 09h
    int 21h
    jmp calc_loop
.add: add ax, bx
      jmp .print
.sub: sub ax, bx
      jmp .print
.mul: mul bx
      jmp .print
.div: cmp bx, 0
      je .div_zero
      xor dx, dx
      div bx
      jmp .print
.and: and ax, bx
      jmp .print
.or:  or ax, bx
      jmp .print
.xor: xor ax, bx
      jmp .print
.not: not ax
      jmp .print
.div_zero:
    mov dx, msg_div0
    mov ah, 09h
    int 21h
    jmp calc_loop
.print:
    push ax
    mov dx, msg_res
    mov ah, 09h
    int 21h
    pop ax
    call print_num
    mov dx, msg_nl
    mov ah, 09h
    int 21h
    jmp calc_loop
exit:
    mov ah, 0x4C
    int 0x21
read_num:
    mov cx, 0
.get_char:
    mov ah, 01h
    int 21h
    cmp al, 13
    je .done
    cmp al, '0'
    jb .get_char
    cmp al, '9'
    ja .get_char
    sub al, '0'
    mov ah, 0
    push ax
    mov ax, cx
    mov bx, 10
    mul bx
    pop dx
    add ax, dx
    mov cx, ax
    jmp .get_char
.done:
    mov ax, cx
    ret
read_op:
    mov bl, 0
.get_char:
    mov ah, 01h
    int 21h
    cmp al, 13
    je .done
    cmp al, 10
    je .get_char
    cmp al, ' '
    je .get_char
    mov bl, al
    jmp .get_char
.done:
    mov al, bl
    ret
print_num:
    mov bx, 10
    mov cx, 0
.div_loop:
    xor dx, dx
    div bx
    push dx
    inc cx
    cmp ax, 0
    jne .div_loop
.print_loop:
    pop dx
    add dl, '0'
    mov ah, 02h
    int 21h
    loop .print_loop
    ret
msg_sep  db 13, 10, '----------------', 13, 10, '$'
msg_A    db 'A = $'
msg_B    db 'B = $'
msg_op   db 'Op (+ - * / & | ^ ~ Q=Quit) = $'
msg_res  db 'Result = $'
msg_div0 db 'Error: Division by zero$'
msg_err  db 'Error: Unknown Operator$'
msg_nl   db 13, 10, '$'

val_A    dw 0
val_B    dw 0
op       db 0