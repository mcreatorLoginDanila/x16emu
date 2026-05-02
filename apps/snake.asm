cpu 8086
org 0x0100
start:
    mov ah, 0x00
    mov al, 0x13
    int 0x10
    mov al, 15
    mov [snake_x], al
    mov al, 10
    mov [snake_y], al
    call spawn_apple
    call draw_all
game_loop:
    call read_input
    mov ax, [frame_counter]
    inc ax
    mov [frame_counter], ax
    cmp ax, strict word 5
    jl .wait_frame
    mov ax, 0
    mov [frame_counter], ax
    call move_snake
    call draw_all
.wait_frame:
    mov dx, 15
    mov ah, 0x86
    int 0x15
    mov ax, [rand_seed]
    add ax, 3
    mov [rand_seed], ax
    jmp game_loop
game_over:
exit:
    mov ah, 0x4C
    int 0x21
read_input:
    mov ah, 0x10
    mov al, 0x26
    int 0x16
    cmp ah, 1
    jne .chk_dn
    mov al, [last_dir_y]
    cmp al, 1
    je .chk_dn
    mov al, 0
    mov [dir_x], al
    mov al, 0xFF
    mov [dir_y], al
    ret
.chk_dn:
    mov ah, 0x10
    mov al, 0x28
    int 0x16
    cmp ah, 1
    jne .chk_lt
    mov al, [last_dir_y]
    cmp al, 0xFF
    je .chk_lt
    mov al, 0
    mov [dir_x], al
    mov al, 1
    mov [dir_y], al
    ret
.chk_lt:
    mov ah, 0x10
    mov al, 0x25
    int 0x16
    cmp ah, 1
    jne .chk_rt
    mov al, [last_dir_x]
    cmp al, 1
    je .chk_rt
    mov al, 0xFF
    mov [dir_x], al
    mov al, 0
    mov [dir_y], al
    ret
.chk_rt:
    mov ah, 0x10
    mov al, 0x27
    int 0x16
    cmp ah, 1
    jne .chk_esc
    mov al, [last_dir_x]
    cmp al, 0xFF
    je .chk_esc
    mov al, 1
    mov [dir_x], al
    mov al, 0
    mov [dir_y], al
    ret
.chk_esc:
    ; ESCAPE
    mov ah, 0x10
    mov al, 0x1B
    int 0x16
    cmp ah, 1
    je exit
    ret
move_snake:
    mov al, [dir_x]
    mov [last_dir_x], al
    mov al, [dir_y]
    mov [last_dir_y], al
    mov bl, [head_ptr]
    mov bh, 0
    mov al, [snake_x + bx]
    add al, [dir_x]
    mov dl, al
    mov al, [snake_y + bx]
    add al, [dir_y]
    mov dh, al
    cmp dl, 0
    jl game_over
    cmp dl, 31
    jg game_over
    cmp dh, 0
    jl game_over
    cmp dh, 19
    jg game_over
    mov cl, [tail_ptr]
.self_loop:
    cmp cl, [head_ptr]
    je .self_done
    mov bl, cl
    mov bh, 0
    mov al, [snake_x + bx]
    cmp al, dl
    jne .self_next
    mov al, [snake_y + bx]
    cmp al, dh
    je game_over
.self_next:
    inc cl
    jmp .self_loop
.self_done:
    cmp dl, [apple_x]
    jne .no_apple
    cmp dh, [apple_y]
    jne .no_apple
    call spawn_apple
    jmp .add_head
.no_apple:
    mov al, [tail_ptr]
    inc al
    mov [tail_ptr], al
.add_head:
    mov al, [head_ptr]
    inc al
    mov [head_ptr], al
    mov bl, al
    mov bh, 0
    mov [snake_x + bx], dl
    mov [snake_y + bx], dh
    ret
spawn_apple:
    mov ax, [rand_seed]
    add ax, 17
    and ax, strict word 0x7FFF
    mov [rand_seed], ax
    mov bx, ax
    and bx, strict word 31
    mov [apple_x], bl
    mov ax, [rand_seed]
    add ax, 11
    and ax, strict word 0x7FFF
    mov [rand_seed], ax
.mod_y:
    cmp ax, strict word 20
    jl .y_done
    sub ax, strict word 20
    jmp .mod_y
.y_done:
    mov [apple_y], al
    mov cl, [tail_ptr]
.chk_apple:
    mov bl, cl
    mov bh, 0
    mov al, [snake_x + bx]
    cmp al, [apple_x]
    jne .chk_next
    mov al, [snake_y + bx]
    cmp al, [apple_y]
    je spawn_apple
.chk_next:
    cmp cl, [head_ptr]
    je .apple_ok
    inc cl
    jmp .chk_apple
.apple_ok:
    ret
draw_all:
    mov ax, 0x2000
    mov es, ax
    mov di, 0
    mov cx, 64000
    mov al, 0
    rep stosb
    mov al, [apple_x]
    mov [rect_x], al
    mov al, [apple_y]
    mov [rect_y], al
    mov al, 12
    mov [rect_c], al
    call draw_rect
    mov cl, [tail_ptr]
.snake_loop:
    mov bl, cl
    mov bh, 0
    mov al, [snake_x + bx]
    mov [rect_x], al
    mov al, [snake_y + bx]
    mov [rect_y], al
    mov al, 10
    cmp cl, [head_ptr]
    jne .is_body
    mov al, 14
.is_body:
    mov [rect_c], al
    push cx
    call draw_rect
    pop cx
    cmp cl, [head_ptr]
    je .snake_done
    inc cl
    jmp .snake_loop
.snake_done:
    push ds
    mov ax, 0x2000
    mov ds, ax
    mov si, 0
    mov ax, 0xA000
    mov es, ax
    mov di, 0
    mov cx, 64000
    rep movsb
    pop ds
    ret
draw_rect:
    mov al, [rect_y]
    mov ah, 0
    mov bx, ax
    mov di, 0
.y_loop:
    cmp bx, strict word 0
    je .y_done
    add di, strict word 3200
    dec bx
    jmp .y_loop
.y_done:
    mov al, [rect_x]
    mov ah, 0
    mov bx, ax
    mov cx, 0
.x_loop:
    cmp bx, strict word 0
    je .x_done
    add cx, strict word 10
    dec bx
    jmp .x_loop
.x_done:
    add di, cx
    
    mov cx, 10
.row_loop:
    push cx
    push di
    mov cx, 10
    mov al, [rect_c]
    rep stosb
    pop di
    pop cx
    add di, strict word 320
    dec cx
    jnz .row_loop
    ret
dir_x db 1
dir_y db 0
last_dir_x db 1
last_dir_y db 0
apple_x db 0
apple_y db 0
head_ptr db 0
tail_ptr db 0
frame_counter dw 0
rand_seed dw 42
rect_x db 0
rect_y db 0
rect_c db 0
snake_x: times 256 db 0
snake_y: times 256 db 0