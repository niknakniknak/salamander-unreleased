/* salamandgo - builds salamandgo.zip from the Gradius Origins executable and
   the MAME Salamander and Life Force zips. It holds no ROM data.

   Copyright (C) 2026 Chris Watson <197681753+niknakniknak@users.noreply.github.com>
   SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(d) _mkdir(d)
#else
#include <sys/stat.h>
#define make_dir(d) mkdir(d, 0777)
#endif

static uint32_t crc_tab[256];

static void crc_init(void) {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_tab[n] = c;
    }
}

static uint32_t crc32(const uint8_t *p, size_t len) {
    uint32_t c = 0xFFFFFFFFu;
    while (len--) c = crc_tab[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

typedef struct {
    const uint8_t *in; size_t inlen, inpos;
    uint8_t *out; size_t outlen, outpos;
    uint32_t bitbuf; int bitcnt, err;
} Inf;

typedef struct { short count[16], symbol[320]; } Huff;

static int bits(Inf *s, int need) {
    uint32_t v = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->inpos >= s->inlen) { s->err = 1; return 0; }
        v |= (uint32_t)s->in[s->inpos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = v >> need;
    s->bitcnt -= need;
    return (int)(v & ((1u << need) - 1));
}

static int decode(Inf *s, const Huff *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= bits(s, 1);
        int count = h->count[len];
        if (code - count < first) return h->symbol[index + (code - first)];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    s->err = 1;
    return 0;
}

static void build(Huff *h, const short *len, int n) {
    short offs[16];
    memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; i++) h->count[len[i]]++;
    h->count[0] = 0;
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->count[i];
    for (int i = 0; i < n; i++) if (len[i]) h->symbol[offs[len[i]]++] = (short)i;
}

static const short LBASE[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const short LEXT[29]  = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const short DBASE[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const short DEXT[30]  = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};

static int codes(Inf *s, const Huff *lh, const Huff *dh) {
    for (;;) {
        int sym = decode(s, lh);
        if (s->err) return 1;
        if (sym < 256) {
            if (s->outpos >= s->outlen) return 1;
            s->out[s->outpos++] = (uint8_t)sym;
        } else if (sym == 256) {
            return 0;
        } else {
            sym -= 257;
            if (sym >= 29) return 1;
            int len = LBASE[sym] + bits(s, LEXT[sym]);
            int d = decode(s, dh);
            if (s->err || d >= 30) return 1;
            size_t dist = DBASE[d] + bits(s, DEXT[d]);
            if (s->err || dist > s->outpos || s->outpos + len > s->outlen) return 1;
            while (len--) { s->out[s->outpos] = s->out[s->outpos - dist]; s->outpos++; }
        }
    }
}

static int inflate(const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen) {
    static const short ORDER[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    Inf s = { in, inlen, 0, out, outlen, 0, 0, 0, 0 };
    Huff lh, dh;
    short len[320];
    int last;
    do {
        last = bits(&s, 1);
        int type = bits(&s, 2);
        if (s.err) return 1;
        if (type == 0) {
            s.bitbuf = 0; s.bitcnt = 0;
            if (s.inpos + 4 > s.inlen) return 1;
            unsigned n = s.in[s.inpos] | s.in[s.inpos + 1] << 8;
            s.inpos += 4;
            if (s.inpos + n > s.inlen || s.outpos + n > s.outlen) return 1;
            memcpy(s.out + s.outpos, s.in + s.inpos, n);
            s.inpos += n; s.outpos += n;
        } else if (type == 1) {
            int i = 0;
            for (; i < 144; i++) len[i] = 8;
            for (; i < 256; i++) len[i] = 9;
            for (; i < 280; i++) len[i] = 7;
            for (; i < 288; i++) len[i] = 8;
            build(&lh, len, 288);
            for (i = 0; i < 30; i++) len[i] = 5;
            build(&dh, len, 30);
            if (codes(&s, &lh, &dh)) return 1;
        } else if (type == 2) {
            int nlen = bits(&s, 5) + 257, ndist = bits(&s, 5) + 1, ncode = bits(&s, 4) + 4;
            if (s.err || nlen > 286 || ndist > 30) return 1;
            memset(len, 0, sizeof len);
            for (int i = 0; i < ncode; i++) len[ORDER[i]] = (short)bits(&s, 3);
            build(&lh, len, 19);
            for (int i = 0; i < nlen + ndist;) {
                int sym = decode(&s, &lh), rep;
                short v = 0;
                if (s.err) return 1;
                if (sym < 16) { len[i++] = (short)sym; continue; }
                if (sym == 16) { if (!i) return 1; v = len[i - 1]; rep = 3 + bits(&s, 2); }
                else if (sym == 17) rep = 3 + bits(&s, 3);
                else rep = 11 + bits(&s, 7);
                if (s.err || i + rep > nlen + ndist) return 1;
                while (rep--) len[i++] = v;
            }
            build(&lh, len, nlen);
            build(&dh, len + nlen, ndist);
            if (codes(&s, &lh, &dh)) return 1;
        } else return 1;
    } while (!last);
    return s.outpos != s.outlen;
}

static uint8_t *load(const char *name, size_t *len) {
    FILE *f = fopen(name, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *p = n > 0 ? malloc(n) : NULL;
    if (p && fread(p, 1, n, f) != (size_t)n) { free(p); p = NULL; }
    fclose(f);
    *len = p ? (size_t)n : 0;
    return p;
}

static unsigned rd16(const uint8_t *p) { return p[0] | p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | (uint32_t)rd16(p + 2) << 16; }

static int is_zip(const uint8_t *z, size_t len) { return len > 4 && rd32(z) == 0x04034b50; }

static uint8_t *zip_find(const uint8_t *z, size_t zlen, uint32_t crc, uint32_t size) {
    if (zlen < 22) return NULL;
    size_t e = zlen - 22;
    while (e > 0 && rd32(z + e) != 0x06054b50) e--;
    if (rd32(z + e) != 0x06054b50) return NULL;
    unsigned n = rd16(z + e + 10);
    size_t p = rd32(z + e + 16);
    for (unsigned i = 0; i < n && p + 46 <= zlen; i++) {
        if (rd32(z + p) != 0x02014b50) return NULL;
        unsigned method = rd16(z + p + 10);
        uint32_t c = rd32(z + p + 16), csize = rd32(z + p + 20), usize = rd32(z + p + 24);
        size_t lh = rd32(z + p + 42);
        if (c == crc && usize == size && lh + 30 <= zlen) {
            size_t data = lh + 30 + rd16(z + lh + 26) + rd16(z + lh + 28);
            uint8_t *out = malloc(size);
            int bad = data + csize > zlen;
            if (!bad && method == 0) memcpy(out, z + data, size);
            else if (!bad && method == 8) bad = inflate(z + data, csize, out, size);
            else bad = 1;
            if (!bad && crc32(out, size) == crc) return out;
            free(out);
        }
        p += 46 + rd16(z + p + 28) + rd16(z + p + 30) + rd16(z + p + 32);
    }
    return NULL;
}

static void put16(FILE *f, unsigned v) { fputc(v & 0xFF, f); fputc(v >> 8 & 0xFF, f); }
static void put32(FILE *f, uint32_t v) { put16(f, v & 0xFFFF); put16(f, v >> 16); }

typedef struct { const char *name; uint32_t size, crc; uint8_t *data; } Rom;

static Rom rom[] = {
    { "lfp.18b",     0x10000, 0x7930ac18, NULL },
    { "lfp.18c",     0x10000, 0xa4379887, NULL },
    { "lfp.17b",     0x20000, 0x0b412515, NULL },
    { "lfp.17c",     0x20000, 0x17400273, NULL },
    { "lfp.11j",     0x08000, 0xd70e491e, NULL },
    { "lfp.8g",      0x04000, 0xf178e685, NULL },
    { "587-c01.10a", 0x20000, 0x09fe0632, NULL },
};
enum { P18B, P18C, D17B, D17C, S11J, V8G, C01, NROMS };

static const char *mame_names[NROMS] = {
    "587-n02.18b", "587-n05.18c", "587-n03.17b", "587-n06.17c", "587-n09.11j", "587-n08.8g", "587-c01.10a" };

#define D08_CRC 0xf9ac6b82u
#define K08_CRC 0x7f0e9b41u

/* Blocks are found by size and a fingerprint of their first 64 bytes. */
typedef struct { const char *what; uint32_t size; unsigned sum32; uint32_t crc64; int even, odd; } Block;

static const Block blocks[] = {
    { "main program",  0x20000,  178, 0xd40131c8, P18B, P18C },
    { "data",          0x40000,  497, 0x3b9eace8, D17B, D17C },
    { "sound program", 0x08000, 2500, 0x2538ed1d, S11J, -1   },
    { "samples",       0x20000, 2168, 0x5c369637, C01,  -1   },
};
#define NBLOCKS 4

static int try_block(const Block *b, const uint8_t *p) {
    if (b->odd < 0) {
        if (crc32(p, b->size) != rom[b->even].crc) return 0;
        rom[b->even].data = malloc(b->size);
        memcpy(rom[b->even].data, p, b->size);
        return 1;
    }
    uint32_t half = b->size / 2;
    uint8_t *e = malloc(half), *o = malloc(half);
    for (uint32_t i = 0; i < half; i++) { e[i] = p[2 * i]; o[i] = p[2 * i + 1]; }
    if (crc32(e, half) == rom[b->even].crc && crc32(o, half) == rom[b->odd].crc) {
        rom[b->even].data = e; rom[b->odd].data = o;
        return 1;
    }
    free(e); free(o);
    return 0;
}

static void scan_exe(const uint8_t *x, size_t len) {
    if (len < 32) return;
    unsigned sum = 0;
    for (int i = 0; i < 32; i++) sum += x[i];
    for (size_t p = 0; p + 32 <= len; p++) {
        for (int k = 0; k < NBLOCKS; k++) {
            const Block *b = &blocks[k];
            if (rom[b->even].data || sum != b->sum32 || p + b->size > len) continue;
            if (crc32(x + p, 64) == b->crc64) try_block(b, x + p);
        }
        if (p + 32 < len) sum += x[p + 32] - x[p];
    }
}

/* Phrases 00-2B from Life Force (12 left out), 2C-2E from Salamander 16-18. */
static uint8_t *build_speech(const uint8_t *k08, const uint8_t *d08) {
    enum { N = 0x2F, KN = 0x2D, DN = 0x1F };
    uint8_t *v = malloc(0x4000);
    unsigned tab[N], pos = 2 * N, kt[KN], dt[DN], kend = 0x4000;
    for (int i = 0; i < KN; i++) kt[i] = k08[2 * i] << 8 | k08[2 * i + 1];
    for (int i = 0; i < DN; i++) dt[i] = d08[2 * i] << 8 | d08[2 * i + 1];
    while (kend > 0 && k08[kend - 1] == 0xFF) kend--;
    memset(v, 0xFF, 0x4000);
    for (int i = 0; i < N; i++) tab[i] = 0xFFFF;
    for (int i = 0; i < 0x2C; i++) {
        if (kt[i] == 0xFFFF || i == 0x12) continue;
        unsigned end = kend;
        for (int j = i + 1; j < KN; j++) if (kt[j] != 0xFFFF) { end = kt[j]; break; }
        if (end < kt[i] || pos + end - kt[i] > 0x4000) { free(v); return NULL; }
        tab[i] = pos;
        memcpy(v + pos, k08 + kt[i], end - kt[i]);
        pos += end - kt[i];
    }
    static const int from[3] = { 0x16, 0x17, 0x18 };
    for (int k = 0; k < 3; k++) {
        unsigned a = dt[from[k]], n = dt[from[k] + 1] - a;
        if (pos + n > 0x4000) { free(v); return NULL; }
        tab[0x2C + k] = pos;
        memcpy(v + pos, d08 + a, n);
        pos += n;
    }
    tab[0x12] = tab[0x2E];
    for (int i = 0; i < N; i++) { v[2 * i] = tab[i] >> 8; v[2 * i + 1] = tab[i] & 0xFF; }
    return v;
}

static int write_zip(const char *name, int for_mame) {
    FILE *f = fopen(name, "wb");
    if (!f) return 1;
    uint32_t off[NROMS], pos = 0;
    const unsigned TIME = 0, DATE = (2026 - 1980) << 9 | 11 << 5 | 3;
    for (int i = 0; i < NROMS; i++) {
        const char *rn = for_mame ? mame_names[i] : rom[i].name;
        unsigned nl = strlen(rn);
        off[i] = pos;
        put32(f, 0x04034b50); put16(f, 10); put16(f, 0); put16(f, 0);
        put16(f, TIME); put16(f, DATE);
        put32(f, rom[i].crc); put32(f, rom[i].size); put32(f, rom[i].size);
        put16(f, nl); put16(f, 0);
        fwrite(rn, 1, nl, f);
        fwrite(rom[i].data, 1, rom[i].size, f);
        pos += 30 + nl + rom[i].size;
    }
    uint32_t cd = pos;
    for (int i = 0; i < NROMS; i++) {
        const char *rn = for_mame ? mame_names[i] : rom[i].name;
        unsigned nl = strlen(rn);
        put32(f, 0x02014b50); put16(f, 20); put16(f, 10); put16(f, 0); put16(f, 0);
        put16(f, TIME); put16(f, DATE);
        put32(f, rom[i].crc); put32(f, rom[i].size); put32(f, rom[i].size);
        put16(f, nl); put16(f, 0); put16(f, 0); put16(f, 0); put16(f, 0);
        put32(f, 0); put32(f, off[i]);
        fwrite(rn, 1, nl, f);
        pos += 46 + nl;
    }
    put32(f, 0x06054b50); put16(f, 0); put16(f, 0); put16(f, NROMS); put16(f, NROMS);
    put32(f, pos - cd); put32(f, cd); put16(f, 0);
    return fclose(f) != 0;
}

static void done(int pause) {
#ifdef _WIN32
    if (pause) { printf("\nPress Enter to close."); getchar(); }
#else
    (void)pause;
#endif
}

static void needs(void) {
    printf("\nIt needs:\n"
           "  origins.exe   from Gradius Origins (the game's executable)\n"
           "  salamand.zip  MAME Salamander, for 587-d08.8g (speech) and 587-c01.10a\n"
           "  lifefrce.zip  MAME Life Force, for 587-k08.8g (speech)\n"
           "                (lifefrcej.zip works too; a merged salamand.zip has both)\n"
           "Put them next to this program and run it, or give them as arguments.\n");
}

int main(int argc, char **argv) {
    static const char *defaults[] = { "origins.exe", "salamand.zip", "lifefrce.zip", "lifefrcej.zip" };
    const char *files[64];
    int nfiles = 0, pause = argc < 2, mister = 0, mame = 0, given;
    uint8_t *d08 = NULL, *k08 = NULL;
    int exe_seen = 0;

    crc_init();
    printf("salamandgo - builds salamandgo.zip (Salamander, Life Force weapons, unreleased)\n\n");
    for (int i = 1; i < argc && nfiles < 64; i++) {
        if (!strcmp(argv[i], "-mister")) mister = 1;
        else if (!strcmp(argv[i], "-mame")) mame = 1;
        else files[nfiles++] = argv[i];
    }
    if (!mister && !mame) mister = mame = 1;
    given = nfiles > 0;
    if (nfiles == 0) { memcpy(files, defaults, sizeof defaults); nfiles = 4; }

    for (int i = 0; i < nfiles; i++) {
        size_t len;
        uint8_t *p = load(files[i], &len);
        if (!p) {
            if (given) printf("  %-14s cannot open\n", files[i]);
            continue;
        }
        if (is_zip(p, len)) {
            int got = 0;
            if (!d08 && (d08 = zip_find(p, len, D08_CRC, 0x4000))) got++;
            if (!k08 && (k08 = zip_find(p, len, K08_CRC, 0x4000))) got++;
            if (!rom[C01].data && (rom[C01].data = zip_find(p, len, rom[C01].crc, rom[C01].size))) got++;
            printf("  %-14s %d ROM%s it needs\n", files[i], got, got == 1 ? "" : "s");
        } else {
            int before = 0, after = 0;
            for (int k = 0; k < NROMS; k++) before += rom[k].data != NULL;
            scan_exe(p, len);
            for (int k = 0; k < NROMS; k++) after += rom[k].data != NULL;
            exe_seen = 1;
            printf("  %-14s %d ROM%s it needs\n", files[i], after - before, after - before == 1 ? "" : "s");
        }
        free(p);
    }

    int missing = 0;
    printf("\n");
    for (int k = 0; k < NROMS; k++) {
        if (k == V8G) continue;
        if (!rom[k].data) {
            printf("  missing %-12s (%s)\n", rom[k].name,
                   k == C01 ? "from salamand.zip or origins.exe" : "from origins.exe");
            missing = 1;
        }
    }
    if (!d08) { printf("  missing %-12s (from salamand.zip)\n", "587-d08.8g"); missing = 1; }
    if (!k08) { printf("  missing %-12s (from lifefrce.zip)\n", "587-k08.8g"); missing = 1; }
    if (missing) {
        if (exe_seen && !rom[P18B].data)
            printf("\nThe executable did not have this game's ROMs. If Gradius Origins has been\n"
                   "updated, they may now be stored differently; this program needs updating.\n");
        needs();
        done(pause);
        return 1;
    }

    rom[V8G].data = build_speech(k08, d08);
    if (!rom[V8G].data || crc32(rom[V8G].data, 0x4000) != rom[V8G].crc) {
        printf("Building the speech ROM went wrong (CRC check failed).\n");
        done(pause);
        return 1;
    }

    static char out[4096];
    size_t dir = 0;
    for (size_t i = 0; argv[0][i] && i < sizeof out - 32; i++)
        if (argv[0][i] == '/' || argv[0][i] == '\\') dir = i + 1;
    memcpy(out, argv[0], dir);
    if (mister) {
        strcpy(out + dir, "salamandgo.zip");
        if (write_zip(out, 0)) {
            printf("Cannot write %s\n", out);
            done(pause);
            return 1;
        }
        printf("Wrote %s for the MiSTer: copy it to games/mame, with the .mra file.\n", out);
    }
    if (mame) {
        strcpy(out + dir, "mame");
        make_dir(out);
        strcpy(out + dir, "mame/lifefrcej.zip");
        if (write_zip(out, 1)) {
            printf("Cannot write %s\n", out);
            done(pause);
            return 1;
        }
        printf("Wrote %s for MAME: run mame lifefrcej -rompath %.*smame\n", out, (int)dir, out);
    }
    printf("All %d ROMs CRC checked.\n", NROMS);
    done(pause);
    return 0;
}
