#include <windows.h>
#pragma comment(lib, "winmm.lib")
#pragma function(memset)
void* __cdecl memset(void* dest, int c, size_t count) { char* b = (char*)dest; while (count--) *b++ = (char)c; return dest; }
BYTE memory[1024 * 1024];
#define PHYS(seg, off) (((DWORD)(seg) << 4) + (WORD)(off))
typedef struct {
    union { WORD ax; struct { BYTE al, ah; }; };
    union { WORD cx; struct { BYTE cl, ch; }; };
    union { WORD dx; struct { BYTE dl, dh; }; };
    union { WORD bx; struct { BYTE bl, bh; }; };
    WORD sp, bp, si, di;
    WORD cs, ds, ss, es, ip;
    BYTE fZ, fS, fC, fD;
    BOOL running;
} CPU;
WORD* r16[8]; BYTE* r8[8]; WORD* sReg[4];
void InitCPU(CPU* cpu) {
    memset(cpu, 0, sizeof(CPU));
    r16[0] = &cpu->ax; r16[1] = &cpu->cx; r16[2] = &cpu->dx; r16[3] = &cpu->bx;
    r16[4] = &cpu->sp; r16[5] = &cpu->bp; r16[6] = &cpu->si; r16[7] = &cpu->di;
    r8[0] = &cpu->al; r8[1] = &cpu->cl; r8[2] = &cpu->dl; r8[3] = &cpu->bl;
    r8[4] = &cpu->ah; r8[5] = &cpu->ch; r8[6] = &cpu->dh; r8[7] = &cpu->bh;
    sReg[0] = &cpu->es; sReg[1] = &cpu->cs; sReg[2] = &cpu->ss; sReg[3] = &cpu->ds;
}
BYTE Fetch8(CPU* cpu) { return memory[PHYS(cpu->cs, cpu->ip++)]; }
WORD Fetch16(CPU* cpu) { WORD w = *(WORD*)&memory[PHYS(cpu->cs, cpu->ip)]; cpu->ip += 2; return w; }
void Push(CPU* cpu, WORD v) { cpu->sp -= 2; *(WORD*)&memory[PHYS(cpu->ss, cpu->sp)] = v; }
WORD Pop(CPU* cpu) { WORD v = *(WORD*)&memory[PHYS(cpu->ss, cpu->sp)]; cpu->sp += 2; return v; }
DWORD GetEA(CPU* cpu, BYTE mod, BYTE rm) {
    WORD offset = 0; WORD seg = cpu->ds;
    switch (rm) {
    case 0: offset = cpu->bx + cpu->si; break; case 1: offset = cpu->bx + cpu->di; break;
    case 2: offset = cpu->bp + cpu->si; seg = cpu->ss; break; case 3: offset = cpu->bp + cpu->di; seg = cpu->ss; break;
    case 4: offset = cpu->si; break; case 5: offset = cpu->di; break;
    case 6: offset = cpu->bp; seg = cpu->ss; break; case 7: offset = cpu->bx; break;
    }
    if (mod == 1) offset += (char)Fetch8(cpu); else if (mod == 2) offset += Fetch16(cpu);
    else if (mod == 0 && rm == 6) { offset = Fetch16(cpu); seg = cpu->ds; }
    return PHYS(seg, offset);
}
void DecodeModRM(CPU* cpu, BYTE modrm, int is16, void** rm_ptr, void** reg_ptr) {
    BYTE mod = (modrm >> 6) & 3; BYTE reg = (modrm >> 3) & 7; BYTE rm = modrm & 7;
    *reg_ptr = is16 ? (void*)r16[reg] : (void*)r8[reg];
    if (mod == 3) *rm_ptr = is16 ? (void*)r16[rm] : (void*)r8[rm];
    else *rm_ptr = (void*)&memory[GetEA(cpu, mod, rm)];
}
DWORD DoALU(CPU* cpu, int op, DWORD d, DWORD s, int is16) {
    DWORD res = 0;
    switch (op) {
    case 0: res = d + s; break; case 1: res = d | s; break; case 2: res = d + s + cpu->fC; break;
    case 3: res = d - s - cpu->fC; break; case 4: res = d & s; break;
    case 5: case 7: res = d - s; break; case 6: res = d ^ s; break;
    }
    cpu->fZ = ((res & (is16 ? 0xFFFF : 0xFF)) == 0); cpu->fS = ((res & (is16 ? 0x8000 : 0x80)) != 0); cpu->fC = (res > (is16 ? 0xFFFF : 0xFF));
    return res;
}
HWND g_hwnd = NULL;
BYTE keys[256] = { 0 };
int mouse_x = 0, mouse_y = 0, mouse_btn = 0;
DWORD vga_pal[16] = { 0x000000,0x0000AA,0x00AA00,0x00AAAA,0xAA0000,0xAA00AA,0xAA5500,0xAAAAAA,0x555555,0x5555FF,0x55FF55,0x55FFFF,0xFF5555,0xFF55FF,0xFFFF55,0xFFFFFF };
DWORD render_buf[64000];
BYTE wav_buf[8192 + 44];
void PlayTone(WORD freq) {
    if (freq == 0) {PlaySoundA(NULL, NULL, 0); return;}
    int sampleRate = 22050;
    int period = sampleRate / freq;
    if (period <= 0) period = 1;
    int samples = period * (8192 / period); 
    if (samples == 0) return;
    DWORD fileSize = 36 + samples;
    BYTE* p = wav_buf;
    *(DWORD*)p = 0x46464952; p += 4;
    *(DWORD*)p = fileSize; p += 4;
    *(DWORD*)p = 0x45564157; p += 4;
    *(DWORD*)p = 0x20746D66; p += 4;
    *(DWORD*)p = 16; p += 4;
    *(WORD*)p = 1; p += 2;
    *(WORD*)p = 1; p += 2;
    *(DWORD*)p = sampleRate; p += 4;
    *(DWORD*)p = sampleRate; p += 4;
    *(WORD*)p = 1; p += 2;
    *(WORD*)p = 8; p += 2;
    *(DWORD*)p = 0x61746164; p += 4;
    *(DWORD*)p = samples; p += 4;
    for (int i = 0; i < samples; i++) {
        p[i] = ((i % period) < (period / 2)) ? 192 : 64; 
    }
    PlaySoundA((LPCSTR)wav_buf, NULL, SND_MEMORY | SND_ASYNC | SND_LOOP);
}
LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) PostQuitMessage(0);
    else if (m == WM_KEYDOWN) keys[w & 0xFF] = 1;
    else if (m == WM_KEYUP) keys[w & 0xFF] = 0;
    else if (m == WM_MOUSEMOVE) {
        RECT r; GetClientRect(h, &r);
        if (r.right > 0 && r.bottom > 0) {
            mouse_x = (LOWORD(l) * 320) / r.right;  
            mouse_y = (HIWORD(l) * 200) / r.bottom;
            if (mouse_x < 0) mouse_x = 0; if (mouse_x > 319) mouse_x = 319;
            if (mouse_y < 0) mouse_y = 0; if (mouse_y > 199) mouse_y = 199;
        }
    }
    else if (m == WM_LBUTTONDOWN) mouse_btn |= 1;
    else if (m == WM_LBUTTONUP) mouse_btn &= ~1;
    else if (m == WM_RBUTTONDOWN) mouse_btn |= 2;
    else if (m == WM_RBUTTONUP) mouse_btn &= ~2;
    return DefWindowProc(h, m, w, l);
}
void SetupVGA() {
    if (g_hwnd) return;
    WNDCLASSA wc = { 0 }; wc.lpfnWndProc = WndProc; wc.hInstance = GetModuleHandle(NULL); wc.lpszClassName = "EmuVGA";
    RegisterClassA(&wc);
    g_hwnd = CreateWindowExA(0, "EmuVGA", "8086f Fantasy Console", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 640, 440, NULL, NULL, wc.hInstance, NULL);
}
void RunCPU(CPU* cpu) {
    cpu->running = TRUE; DWORD tick = GetTickCount();
    while (cpu->running) {
        if (g_hwnd) { MSG msg; while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { if (msg.message == WM_QUIT) cpu->running = FALSE; TranslateMessage(&msg); DispatchMessageA(&msg); } }
        for (int k = 0; k < 10000 && cpu->running; k++) {
            BYTE op = Fetch8(cpu);
            BOOL rep = FALSE; int rep_type = 0;
            if (op == 0xF3 || op == 0xF2) { rep = TRUE; rep_type = op; op = Fetch8(cpu); }
            int is16 = op & 1; void* pRM, * pReg;
            if (op < 0x40 && (op & 7) < 6) {
                int low3 = op & 7; int alu_op = (op >> 3) & 7;
                if (low3 < 4) {
                    BYTE modrm = Fetch8(cpu); DecodeModRM(cpu, modrm, is16, &pRM, &pReg);
                    DWORD d = is16 ? *(WORD*)pRM : *(BYTE*)pRM, s = is16 ? *(WORD*)pReg : *(BYTE*)pReg;
                    if (op & 2) { d = s; s = is16 ? *(WORD*)pRM : *(BYTE*)pRM; }
                    DWORD res = DoALU(cpu, alu_op, d, s, is16);
                    if (alu_op != 7) { if (op & 2) { if (is16) *(WORD*)pReg = (WORD)res; else *(BYTE*)pReg = (BYTE)res; } else { if (is16) *(WORD*)pRM = (WORD)res; else *(BYTE*)pRM = (BYTE)res; } }
                }
                else {
                    DWORD s = is16 ? Fetch16(cpu) : Fetch8(cpu), d = is16 ? cpu->ax : cpu->al;
                    DWORD res = DoALU(cpu, alu_op, d, s, is16);
                    if (alu_op != 7) { if (is16) cpu->ax = (WORD)res; else cpu->al = (BYTE)res; }
                }
            }
            else if (op >= 0x40 && op <= 0x4F) { int reg = op & 7; *r16[reg] = (WORD)DoALU(cpu, (op < 0x48) ? 0 : 5, *r16[reg], 1, 1); }
            else if (op >= 0x50 && op <= 0x57) Push(cpu, *r16[op & 7]);
            else if (op >= 0x58 && op <= 0x5F) *r16[op & 7] = Pop(cpu);
            else if ((op & 0xE7) == 0x06) Push(cpu, *sReg[(op >> 3) & 3]);
            else if ((op & 0xE7) == 0x07) *sReg[(op >> 3) & 3] = Pop(cpu);
            else if (op >= 0x70 && op <= 0x7F) {
                char offset = (char)Fetch8(cpu); BOOL jmp = FALSE;
                switch (op & 0x0F) {
                case 0x2: jmp = cpu->fC; break; case 0x3: jmp = !cpu->fC; break;
                case 0x4: jmp = cpu->fZ; break; case 0x5: jmp = !cpu->fZ; break;
                case 0x6: jmp = cpu->fC || cpu->fZ; break; case 0x7: jmp = !cpu->fC && !cpu->fZ; break;
                case 0x8: jmp = cpu->fS; break; case 0x9: jmp = !cpu->fS; break;
                case 0xC: jmp = cpu->fS; break; case 0xD: jmp = !cpu->fS || cpu->fZ; break;
                case 0xE: jmp = cpu->fS || cpu->fZ; break; case 0xF: jmp = !cpu->fS && !cpu->fZ; break;
                }
                if (jmp) cpu->ip += offset;
            }
            else if (op >= 0x80 && op <= 0x83) {
                BYTE modrm = Fetch8(cpu); int is_16_bit = (op == 0x81 || op == 0x83);
                DecodeModRM(cpu, modrm, is_16_bit, &pRM, &pReg);
                int alu_op = (modrm >> 3) & 7;
                DWORD s = (op == 0x81) ? Fetch16(cpu) : (char)Fetch8(cpu), d = is_16_bit ? *(WORD*)pRM : *(BYTE*)pRM;
                DWORD res = DoALU(cpu, alu_op, d, s, is_16_bit);
                if (alu_op != 7) { if (is_16_bit) *(WORD*)pRM = (WORD)res; else *(BYTE*)pRM = (BYTE)res; }
            }
            else if (op == 0x84 || op == 0x85) { BYTE modrm = Fetch8(cpu); DecodeModRM(cpu, modrm, is16, &pRM, &pReg); DoALU(cpu, 4, is16 ? *(WORD*)pRM : *(BYTE*)pRM, is16 ? *(WORD*)pReg : *(BYTE*)pReg, is16); }
            else if (op == 0xA8 || op == 0xA9) { DoALU(cpu, 4, is16 ? cpu->ax : cpu->al, is16 ? Fetch16(cpu) : Fetch8(cpu), is16); }
            else if (op >= 0x88 && op <= 0x8B) { BYTE modrm = Fetch8(cpu); DecodeModRM(cpu, modrm, is16, &pRM, &pReg); if (op & 2) { if (is16) *(WORD*)pReg = *(WORD*)pRM; else *(BYTE*)pReg = *(BYTE*)pRM; } else { if (is16) *(WORD*)pRM = *(WORD*)pReg; else *(BYTE*)pRM = *(BYTE*)pReg; } }
            else if (op == 0x8C || op == 0x8E) { BYTE modrm = Fetch8(cpu); DecodeModRM(cpu, modrm, 1, &pRM, &pReg); int sreg = (modrm >> 3) & 3; if (op == 0x8E) *sReg[sreg] = *(WORD*)pRM; else *(WORD*)pRM = *sReg[sreg]; }
            else if (op == 0x8D) {
                BYTE modrm = Fetch8(cpu); BYTE mod = (modrm >> 6) & 3; BYTE rm = modrm & 7;
                if (mod != 3) {
                    WORD off = 0;
                    switch (rm) {
                    case 0: off = cpu->bx + cpu->si; break; case 1: off = cpu->bx + cpu->di; break;
                    case 2: off = cpu->bp + cpu->si; break; case 3: off = cpu->bp + cpu->di; break;
                    case 4: off = cpu->si; break; case 5: off = cpu->di; break;
                    case 6: off = cpu->bp; break; case 7: off = cpu->bx; break;
                    }
                    if (mod == 1) off += (char)Fetch8(cpu); else if (mod == 2) off += Fetch16(cpu);
                    else if (mod == 0 && rm == 6) off = Fetch16(cpu);
                    *r16[(modrm >> 3) & 7] = off;
                }
            }
            else if (op >= 0x90 && op <= 0x97) { int reg = op & 7; WORD temp = cpu->ax; cpu->ax = *r16[reg]; *r16[reg] = temp; }
            else if (op == 0x98) cpu->ax = (short)((char)cpu->al);
            else if (op == 0x99) cpu->dx = (cpu->ax & 0x8000) ? 0xFFFF : 0x0000;
            else if (op == 0x9A) { WORD o = Fetch16(cpu), s = Fetch16(cpu); Push(cpu, cpu->cs); Push(cpu, cpu->ip); cpu->cs = s; cpu->ip = o; }
            else if (op == 0xEA) { WORD o = Fetch16(cpu), s = Fetch16(cpu); cpu->cs = s; cpu->ip = o; }
            else if (op == 0xCB) { cpu->ip = Pop(cpu); cpu->cs = Pop(cpu); }
            else if (op >= 0xA0 && op <= 0xA3) { WORD addr = Fetch16(cpu); if (op == 0xA0) cpu->al = memory[PHYS(cpu->ds, addr)]; else if (op == 0xA1) cpu->ax = *(WORD*)&memory[PHYS(cpu->ds, addr)]; else if (op == 0xA2) memory[PHYS(cpu->ds, addr)] = cpu->al; else if (op == 0xA3) *(WORD*)&memory[PHYS(cpu->ds, addr)] = cpu->ax; }
            else if (op >= 0xA4 && op <= 0xAD) {
                int step = is16 ? 2 : 1; if (cpu->fD) step = -step;
                do {
                    DWORD s = PHYS(cpu->ds, cpu->si), d = PHYS(cpu->es, cpu->di);
                    if (op <= 0xA5) { if (is16) *(WORD*)&memory[d] = *(WORD*)&memory[s]; else memory[d] = memory[s]; cpu->si += step; cpu->di += step; }
                    else if (op <= 0xA7) { DWORD d_v = is16 ? *(WORD*)&memory[d] : memory[d], s_v = is16 ? *(WORD*)&memory[s] : memory[s]; DoALU(cpu, 7, s_v, d_v, is16); cpu->si += step; cpu->di += step; }
                    else if (op <= 0xAB) { if (is16) *(WORD*)&memory[d] = cpu->ax; else memory[d] = cpu->al; cpu->di += step; }
                    else { if (is16) cpu->ax = *(WORD*)&memory[s]; else cpu->al = memory[s]; cpu->si += step; }
                    if (rep) { cpu->cx--; if (op >= 0xA6 && op <= 0xA7 && (!cpu->cx || !cpu->fZ)) rep = FALSE; }
                } while (rep && cpu->cx > 0);
            }
            else if (op == 0xAE || op == 0xAF) {
                int step = is16 ? 2 : 1; if (cpu->fD) step = -step;
                do {
                    DWORD d_v = is16 ? *(WORD*)&memory[PHYS(cpu->es, cpu->di)] : memory[PHYS(cpu->es, cpu->di)];
                    DWORD s_v = is16 ? cpu->ax : cpu->al;
                    DoALU(cpu, 7, s_v, d_v, is16); cpu->di += step;
                    if (rep) { cpu->cx--; if (!cpu->cx || (rep_type == 0xF3 && !cpu->fZ) || (rep_type == 0xF2 && cpu->fZ)) rep = FALSE; }
                } while (rep && cpu->cx > 0);
            }
            else if (op >= 0xB0 && op <= 0xB7) *r8[op & 7] = Fetch8(cpu);
            else if (op >= 0xB8 && op <= 0xBF) *r16[op & 7] = Fetch16(cpu);
            else if (op >= 0xD0 && op <= 0xD3) {
                BYTE modrm = Fetch8(cpu); DecodeModRM(cpu, modrm, is16, &pRM, &pReg);
                int sub = (modrm >> 3) & 7; int count = (op >= 0xD2) ? cpu->cl : 1;
                DWORD val = is16 ? *(WORD*)pRM : *(BYTE*)pRM; int bits = is16 ? 16 : 8;
                while (count--) {
                    if (sub == 0) { cpu->fC = (val >> (bits - 1)) & 1; val = (val << 1) | cpu->fC; }
                    else if (sub == 1) { cpu->fC = val & 1; val = (val >> 1) | (cpu->fC << (bits - 1)); }
                    else if (sub == 4) { cpu->fC = (val >> (bits - 1)) & 1; val <<= 1; }
                    else if (sub == 5) { cpu->fC = val & 1; val >>= 1; }
                    else if (sub == 7) { cpu->fC = val & 1; val = is16 ? ((short)val >> 1) : ((char)val >> 1); }
                    val &= (is16 ? 0xFFFF : 0xFF);
                }
                if (is16) *(WORD*)pRM = (WORD)val; else *(BYTE*)pRM = (BYTE)val;
            }
            else if (op >= 0xE0 && op <= 0xE3) {
                char off = (char)Fetch8(cpu); BOOL do_jmp = FALSE;
                if (op != 0xE3) cpu->cx--;
                if (op == 0xE0) do_jmp = (cpu->cx != 0 && !cpu->fZ); else if (op == 0xE1) do_jmp = (cpu->cx != 0 && cpu->fZ);
                else if (op == 0xE2) do_jmp = (cpu->cx != 0); else if (op == 0xE3) do_jmp = (cpu->cx == 0);
                if (do_jmp) cpu->ip += off;
            }
            else if (op == 0xE8) { WORD off = Fetch16(cpu); Push(cpu, cpu->ip); cpu->ip += off; }
            else if (op == 0xEB) cpu->ip += (char)Fetch8(cpu);
            else if (op == 0xE9) cpu->ip += Fetch16(cpu);
            else if (op == 0xC3) cpu->ip = Pop(cpu);
            else if (op == 0xF6 || op == 0xF7) {
                BYTE modrm = Fetch8(cpu); DecodeModRM(cpu, modrm, is16, &pRM, &pReg);
                int sub = (modrm >> 3) & 7; DWORD v = is16 ? *(WORD*)pRM : *(BYTE*)pRM;
                if (sub == 2) v = ~v;
                else if (sub == 3) { cpu->fC = (v != 0); v = (is16 ? 0 : 0) - v; }
                else if (sub == 4) { if (is16) { DWORD r = cpu->ax * v; cpu->ax = r & 0xFFFF; cpu->dx = r >> 16; } else { cpu->ax = cpu->al * v; } }
                else if (sub == 5) { if (is16) { int r = (short)cpu->ax * (short)v; cpu->ax = r & 0xFFFF; cpu->dx = r >> 16; } else { cpu->ax = (short)((char)cpu->al * (char)v); } }
                else if (sub == 6 && v != 0) { if (is16) { DWORD n = (cpu->dx << 16) | cpu->ax; cpu->ax = n / v; cpu->dx = n % v; } else { WORD n = cpu->ax; cpu->al = n / v; cpu->ah = n % v; } }
                else if (sub == 7 && v != 0) { if (is16) { int n = (int)((cpu->dx << 16) | cpu->ax); cpu->ax = n / (short)v; cpu->dx = n % (short)v; } else { short n = (short)cpu->ax; cpu->al = n / (char)v; cpu->ah = n % (char)v; } }
                if (sub < 4) { if (is16) *(WORD*)pRM = (WORD)v; else *(BYTE*)pRM = (BYTE)v; }
            }
            else if (op == 0xD4) { BYTE base = Fetch8(cpu); if (base == 0) cpu->running = FALSE; else { cpu->ah = cpu->al / base; cpu->al %= base; cpu->fZ = (cpu->ax == 0); cpu->fS = (cpu->ah & 0x80) != 0; } }
            else if (op == 0xD5) { BYTE base = Fetch8(cpu); cpu->al = (cpu->ah * base) + cpu->al; cpu->ah = 0; cpu->fZ = (cpu->al == 0); cpu->fS = (cpu->al & 0x80) != 0; }
            else if (op == 0x37) { if ((cpu->al & 0x0F) > 9 || cpu->fC) { cpu->al += 6; cpu->ah++; cpu->fC = 1; } else cpu->fC = 0; cpu->al &= 0x0F; }
            else if (op == 0x3F) { if ((cpu->al & 0x0F) > 9 || cpu->fC) { cpu->al -= 6; cpu->ah--; cpu->fC = 1; } else cpu->fC = 0; cpu->al &= 0x0F; }
            else if (op == 0xFE || op == 0xFF) {
                BYTE modrm = Fetch8(cpu); DecodeModRM(cpu, modrm, is16, &pRM, &pReg);
                int sub = (modrm >> 3) & 7; DWORD val = is16 ? *(WORD*)pRM : *(BYTE*)pRM;
                if (sub == 0 || sub == 1) { DWORD res = DoALU(cpu, (sub == 0) ? 0 : 5, val, 1, is16); if (is16) *(WORD*)pRM = (WORD)res; else *(BYTE*)pRM = (BYTE)res; }
                else if (sub == 2) { Push(cpu, cpu->ip); cpu->ip = (WORD)val; }
                else if (sub == 4) { cpu->ip = (WORD)val; }
                else if (sub == 6) { Push(cpu, (WORD)val); }
            }
            else if (op == 0x9C) { Push(cpu, (cpu->fZ ? 0x40 : 0) | (cpu->fS ? 0x80 : 0) | (cpu->fC ? 1 : 0) | (cpu->fD ? 0x400 : 0)); }
            else if (op == 0x9D) { WORD f = Pop(cpu); cpu->fZ = (f & 0x40) != 0; cpu->fS = (f & 0x80) != 0; cpu->fC = (f & 1) != 0; cpu->fD = (f & 0x400) != 0; }
            else if (op == 0xF8) cpu->fC = 0; else if (op == 0xF9) cpu->fC = 1; else if (op == 0xFC) cpu->fD = 0; else if (op == 0xFD) cpu->fD = 1;
            else if (op == 0xC6 || op == 0xC7) { BYTE modrm = Fetch8(cpu); DecodeModRM(cpu, modrm, is16, &pRM, &pReg); if (is16) *(WORD*)pRM = Fetch16(cpu); else *(BYTE*)pRM = Fetch8(cpu); }
            else if (op == 0xCD) {
                BYTE intNum = Fetch8(cpu);
                if (intNum == 0x10 && cpu->ah == 0x00 && cpu->al == 0x13) SetupVGA();
                else if (intNum == 0x15 && cpu->ah == 0x86) { Sleep(cpu->dx); k = 10000; }
                else if (intNum == 0x16 && cpu->ah == 0x10) { cpu->ah = keys[cpu->al] ? 1 : 0; }
                else if (intNum == 0x21) {
                    if (cpu->ah == 0x01) { DWORD r; ReadFile(GetStdHandle(STD_INPUT_HANDLE), &cpu->al, 1, &r, NULL); }
                    else if (cpu->ah == 0x02) { DWORD w; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), &cpu->dl, 1, &w, NULL); }
                    else if (cpu->ah == 0x09) { DWORD addr = PHYS(cpu->ds, cpu->dx), len = 0; while (memory[addr + len] != '$') len++; DWORD w; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), &memory[addr], len, &w, NULL); }
                    else if (cpu->ah == 0x4C) cpu->running = FALSE;
                }
                else if (intNum == 0x33) {
                    if (cpu->ax == 0x0000) { cpu->ax = 0xFFFF; }
                    else if (cpu->ax == 0x0003) {                
                        cpu->cx = mouse_x;                       
                        cpu->dx = mouse_y;                       
                        cpu->bx = mouse_btn;                     
                    }
                }
                else if (intNum == 0x34) {
                    if (cpu->ax == 0x0001) {PlayTone(cpu->bx);}
                    else if (cpu->ax == 0x0000) {PlayTone(0);}
                }
            }
            else if (op == 0xF4) { cpu->running = FALSE; }
            else if (op == 0xCC || op == 0xCE) {}
            else {
                HANDLE hCons = GetStdHandle(STD_OUTPUT_HANDLE); char err[128];
                wsprintfA(err, "\nCRASH: Unhandled Opcode %02X at %04X:%04X\n", op, cpu->cs, cpu->ip - 1);
                DWORD w; WriteFile(hCons, err, lstrlenA(err), &w, NULL);
                cpu->running = FALSE;
            }
        }
        if (g_hwnd && GetTickCount() - tick > 16) {
            for (int i = 0; i < 64000; i++) render_buf[i] = vga_pal[memory[0xA0000 + i] & 0x0F];
            BITMAPINFO bmi = { 0 }; bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = 320; bmi.bmiHeader.biHeight = -200; bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32; bmi.bmiHeader.biCompression = BI_RGB;
            RECT r; GetClientRect(g_hwnd, &r); HDC hdc = GetDC(g_hwnd);
            StretchDIBits(hdc, 0, 0, r.right, r.bottom, 0, 0, 320, 200, render_buf, &bmi, DIB_RGB_COLORS, SRCCOPY);
            ReleaseDC(g_hwnd, hdc); tick = GetTickCount();
        }
    }
}
void GetCmdLineFile(char* out) { char* c = GetCommandLineA(); BOOL q = FALSE; while (*c) { if (*c == '"') q = !q; else if (*c == ' ' && !q) { c++; break; } c++; } while (*c == ' ') c++; int i = 0; while (*c && *c != '\r' && *c != '\n') { if (*c != '"') out[i++] = *c; c++; } out[i] = 0; }
void __stdcall mainCRTStartup() {
    char file[MAX_PATH]; memset(file, 0, MAX_PATH); GetCmdLineFile(file);
    HANDLE h = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { DWORD w; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "File not found.\n", 16, &w, NULL); ExitProcess(1); }
    CPU cpu; InitCPU(&cpu); cpu.cs = cpu.ds = cpu.ss = 0x1000; cpu.ip = 0x0100; cpu.sp = 0xFFFE;
    DWORD read; ReadFile(h, &memory[PHYS(cpu.cs, cpu.ip)], GetFileSize(h, NULL), &read, NULL); CloseHandle(h);
    RunCPU(&cpu); ExitProcess(0);
}