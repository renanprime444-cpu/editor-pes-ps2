#ifndef CORE_H
#define CORE_H
/* Nucleo independente de plataforma: abre, edita e fecha o "option file"
 * do PES 6 (PAL) e do Winning Eleven 10 (PS2). */
#include <stdint.h>
#include <stddef.h>

#define LEN_PES6 1191936
#define LEN_WE10 1189888
#define NBLOCKS  10

typedef struct { const char *name; int off, shift, mask; } StatDef;
extern const StatDef ABILITIES[];
extern const int N_ABILITIES;

typedef struct {
    uint8_t *buf;          /* dados DECIFRADOS (blocos 1..9) */
    size_t   len;
    int      we10;         /* 1 = Winning Eleven 10, 0 = PES 6 PAL */
    const int *B, *S;      /* inicio e tamanho de cada bloco */
    int player_base;       /* inicio dos 5000 jogadores (124 bytes cada) */
    int edit_base;         /* inicio dos jogadores editados (ids 32768+) */
    int names_off, n_names;/* nomes dos clubes (88 bytes cada) */
    int squad_off, n_squad;/* elencos dos clubes (32 ids de 2 bytes) */
    int hdr_ok;            /* quantos cabecalhos de bloco conferem (de 10) */
} Opt;

int  opt_open(Opt *o, uint8_t *buf, size_t len);       /* 0 = ok */
void opt_seal(const Opt *o, uint8_t *out);             /* out = copia cifrada + checksums */
int  player_valid(const Opt *o, int id);
int  stat_get(const Opt *o, int id, int off, int shift, int mask);
int  stat_set(Opt *o, int id, int off, int shift, int mask, int val);
int  ability_get(const Opt *o, int id, int idx);
int  ability_set(Opt *o, int id, int idx, int val);    /* grava e marca "habilidade editada" */
const char *club_name(const Opt *o, int club, char *tmp, int tmpsz);
int  squad_id(const Opt *o, int club, int slot);       /* -1 = vazio */
#endif
