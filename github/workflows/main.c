/* Editor de atributos para PES 6 (PAL) e Winning Eleven 10 - PS2 (.elf)
 * Abre pela aba "Apps" do OPL (ou uLaunchELF). Le o save do MEMORY CARD fisico (mc0/mc1).
 * ATENCAO: faca backup do memory card antes de salvar qualquer coisa. */
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <libpad.h>
#include <libmc.h>
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core.h"

#define COLS 76
#define ROWS 26

static char padBuf[256] __attribute__((aligned(64)));
static uint8_t optbuf[LEN_PES6 + 64] __attribute__((aligned(64)));
static uint8_t outbuf[LEN_PES6 + 64] __attribute__((aligned(64)));
static uint8_t vrfbuf[LEN_PES6 + 64] __attribute__((aligned(64)));
static sceMcTblGetDir mctable[64] __attribute__((aligned(64)));

static Opt opt;
static int g_port = 0, g_slot = 0;
static char g_path[96];
static char g_msg[COLS + 1];

/* ---------------- tela de texto (sem depender de scr_clear) ---------------- */
static char screen[ROWS][COLS + 1];
static void cls(void){ for (int i = 0; i < ROWS; i++){ memset(screen[i], ' ', COLS); screen[i][COLS] = 0; } }
static void put(int row, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void put(int row, const char *fmt, ...){
    char tmp[200];
    va_list ap; va_start(ap, fmt); vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (row < 0 || row >= ROWS) return;
    int n = (int)strlen(tmp); if (n > COLS) n = COLS;
    memcpy(screen[row], tmp, n);
}
static void flip(void){
    scr_setXY(0, 0);
    for (int i = 0; i < ROWS; i++) scr_printf("%s\n", screen[i]);
}

/* ---------------- joystick ---------------- */
static u32 pad_poll(void){
    struct padButtonStatus b;
    int st = padGetState(0, 0);
    if (st != PAD_STATE_STABLE && st != PAD_STATE_FINDCTP1) return 0;
    if (!padRead(0, 0, &b)) return 0;
    return 0xffff ^ b.btns;
}
static u32 wait_buttons(void){           /* devolve so botoes recem-apertados, com repeticao */
    static u32 prev = 0; static int hold = 0;
    for (;;){
        u32 cur = pad_poll();
        u32 down = cur & ~prev;
        if (cur && cur == prev){ if (++hold > 18 && (hold % 3) == 0) down = cur & (PAD_UP|PAD_DOWN|PAD_LEFT|PAD_RIGHT|PAD_L1|PAD_R1); }
        else hold = 0;
        prev = cur;
        if (down) return down;
        for (volatile int i = 0; i < 200000; i++) ;
    }
}

/* ---------------- memory card ---------------- */
static int mc_wait(void){ int cmd, res; mcSync(0, &cmd, &res); return res; }

static int ends_with(const char *s, const char *suf){ int a = (int)strlen(s), b = (int)strlen(suf); return a >= b && !strcmp(s + a - b, suf); }
static int starts_with(const char *s, const char *pre){ return !strncmp(s, pre, strlen(pre)); }
static int is_opt_folder(const char *n){
    if (starts_with(n, "BISLPM-66374WE10OPT")) return 1;       /* Winning Eleven 10 (JP) */
    if (starts_with(n, "BASLUS-21464W2K7OPT")) return 1;       /* WE 2007 (US) */
    if (starts_with(n, "BESLES-") && ends_with(n, "PES6OPT")) return 1; /* PES 6 PAL */
    return 0;
}

static int find_save(void){
    for (int port = 0; port < 2; port++){
        int type = 0, freec = 0, fmt = 0, r = 0;
        for (int t = 0; t < 4; t++){ mcGetInfo(port, 0, &type, &freec, &fmt); r = mc_wait(); if (r >= -1) break; }
        if (r < -1 || type != 2) continue;                      /* type 2 = PS2 memory card */
        mcGetDir(port, 0, "/*", 0, 64, mctable);
        int n = mc_wait();
        for (int i = 0; i < n; i++){
            const char *nm = (const char *)mctable[i].EntryName;
            if (is_opt_folder(nm)){
                g_port = port; g_slot = 0;
                snprintf(g_path, sizeof g_path, "/%s/%s", nm, nm);
                return 1;
            }
        }
    }
    return 0;
}

static int read_save(size_t *len){
    mcOpen(g_port, g_slot, g_path, 1 /* O_RDONLY */);
    int fd = mc_wait(); if (fd < 0) return -1;
    size_t total = 0;
    for (;;){
        size_t want = 0x4000; if (total + want > sizeof(optbuf)) want = sizeof(optbuf) - total;
        if (!want) break;
        mcRead(fd, optbuf + total, (int)want);
        int r = mc_wait(); if (r < 0){ mcClose(fd); mc_wait(); return -2; }
        total += (size_t)r; if (r == 0) break;
    }
    mcClose(fd); mc_wait();
    *len = total; return 0;
}

static int write_save(const uint8_t *data, size_t len){
    mcOpen(g_port, g_slot, g_path, 2 /* O_WRONLY */);
    int fd = mc_wait(); if (fd < 0) return -1;
    size_t done = 0;
    while (done < len){
        size_t w = len - done; if (w > 0x4000) w = 0x4000;
        mcWrite(fd, data + done, (int)w);
        int r = mc_wait(); if (r != (int)w){ mcClose(fd); mc_wait(); return -2; }
        done += w;
    }
    mcClose(fd); mc_wait();
    return 0;
}

static int read_back(size_t len){                 /* le de novo o arquivo e compara com o que gravamos */
    mcOpen(g_port, g_slot, g_path, 1);
    int fd = mc_wait(); if (fd < 0) return -1;
    size_t total = 0;
    while (total < len){
        size_t w = len - total; if (w > 0x4000) w = 0x4000;
        mcRead(fd, vrfbuf + total, (int)w);
        int r = mc_wait(); if (r <= 0) break;
        total += (size_t)r;
    }
    mcClose(fd); mc_wait();
    return (total == len && !memcmp(vrfbuf, outbuf, len)) ? 0 : -3;
}

/* ---------------- telas ---------------- */
enum { ST_CLUB, ST_SQUAD, ST_ID, ST_STATS, ST_SAVE };

static int changed = 0;

static void do_save(void){
    opt_seal(&opt, outbuf);
    int r = write_save(outbuf, opt.len);
    if (r == 0) r = read_back(opt.len);
    if (r == 0){ changed = 0; snprintf(g_msg, sizeof g_msg, "SALVO e conferido no memory card."); }
    else snprintf(g_msg, sizeof g_msg, "ERRO ao salvar (%d). Restaure o backup do memory card!", r);
}

static void quit(void){
    scr_printf("\nSaindo...\n");
    LoadExecPS2("rom0:OSDSYS", 0, NULL);
    for (;;) ;
}

int main(void){
    SifInitRpc(0);
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN", 0, NULL);
    SifLoadModule("rom0:MCMAN", 0, NULL);
    SifLoadModule("rom0:MCSERV", 0, NULL);
    init_scr();
    mcInit(MC_TYPE_MC);
    padInit(0);
    padPortOpen(0, 0, padBuf);
    for (int i = 0; i < 100; i++){ int st = padGetState(0, 0); if (st == PAD_STATE_STABLE || st == PAD_STATE_FINDCTP1) break; for (volatile int k = 0; k < 400000; k++) ; }

    cls(); put(1, "Editor PES 6 / Winning Eleven 10 (PS2)"); put(3, "Procurando o save no memory card (mc0 / mc1)..."); flip();
    if (!find_save()){
        cls(); put(1, "Save de opcoes NAO encontrado.");
        put(3, "Procurei no memory card por: PES 6 PAL (BESLES-...PES6OPT),");
        put(4, "Winning Eleven 10 (BISLPM-66374WE10OPT) e WE2007 US.");
        put(6, "Coloque o memory card com o save e abra o programa de novo."); put(8, "Aperte START para sair."); flip();
        for (;;){ if (wait_buttons() & PAD_START) quit(); }
    }
    cls(); put(1, "Lendo %s ...", g_path); flip();
    size_t len = 0;
    int rr = read_save(&len);
    if (rr || opt_open(&opt, optbuf, len)){
        cls(); put(1, "Nao consegui ler o save (erro %d, tamanho %u).", rr, (unsigned)len);
        put(3, "Tamanhos esperados: %d (PES 6) ou %d (WE10).", LEN_PES6, LEN_WE10); put(5, "START = sair"); flip();
        for (;;){ if (wait_buttons() & PAD_START) quit(); }
    }
    snprintf(g_msg, sizeof g_msg, "Layout %s carregado (cabecalhos %d/10).", opt.we10 ? "WE10" : "PES6", opt.hdr_ok);

    int st = ST_CLUB, club = 0, slot = 0, pid = 1, stat = 0;
    char nm[64];
    for (;;){
        cls();
        if (st == ST_CLUB){
            put(0, "== ESCOLHA O CLUBE ==  (%s)%s", opt.we10 ? "WE10" : "PES6", changed ? "  [ALTERADO - nao salvo]" : "");
            int first = club - (club % 20);
            for (int i = 0; i < 20 && first + i < opt.n_names; i++){
                put(2 + i, "%c %3d  %s", (first + i == club) ? '>' : ' ', first + i, club_name(&opt, first + i, nm, sizeof nm));
            }
            put(23, "CIMA/BAIXO mover  L1/R1 +-10  X escolher  SELECT = digitar ID do jogador");
            put(24, "START = salvar no memory card   L1+R1+START = sair sem salvar");
            put(25, "%s", g_msg);
            flip();
            u32 b = wait_buttons();
            if (b & PAD_UP) club = (club + opt.n_names - 1) % opt.n_names;
            if (b & PAD_DOWN) club = (club + 1) % opt.n_names;
            if (b & PAD_L1) club = (club + opt.n_names - 10) % opt.n_names;
            if (b & PAD_R1) club = (club + 10) % opt.n_names;
            if (b & PAD_CROSS){ st = ST_SQUAD; slot = 0; }
            if (b & PAD_SELECT) st = ST_ID;
            if (b & PAD_START){ if ((pad_poll() & (PAD_L1|PAD_R1)) == (PAD_L1|PAD_R1)) quit(); st = ST_SAVE; }
        } else if (st == ST_SQUAD){
            put(0, "== ELENCO: %s ==%s", club_name(&opt, club, nm, sizeof nm), changed ? "  [ALTERADO]" : "");
            put(1, "(o save nao guarda nomes dos jogadores; aparece o ID e os 3 primeiros atributos)");
            for (int i = 0; i < 32 && i < 22; i++){
                int s = (slot / 22) * 22 + i; if (s >= 32) break;
                int id = squad_id(&opt, club, s);
                if (id < 0) put(3 + i, "%c slot %2d  (vazio)", (s == slot) ? '>' : ' ', s);
                else put(3 + i, "%c slot %2d  ID %5d   ata %3d  def %3d  bal %3d", (s == slot) ? '>' : ' ', s, id,
                         ability_get(&opt, id, 0), ability_get(&opt, id, 1), ability_get(&opt, id, 2));
            }
            put(25, "X editar jogador   O voltar   CIMA/BAIXO mover");
            flip();
            u32 b = wait_buttons();
            if (b & PAD_UP) slot = (slot + 31) % 32;
            if (b & PAD_DOWN) slot = (slot + 1) % 32;
            if (b & PAD_CIRCLE) st = ST_CLUB;
            if (b & PAD_CROSS){ int id = squad_id(&opt, club, slot); if (id >= 0){ pid = id; stat = 0; st = ST_STATS; } }
        } else if (st == ST_ID){
            put(0, "== DIGITAR ID DO JOGADOR ==");
            put(2, "ID: %d   %s", pid, player_valid(&opt, pid) ? "(valido)" : "(invalido)");
            put(4, "CIMA/BAIXO +-1   ESQ/DIR +-10   L1/R1 +-100   L2/R2 = pular p/ jogadores editados (32768+)");
            put(6, "Validos: 0..4999 e 32768..32950 (jogadores editados)");
            put(8, "X abrir   O voltar");
            flip();
            u32 b = wait_buttons();
            if (b & PAD_UP) pid++;  if (b & PAD_DOWN) pid--;
            if (b & PAD_RIGHT) pid += 10; if (b & PAD_LEFT) pid -= 10;
            if (b & PAD_R1) pid += 100; if (b & PAD_L1) pid -= 100;
            if (b & PAD_L2) pid = 32768; if (b & PAD_R2) pid = 1;
            if (pid < 0) pid = 0; if (pid > 65535) pid = 65535;
            if (b & PAD_CIRCLE) st = ST_CLUB;
            if ((b & PAD_CROSS) && player_valid(&opt, pid)){ stat = 0; st = ST_STATS; }
        } else if (st == ST_STATS){
            put(0, "== JOGADOR ID %d ==%s", pid, changed ? "  [ALTERADO - nao salvo]" : "");
            int first = (stat / 20) * 20;
            for (int i = 0; i < 20 && first + i < N_ABILITIES; i++){
                int k = first + i;
                put(2 + i, "%c %-20s %3d", (k == stat) ? '>' : ' ', ABILITIES[k].name, ability_get(&opt, pid, k));
            }
            put(22, "ESQ/DIR -+1   L1/R1 -+10   QUADRADO = 99   TRIANGULO = 127   O voltar");
            put(23, "Valores acima de 99 so valem se o jogo aceitar (teste!).");
            put(25, "%s", g_msg);
            flip();
            u32 b = wait_buttons();
            int v = ability_get(&opt, pid, stat), nv = v;
            if (b & PAD_UP) stat = (stat + N_ABILITIES - 1) % N_ABILITIES;
            if (b & PAD_DOWN) stat = (stat + 1) % N_ABILITIES;
            if (b & PAD_RIGHT) nv = v + 1; if (b & PAD_LEFT) nv = v - 1;
            if (b & PAD_R1) nv = v + 10;   if (b & PAD_L1) nv = v - 10;
            if (b & PAD_SQUARE) nv = 99;   if (b & PAD_TRIANGLE) nv = 127;
            if (nv < 0) nv = 0; if (nv > 127) nv = 127;
            if (nv != v){ if (ability_set(&opt, pid, stat, nv) == 0) changed = 1; }
            if (b & PAD_CIRCLE) st = ST_CLUB;
        } else { /* ST_SAVE */
            put(1, "SALVAR no memory card?");
            put(3, "Isto SOBRESCREVE o save: %s", g_path);
            put(5, "Voce fez backup do memory card?  (use uLaunchELF ou o PS2 para copiar)");
            put(8, "X = SIM, salvar agora      O = NAO, voltar");
            flip();
            u32 b = wait_buttons();
            if (b & PAD_CROSS){ put(10, "Gravando... nao desligue o console."); flip(); do_save(); st = ST_CLUB; }
            if (b & PAD_CIRCLE) st = ST_CLUB;
        }
    }
    return 0;
}
