cpu 8086
org 0x0100
start:
    mov ah, 0x00
    mov al, 0x13
    int 0x10
spawn_piece:
    mov ax, [next_shape]
    mov [curr_shape], ax
    mov ax, 0
    mov [curr_rot], ax
    mov ax, 3
    mov [curr_x], ax
    mov ax, 0
    mov [curr_y], ax
    call rand_7
    mov [next_shape], ax
    call check_collision
    cmp ax, 1
    jne game_loop
    jmp exit
game_loop:
    call draw_all
    mov al, 0x1B
    mov ah, 0x10
    int 0x16
    cmp ah, 1
    je exit
    mov al, 0x26
    mov ah, 0x10
    int 0x16
    cmp ah, 1
    jne .up_released
    mov ax, [prev_up]
    cmp ax, 1
    je .check_left
    mov ax, 1
    mov [prev_up], ax
    jmp rotate
.up_released:
    mov ax, 0
    mov [prev_up], ax
.check_left:
    mov al, 0x25
    mov ah, 0x10
    int 0x16
    cmp ah, 1
    je .left_pressed
    mov ax, 0
    mov [tmr_left], ax
    jmp .check_right
.left_pressed:
    mov ax, [tmr_left]
    inc ax
    mov [tmr_left], ax
    cmp ax, 1
    je move_left
    cmp ax, 10
    jl .check_right
    mov ax, 8
    mov [tmr_left], ax
    jmp move_left
.check_right:
    mov al, 0x27
    mov ah, 0x10
    int 0x16
    cmp ah, 1
    je .right_pressed
    mov ax, 0
    mov [tmr_right], ax 
    jmp .check_down
.right_pressed:
    mov ax, [tmr_right]
    inc ax
    mov [tmr_right], ax
    cmp ax, 1           
    je move_right
    cmp ax, 10          
    jl .check_down
    mov ax, 8           
    mov [tmr_right], ax
    jmp move_right
.check_down:
    mov al, 0x28
    mov ah, 0x10
    int 0x16
    cmp ah, 1
    je drop
    jmp handle_timer
move_left:
    mov ax, [curr_x]
    dec ax
    mov [curr_x], ax
    call check_collision
    cmp ax, 1
    jne move_done
    mov ax, [curr_x]
    inc ax             
    mov [curr_x], ax
    jmp move_done
move_right:
    mov ax, [curr_x]
    inc ax
    mov [curr_x], ax
    call check_collision
    cmp ax, 1
    jne move_done
    mov ax, [curr_x]
    dec ax             
    mov [curr_x], ax
    jmp move_done
rotate:
    mov cx, [curr_rot]
    push cx
    mov ax, cx
    inc ax
    cmp ax, 4          
    jl rot_test
    mov ax, 0
rot_test:
    mov [curr_rot], ax
    call check_collision
    cmp ax, 1
    jne rot_apply
    pop cx             
    mov [curr_rot], cx
    jmp move_done
rot_apply:
    pop cx
    jmp move_done
drop:
    mov ax, [curr_y]
    inc ax
    mov [curr_y], ax
    call check_collision
    cmp ax, 1
    jne move_done
    mov ax, [curr_y]
    dec ax
    mov [curr_y], ax
    jmp lock_piece
move_done:
    jmp handle_timer
handle_timer:
    mov ax, [timer]
    inc ax
    mov [timer], ax
    cmp ax, 15         
    jl wait_frame
    mov ax, 0
    mov [timer], ax
    mov ax, [curr_y]
    inc ax
    mov [curr_y], ax
    call check_collision
    cmp ax, 1
    jne wait_frame
    mov ax, [curr_y]
    dec ax
    mov [curr_y], ax
    jmp lock_piece
wait_frame:
    mov dx, 15         
    mov ah, 0x86
    int 0x15
    mov ax, [rand_seed]
    add ax, 3
    mov [rand_seed], ax
    jmp game_loop
lock_piece:
    call place_piece
    call clear_lines
    jmp spawn_piece
exit:
    mov ah, 0x4C
    int 0x21
check_collision:
    mov cx, 0
.col_loop:
    call get_block_abs_xy
    cmp ax, 0
    jl .blocked      
    cmp ax, 9
    jg .blocked      
    cmp bx, 19
    jg .blocked      
    cmp bx, 0
    jl .next_blk
    push ax
    mov ax, bx
    call mul_10
    pop bx
    add ax, bx
    mov bx, board
    add bx, ax
    mov al, [bx]
    cmp al, 0
    jne .blocked
.next_blk:
    inc cx
    cmp cx, 4
    jl .col_loop
    mov ax, 0        
    ret
.blocked:
    mov ax, 1        
    ret
get_block_abs_xy:
    push cx
    mov ax, [curr_shape]
    call mul_32
    mov bx, ax
    mov ax, [curr_rot]
    call mul_8
    add bx, ax
    mov ax, cx
    add ax, ax
    add bx, ax
    add bx, shapes
    mov al, [bx]
    mov ah, 0
    mov cx, [curr_x]
    add ax, cx
    push ax
    inc bx
    mov al, [bx]
    mov ah, 0
    mov cx, [curr_y]
    add ax, cx
    mov bx, ax
    pop ax
    pop cx
    ret
mul_32:
    add ax, ax
    add ax, ax
    add ax, ax
    add ax, ax
    add ax, ax
    ret
mul_8:
    add ax, ax
    add ax, ax
    add ax, ax
    ret
mul_10:
    push cx
    push bx
    mov cx, ax
    mov ax, 0
.l:
    cmp cx, 0
    je .d
    add ax, 10
    dec cx
    jmp .l
.d:
    pop bx
    pop cx
    ret
rand_7:
    mov ax, [rand_seed]
    add ax, 17
    and ax, strict word 0x7FFF  
    mov [rand_seed], ax
.mod:
    cmp ax, 7
    jl .done
    sub ax, 7
    jmp .mod
.done:
    ret
place_piece:
    mov cx, 0
.l:
    call get_block_abs_xy
    cmp bx, 0
    jl .next
    push cx
    push ax
    mov ax, bx
    call mul_10
    pop bx
    add ax, bx
    mov bx, board
    add bx, ax
    mov ax, [curr_shape]
    inc ax           
    mov [bx], al
    pop cx
.next:
    inc cx
    cmp cx, 4
    jl .l
    ret
clear_lines:
    mov ax, 19
    mov [check_y], ax
.check_row:
    mov bx, [check_y]
    cmp bx, 0
    jl .done
    mov cx, 0
    mov ax, 1
    mov [row_full], ax
.col_loop:
    mov ax, [check_y]
    call mul_10
    add ax, cx
    mov bx, board
    add bx, ax
    mov al, [bx]
    cmp al, 0
    jne .next_col
    mov ax, 0
    mov [row_full], ax 
.next_col:
    inc cx
    cmp cx, 10
    jl .col_loop
    mov ax, [row_full]
    cmp ax, 1
    jne .row_not_full
    call shift_down    
    jmp .check_row
.row_not_full:
    mov ax, [check_y]
    dec ax
    mov [check_y], ax
    jmp .check_row
.done:
    ret
shift_down:
    mov cx, [check_y]
.rloop:
    cmp cx, 0
    je .r_zero
    mov dx, 0
.cloop:
    mov ax, cx
    dec ax
    call mul_10
    add ax, dx
    mov bx, board
    add bx, ax
    mov al, [bx]
    push ax
    mov ax, cx
    call mul_10
    add ax, dx
    mov bx, board
    add bx, ax
    pop ax
    mov [bx], al
    inc dx
    cmp dx, 10
    jl .cloop
    dec cx
    jmp .rloop
.r_zero:
    mov dx, 0
.zloop:
    mov bx, board
    add bx, dx
    mov al, 0
    mov [bx], al
    inc dx
    cmp dx, 10
    jl .zloop
    ret
draw_all:
    mov ax, 0x2000
    mov es, ax
    mov di, 0
    mov cx, 64000
    mov al, 0
    rep stosb
    mov ax, 0
    mov [draw_y], ax
.yloop:
    mov ax, 0
    mov [draw_x], ax
.xloop:
    mov ax, [draw_y]
    call mul_10
    mov bx, [draw_x]
    add ax, bx
    mov bx, board
    add bx, ax
    mov al, [bx]
    cmp al, 0
    je .skip
    add al, 8        
    mov [rect_c], al
    mov ax, [draw_x]
    call mul_10
    add ax, 110
    mov [rect_x], ax
    mov ax, [draw_y]
    call mul_10
    mov [rect_y], ax
    call draw_rect
.skip:
    mov ax, [draw_x]
    inc ax
    mov [draw_x], ax
    cmp ax, 10
    jl .xloop
    mov ax, [draw_y]
    inc ax
    mov [draw_y], ax
    cmp ax, 20
    jl .yloop
    mov cx, 0
.ploop:
    push cx
    call get_block_abs_xy
    cmp bx, 0
    jl .pskip
    push ax
    mov ax, [curr_shape]
    add ax, 9        
    mov [rect_c], al
    pop ax
    call mul_10
    add ax, 110
    mov [rect_x], ax
    mov ax, bx
    call mul_10
    mov [rect_y], ax
    call draw_rect
.pskip:
    pop cx
    inc cx
    cmp cx, 4
    jl .ploop
    mov di, 109
    mov cx, 200
.l1:
    push cx
    mov cx, 1
    mov al, 7        
    rep stosb
    add di, 319
    pop cx
    dec cx
    jnz .l1
    mov di, 210
    mov cx, 200
.l2:
    push cx
    mov cx, 1
    mov al, 7
    rep stosb
    add di, 319
    pop cx
    dec cx
    jnz .l2
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
    mov ax, [rect_y]
    push bx
    push cx
    mov cx, ax
    mov di, 0
.y1:
    cmp cx, 0
    je .y2
    add di, 320
    dec cx
    jmp .y1
.y2:
    mov ax, [rect_x]
    add di, ax
    mov cx, 10
.row:
    push cx
    push di
    mov cx, 10
    mov al, [rect_c]
    rep stosb
    pop di
    pop cx
    add di, 320
    dec cx
    jnz .row
    pop cx
    pop bx
    ret
rand_seed dw 42
curr_x dw 0
curr_y dw 0
curr_shape dw 0
curr_rot dw 0
next_shape dw 0
timer dw 0
check_y dw 0
row_full dw 0
draw_x dw 0
draw_y dw 0
rect_x dw 0
rect_y dw 0
rect_c db 0
prev_up dw 0
tmr_left dw 0
tmr_right dw 0
board: times 200 db 0
shapes:
db 0,1, 1,1, 2,1, 3,1
db 2,0, 2,1, 2,2, 2,3
db 0,2, 1,2, 2,2, 3,2
db 1,0, 1,1, 1,2, 1,3
db 0,0, 0,1, 1,1, 2,1
db 1,0, 2,0, 1,1, 1,2
db 0,1, 1,1, 2,1, 2,2
db 1,0, 1,1, 0,2, 1,2
db 2,0, 0,1, 1,1, 2,1
db 1,0, 1,1, 1,2, 2,2
db 0,1, 1,1, 2,1, 0,2
db 0,0, 1,0, 1,1, 1,2
db 1,0, 2,0, 1,1, 2,1
db 1,0, 2,0, 1,1, 2,1
db 1,0, 2,0, 1,1, 2,1
db 1,0, 2,0, 1,1, 2,1
db 1,0, 2,0, 0,1, 1,1
db 1,0, 1,1, 2,1, 2,2
db 1,1, 2,1, 0,2, 1,2
db 0,0, 0,1, 1,1, 1,2
db 1,0, 0,1, 1,1, 2,1
db 1,0, 1,1, 2,1, 1,2
db 0,1, 1,1, 2,1, 1,2
db 1,0, 0,1, 1,1, 1,2
db 0,0, 1,0, 1,1, 2,1
db 2,0, 1,1, 2,1, 1,2
db 0,1, 1,1, 1,2, 2,2
db 1,0, 0,1, 1,1, 0,2