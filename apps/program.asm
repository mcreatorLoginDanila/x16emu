org 0x0100
start:
    mov dx, msg
    mov ah, 09h
    int 21h
    mov ah, 4Ch
    int 21h
msg db 'Hello, I am a real 16-bit program loaded from a file!', 0x0D, 0x0A, '$'