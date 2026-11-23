/******************************************************************************/
// Bullfrog Engine Smacker Playback Library - for use to remake classic games
// like Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file smkdec.c
 *     Smacker video decoder in portable C.
 * @par Purpose:
 *     Decodes Smacker files (palette, video frames, audio tracks) without
 *     the original x86 asm decoder.
 * @par Comment:
 *     Written from the Smacker file format description. The bitstream is
 *     read least significant bit first. Video is coded in 4x4 blocks with
 *     four Huffman "big trees" (mono map, mono colours, full blocks, block
 *     types); each big tree keeps a cache of the three most recent values,
 *     reset at the start of each frame.
 * @author   SyndicateVR decomp agent
 * @date     22 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "smkdec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************/

#define SMK_HEADER_SIZE 104

enum SmkBlockType {
    SMK_BLK_MONO = 0,
    SMK_BLK_FULL = 1,
    SMK_BLK_VOID = 2,
    SMK_BLK_SOLID = 3,
};

static const uint16_t smk_block_runs[64] = {
       1,    2,    3,    4,    5,    6,    7,    8,
       9,   10,   11,   12,   13,   14,   15,   16,
      17,   18,   19,   20,   21,   22,   23,   24,
      25,   26,   27,   28,   29,   30,   31,   32,
      33,   34,   35,   36,   37,   38,   39,   40,
      41,   42,   43,   44,   45,   46,   47,   48,
      49,   50,   51,   52,   53,   54,   55,   56,
      57,   58,   59,  128,  256,  512, 1024, 2048,
};

/** Bit reader, least significant bit first. */
struct SmkBits {
    const uint8_t *data;
    uint32_t size_bits;
    uint32_t pos;
    int overrun;
};

static void bits_init(struct SmkBits *b, const uint8_t *data, uint32_t size)
{
    b->data = data;
    b->size_bits = size * 8;
    b->pos = 0;
    b->overrun = 0;
}

static uint32_t bits_get1(struct SmkBits *b)
{
    uint32_t v;
    if (b->pos >= b->size_bits) {
        b->overrun = 1;
        return 0;
    }
    v = (b->data[b->pos >> 3] >> (b->pos & 7)) & 1;
    b->pos++;
    return v;
}

static uint32_t bits_get(struct SmkBits *b, int n)
{
    uint32_t v = 0;
    int i;
    for (i = 0; i < n; i++)
        v |= bits_get1(b) << i;
    return v;
}

/** Huffman tree of 8-bit values. Node i: child[i][0..1] >= 0 for inner
 * nodes; leaves have child[i][0] = -1 and their value in val[i]. */
struct SmkSmallTree {
    int16_t child[512][2];
    uint8_t val[512];
    int count;
    int present;
};

static int small_tree_node(struct SmkBits *b, struct SmkSmallTree *t, int depth)
{
    int n;
    if (t->count >= 511 || depth > 32)
        return -1;
    n = t->count++;
    if (bits_get1(b)) {
        int l, r;
        l = small_tree_node(b, t, depth + 1);
        if (l < 0)
            return -1;
        r = small_tree_node(b, t, depth + 1);
        if (r < 0)
            return -1;
        t->child[n][0] = l;
        t->child[n][1] = r;
    } else {
        t->child[n][0] = -1;
        t->child[n][1] = -1;
        t->val[n] = bits_get(b, 8);
    }
    return n;
}

/** Reads a presence bit and, if set, the tree and its terminating bit. */
static int small_tree_read(struct SmkBits *b, struct SmkSmallTree *t)
{
    t->count = 0;
    t->present = bits_get1(b);
    if (!t->present)
        return 0;
    if (small_tree_node(b, t, 0) < 0)
        return -1;
    bits_get1(b); // end of tree
    return b->overrun ? -1 : 0;
}

static uint8_t small_tree_decode(struct SmkBits *b, const struct SmkSmallTree *t)
{
    int n = 0;
    if (!t->present)
        return 0;
    while (t->child[n][0] >= 0)
        n = t->child[n][bits_get1(b)];
    return t->val[n];
}

/** Big tree of 16-bit values with a cache of the last three values. */
struct SmkDecTree {
    int32_t *child0;  /**< left child, -1 for leaves */
    int32_t *child1;
    uint16_t *val;
    int count;
    int cap;
    int last[3];      /**< leaves holding the cached values */
};

struct SmkBigCtx {
    struct SmkSmallTree lo, hi;
    uint16_t esc[3];
};

static int big_tree_alloc(struct SmkDecTree *t, int cap)
{
    t->cap = cap;
    t->count = 0;
    t->child0 = malloc(sizeof(int32_t) * cap);
    t->child1 = malloc(sizeof(int32_t) * cap);
    t->val = malloc(sizeof(uint16_t) * cap);
    return (t->child0 && t->child1 && t->val) ? 0 : -1;
}

static int big_tree_new(struct SmkDecTree *t)
{
    if (t->count >= t->cap)
        return -1;
    t->child0[t->count] = -1;
    t->child1[t->count] = -1;
    t->val[t->count] = 0;
    return t->count++;
}

static int big_tree_node(struct SmkBits *b, struct SmkDecTree *t,
  struct SmkBigCtx *ctx, int depth)
{
    int n;
    if (depth > 64)
        return -1;
    n = big_tree_new(t);
    if (n < 0)
        return -1;
    if (bits_get1(b)) {
        int l, r;
        l = big_tree_node(b, t, ctx, depth + 1);
        if (l < 0)
            return -1;
        r = big_tree_node(b, t, ctx, depth + 1);
        if (r < 0)
            return -1;
        t->child0[n] = l;
        t->child1[n] = r;
    } else {
        uint16_t v;
        v = small_tree_decode(b, &ctx->lo);
        v |= (uint16_t)small_tree_decode(b, &ctx->hi) << 8;
        if (v == ctx->esc[0]) {
            t->last[0] = n;
            v = 0;
        } else if (v == ctx->esc[1]) {
            t->last[1] = n;
            v = 0;
        } else if (v == ctx->esc[2]) {
            t->last[2] = n;
            v = 0;
        }
        t->val[n] = v;
    }
    return n;
}

/** Reads one big tree: presence bit, low and high byte trees, the three
 * escape values, the tree and its terminating bit. size is the unpacked
 * size from the header, which bounds the number of nodes. */
static int big_tree_read(struct SmkBits *b, struct SmkDecTree *t, uint32_t size)
{
    struct SmkBigCtx *ctx;
    int k;

    memset(t, 0, sizeof(*t));
    if (!bits_get1(b)) {
        // absent tree: a single leaf of value 0
        if (big_tree_alloc(t, 4) < 0)
            return -1;
        big_tree_new(t);
        k = big_tree_new(t);
        t->last[0] = t->last[1] = t->last[2] = k;
        return 0;
    }
    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
        return -1;
    if (small_tree_read(b, &ctx->lo) < 0 || small_tree_read(b, &ctx->hi) < 0) {
        free(ctx);
        return -1;
    }
    ctx->esc[0] = bits_get(b, 16);
    ctx->esc[1] = bits_get(b, 16);
    ctx->esc[2] = bits_get(b, 16);
    t->last[0] = t->last[1] = t->last[2] = -1;
    if (big_tree_alloc(t, (int)(size / 4) + 16) < 0 || big_tree_node(b, t, ctx, 0) < 0) {
        free(ctx);
        return -1;
    }
    bits_get1(b); // end of tree
    free(ctx);
    for (k = 0; k < 3; k++) {
        if (t->last[k] < 0) {
            int n = big_tree_new(t);
            if (n < 0)
                return -1;
            t->last[k] = n;
        }
    }
    return b->overrun ? -1 : 0;
}

static void big_tree_free(struct SmkDecTree *t)
{
    if (t == NULL)
        return;
    free(t->child0);
    free(t->child1);
    free(t->val);
}

static void big_tree_reset(struct SmkDecTree *t)
{
    t->val[t->last[0]] = 0;
    t->val[t->last[1]] = 0;
    t->val[t->last[2]] = 0;
}

static uint16_t big_tree_decode(struct SmkBits *b, struct SmkDecTree *t)
{
    int n = 0;
    uint16_t v;
    while (t->child0[n] >= 0)
        n = bits_get1(b) ? t->child1[n] : t->child0[n];
    v = t->val[n];
    if (v != t->val[t->last[0]]) {
        t->val[t->last[2]] = t->val[t->last[1]];
        t->val[t->last[1]] = t->val[t->last[0]];
        t->val[t->last[0]] = v;
    }
    return v;
}

/******************************************************************************/

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

struct SmkDec *smkdec_open_mem(uint8_t *buf, uint32_t size)
{
    struct SmkDec *d;
    uint32_t nfr, i, pos, tree_size;
    int32_t rate;
    struct SmkBits b;

    if (buf == NULL || size < SMK_HEADER_SIZE || memcmp(buf, "SMK", 3) != 0)
        goto fail_buf;
    if (buf[3] != '2' && buf[3] != '4')
        goto fail_buf;
    d = calloc(1, sizeof(*d));
    if (d == NULL)
        goto fail_buf;
    d->file = buf;
    d->file_size = size;
    d->version = buf[3] - '0';
    d->width = rd32(buf + 4);
    d->height = rd32(buf + 8);
    d->frames = rd32(buf + 12);
    rate = (int32_t)rd32(buf + 16);
    d->flags = rd32(buf + 20);
    if (rate > 0)
        d->us_per_frame = rate * 1000;
    else if (rate < 0)
        d->us_per_frame = (uint32_t)(-rate) * 10;
    else
        d->us_per_frame = 100000;
    for (i = 0; i < SMKDEC_TRACKS; i++)
    {
        uint32_t ar = rd32(buf + 72 + 4 * i);
        if ((ar & 0x40000000) == 0)
            continue;
        d->audio[i].rate = ar & 0xFFFFFF;
        d->audio[i].packed = (ar & 0x80000000) ? 1 : 0;
        d->audio[i].bits = (ar & 0x20000000) ? 16 : 8;
        d->audio[i].channels = (ar & 0x10000000) ? 2 : 1;
    }
    tree_size = rd32(buf + 52);
    if (d->width == 0 || d->height == 0 || d->width > 4096 || d->height > 4096 ||
      d->frames == 0 || d->frames > 1000000)
        goto fail;
    nfr = d->frames + ((d->flags & 1) ? 1 : 0);
    pos = SMK_HEADER_SIZE;
    if (pos + nfr * 5 + tree_size > size)
        goto fail;
    d->frame_size = malloc(sizeof(uint32_t) * nfr);
    d->frame_type = malloc(nfr);
    d->frame_offs = malloc(sizeof(uint32_t) * nfr);
    if (!d->frame_size || !d->frame_type || !d->frame_offs)
        goto fail;
    for (i = 0; i < nfr; i++)
        d->frame_size[i] = rd32(buf + pos + 4 * i) & ~3u;
    pos += 4 * nfr;
    memcpy(d->frame_type, buf + pos, nfr);
    pos += nfr;
    bits_init(&b, buf + pos, tree_size);
    for (i = 0; i < 4; i++)
    {
        d->tree[i] = calloc(1, sizeof(struct SmkDecTree));
        if (d->tree[i] == NULL || big_tree_read(&b, d->tree[i], rd32(buf + 56 + 4 * i)) < 0)
            goto fail;
    }
    pos += tree_size;
    for (i = 0; i < nfr; i++)
    {
        d->frame_offs[i] = pos;
        if (pos + d->frame_size[i] > size)
            goto fail;
        pos += d->frame_size[i];
    }
    d->image = calloc(d->width, d->height);
    if (d->image == NULL)
        goto fail;
    return d;
fail:
    smkdec_close(d);
    return NULL;
fail_buf:
    free(buf);
    return NULL;
}

struct SmkDec *smkdec_open(const char *fname)
{
    FILE *fp;
    long len;
    uint8_t *buf;

    fp = fopen(fname, "rb");
    if (fp == NULL)
        return NULL;
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (len <= 0) {
        fclose(fp);
        return NULL;
    }
    buf = malloc(len);
    if (buf == NULL || fread(buf, 1, len, fp) != (size_t)len) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    return smkdec_open_mem(buf, (uint32_t)len);
}

void smkdec_close(struct SmkDec *d)
{
    int i;
    if (d == NULL)
        return;
    for (i = 0; i < 4; i++) {
        big_tree_free(d->tree[i]);
        free(d->tree[i]);
    }
    free(d->frame_size);
    free(d->frame_type);
    free(d->frame_offs);
    free(d->image);
    free(d->file);
    free(d);
}

/** Applies a palette record; returns its size in bytes, or -1. */
static int32_t decode_palette(struct SmkDec *d, const uint8_t *p, uint32_t avail)
{
    uint8_t oldpal[768];
    uint32_t size, k, sz;
    uint8_t *pal;

    if (avail < 1)
        return -1;
    size = p[0] * 4;
    if (size == 0 || size > avail)
        return -1;
    memcpy(oldpal, d->palette, 768);
    pal = d->palette;
    k = 1;
    sz = 0;
    while (sz < 256 && k < size)
    {
        uint8_t t = p[k++];
        if (t & 0x80) {
            // keep entries
            sz += (t & 0x7F) + 1;
            pal += ((t & 0x7F) + 1) * 3;
        } else if (t & 0x40) {
            // copy entries of the previous palette
            uint32_t off, j;
            if (k >= size)
                return -1;
            off = p[k++];
            j = (t & 0x3F) + 1;
            if (off + j > 256)
                return -1;
            off *= 3;
            while (j-- && sz < 256) {
                *pal++ = oldpal[off++];
                *pal++ = oldpal[off++];
                *pal++ = oldpal[off++];
                sz++;
            }
        } else {
            // new entry, 6-bit components
            if (k + 2 > size)
                return -1;
            *pal++ = t & 0x3F;
            *pal++ = p[k++] & 0x3F;
            *pal++ = p[k++] & 0x3F;
            sz++;
        }
    }
    return size;
}

static void decode_video(struct SmkDec *d, const uint8_t *p, uint32_t size)
{
    struct SmkBits b;
    uint32_t bw, blocks, blk, stride;
    int i;

    bits_init(&b, p, size);
    for (i = 0; i < 4; i++)
        big_tree_reset(d->tree[i]);
    bw = d->width / 4;
    blocks = bw * (d->height / 4);
    stride = d->width;
    blk = 0;
    while (blk < blocks && !b.overrun)
    {
        uint16_t type;
        uint32_t run;
        int mode;
        uint8_t *out;

        type = big_tree_decode(&b, d->tree[3]);
        run = smk_block_runs[(type >> 2) & 0x3F];
        switch (type & 3)
        {
        case SMK_BLK_MONO:
            while (run-- && blk < blocks)
            {
                uint16_t clr, map;
                uint8_t hi, lo;
                clr = big_tree_decode(&b, d->tree[1]);
                map = big_tree_decode(&b, d->tree[0]);
                out = d->image + (blk / bw) * stride * 4 + (blk % bw) * 4;
                hi = clr >> 8;
                lo = clr & 0xFF;
                for (i = 0; i < 4; i++) {
                    out[0] = (map & 1) ? hi : lo;
                    out[1] = (map & 2) ? hi : lo;
                    out[2] = (map & 4) ? hi : lo;
                    out[3] = (map & 8) ? hi : lo;
                    map >>= 4;
                    out += stride;
                }
                blk++;
            }
            break;
        case SMK_BLK_FULL:
            mode = 0;
            if (d->version == 4) {
                if (bits_get1(&b))
                    mode = 1;
                else if (bits_get1(&b))
                    mode = 2;
            }
            while (run-- && blk < blocks)
            {
                uint16_t pix, pix1, pix2;
                out = d->image + (blk / bw) * stride * 4 + (blk % bw) * 4;
                switch (mode)
                {
                case 0:
                    for (i = 0; i < 4; i++) {
                        pix = big_tree_decode(&b, d->tree[2]);
                        out[2] = pix & 0xFF;
                        out[3] = pix >> 8;
                        pix = big_tree_decode(&b, d->tree[2]);
                        out[0] = pix & 0xFF;
                        out[1] = pix >> 8;
                        out += stride;
                    }
                    break;
                case 1:
                    for (i = 0; i < 2; i++) {
                        pix = big_tree_decode(&b, d->tree[2]);
                        out[0] = out[1] = pix & 0xFF;
                        out[2] = out[3] = pix >> 8;
                        out += stride;
                        out[0] = out[1] = pix & 0xFF;
                        out[2] = out[3] = pix >> 8;
                        out += stride;
                    }
                    break;
                default:
                    for (i = 0; i < 2; i++) {
                        pix2 = big_tree_decode(&b, d->tree[2]);
                        pix1 = big_tree_decode(&b, d->tree[2]);
                        out[0] = pix1 & 0xFF;
                        out[1] = pix1 >> 8;
                        out[2] = pix2 & 0xFF;
                        out[3] = pix2 >> 8;
                        out += stride;
                        out[0] = pix1 & 0xFF;
                        out[1] = pix1 >> 8;
                        out[2] = pix2 & 0xFF;
                        out[3] = pix2 >> 8;
                        out += stride;
                    }
                    break;
                }
                blk++;
            }
            break;
        case SMK_BLK_VOID:
            while (run-- && blk < blocks)
                blk++;
            break;
        case SMK_BLK_SOLID:
            {
                uint8_t col = type >> 8;
                while (run-- && blk < blocks)
                {
                    out = d->image + (blk / bw) * stride * 4 + (blk % bw) * 4;
                    for (i = 0; i < 4; i++) {
                        out[0] = out[1] = out[2] = out[3] = col;
                        out += stride;
                    }
                    blk++;
                }
            }
            break;
        }
    }
}

/** Position of the audio chunk of a track within a frame (after the
 * palette record and the chunks of lower tracks), or 0 if absent. */
static uint32_t audio_chunk(const struct SmkDec *d, uint32_t frame, int track,
  uint32_t *p_len)
{
    uint32_t pos, end;
    int i;

    pos = d->frame_offs[frame];
    end = pos + d->frame_size[frame];
    if (d->frame_type[frame] & 1) {
        if (pos >= end)
            return 0;
        pos += d->file[pos] * 4;
    }
    for (i = 0; i < SMKDEC_TRACKS; i++)
    {
        uint32_t len;
        if ((d->frame_type[frame] & (2 << i)) == 0)
            continue;
        if (pos + 4 > end)
            return 0;
        len = rd32(d->file + pos);
        if (len < 4 || pos + len > end)
            return 0;
        if (i == track) {
            *p_len = len - 4;
            return pos + 4;
        }
        pos += len;
    }
    return 0;
}

int32_t smkdec_audio_chunk(struct SmkDec *d, uint32_t frame, int track,
  const uint8_t **p_data)
{
    uint32_t pos, len;
    if (track < 0 || track >= SMKDEC_TRACKS || frame >= d->frames)
        return -1;
    pos = audio_chunk(d, frame, track, &len);
    if (pos == 0)
        return 0;
    *p_data = d->file + pos;
    return len;
}

int smkdec_next_frame(struct SmkDec *d)
{
    uint32_t pos, end;
    int i;

    if (d->next_frame >= d->frames)
        return -1;
    pos = d->frame_offs[d->next_frame];
    end = pos + d->frame_size[d->next_frame];
    d->palette_changed = 0;
    if (d->frame_type[d->next_frame] & 1) {
        int32_t n = decode_palette(d, d->file + pos, end - pos);
        if (n < 0)
            return -1;
        pos += n;
        d->palette_changed = 1;
    }
    for (i = 0; i < SMKDEC_TRACKS; i++)
    {
        if ((d->frame_type[d->next_frame] & (2 << i)) == 0)
            continue;
        if (pos + 4 > end)
            return -1;
        pos += rd32(d->file + pos);
    }
    if (pos > end)
        return -1;
    decode_video(d, d->file + pos, end - pos);
    d->next_frame++;
    return 0;
}

static int32_t unpack_audio(const struct SmkDecAudio *a, const uint8_t *p,
  uint32_t len, uint8_t *out, uint32_t out_size)
{
    struct SmkBits b;
    struct SmkSmallTree *tree;
    uint32_t unp, i, nsamp;
    int stereo, bits16, ntrees, k;
    int32_t pred[2];

    if (len < 4)
        return -1;
    unp = rd32(p);
    if (unp > out_size)
        return -1;
    bits_init(&b, p + 4, len - 4);
    if (!bits_get1(&b))
        return 0; // no data
    stereo = bits_get1(&b);
    bits16 = bits_get1(&b);
    if (stereo != (a->channels == 2) || bits16 != (a->bits == 16))
        return -1;
    ntrees = 1 << (stereo + bits16);
    tree = calloc(ntrees, sizeof(*tree));
    if (tree == NULL)
        return -1;
    for (k = 0; k < ntrees; k++) {
        if (small_tree_read(&b, &tree[k]) < 0) {
            free(tree);
            return -1;
        }
    }
    if (bits16)
    {
        int16_t *s = (int16_t *)out;
        nsamp = unp / 2;
        for (k = stereo; k >= 0; k--) {
            uint32_t v = bits_get(&b, 16);
            // stored high byte first
            pred[k] = (int16_t)(((v & 0xFF) << 8) | (v >> 8));
        }
        for (i = 0; i <= (uint32_t)stereo && i < nsamp; i++)
            s[i] = (int16_t)pred[i];
        for (; i < nsamp; i++)
        {
            int ch = (i & stereo) ? 1 : 0;
            uint16_t lo, hi;
            lo = small_tree_decode(&b, &tree[ch * 2]);
            hi = small_tree_decode(&b, &tree[ch * 2 + 1]);
            pred[ch] += (int16_t)(lo | (hi << 8));
            s[i] = (int16_t)pred[ch];
        }
    }
    else
    {
        nsamp = unp;
        for (k = stereo; k >= 0; k--)
            pred[k] = bits_get(&b, 8);
        for (i = 0; i <= (uint32_t)stereo && i < nsamp; i++)
            out[i] = (uint8_t)pred[i];
        for (; i < nsamp; i++)
        {
            int ch = (i & stereo) ? 1 : 0;
            pred[ch] += (int8_t)small_tree_decode(&b, &tree[ch]);
            out[i] = (uint8_t)pred[ch];
        }
    }
    free(tree);
    return b.overrun ? -1 : (int32_t)unp;
}

int32_t smkdec_frame_audio(struct SmkDec *d, uint32_t frame, int track,
  uint8_t *out, uint32_t out_size)
{
    uint32_t pos, len;
    const struct SmkDecAudio *a;

    if (track < 0 || track >= SMKDEC_TRACKS || frame >= d->frames)
        return -1;
    a = &d->audio[track];
    if (a->rate == 0)
        return 0;
    pos = audio_chunk(d, frame, track, &len);
    if (pos == 0 || len == 0)
        return 0;
    if (!a->packed) {
        if (len > out_size)
            return -1;
        memcpy(out, d->file + pos, len);
        return len;
    }
    return unpack_audio(a, d->file + pos, len, out, out_size);
}

int32_t smkdec_decode_audio(struct SmkDec *d, int track, uint8_t **p_buf)
{
    uint32_t f, total, cap;
    uint8_t *buf;

    *p_buf = NULL;
    if (track < 0 || track >= SMKDEC_TRACKS || d->audio[track].rate == 0)
        return -1;
    // the unpacked size of each chunk is at its start (packed) or is its length
    total = 0;
    cap = 0;
    for (f = 0; f < d->frames; f++)
    {
        uint32_t len, pos;
        pos = audio_chunk(d, f, track, &len);
        if (pos == 0)
            continue;
        cap += d->audio[track].packed ? (len >= 4 ? rd32(d->file + pos) : 0) : len;
    }
    buf = malloc(cap ? cap : 1);
    if (buf == NULL)
        return -1;
    for (f = 0; f < d->frames; f++)
    {
        int32_t n = smkdec_frame_audio(d, f, track, buf + total, cap - total);
        if (n < 0) {
            free(buf);
            return -1;
        }
        total += n;
    }
    *p_buf = buf;
    return total;
}

/******************************************************************************/
