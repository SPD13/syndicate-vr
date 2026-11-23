/******************************************************************************/
// Bullfrog Engine Smacker Playback Library - for use to remake classic games
// like Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file smkdec.h
 *     Header file for smkdec.c.
 * @par Purpose:
 *     Smacker video decoder in portable C.
 * @par Comment:
 *     Written from the Smacker file format description, as a replacement
 *     of the original x86 asm decoder. Supports what Smacker version 2
 *     files use: palette records, video blocks (mono, full, void, solid)
 *     and audio tracks (raw or packed, 8 or 16 bit, mono or stereo).
 * @author   SyndicateVR decomp agent
 * @date     22 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef BFSMACKLIB_SMKDEC_H_
#define BFSMACKLIB_SMKDEC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SMKDEC_TRACKS 7

struct SmkDecTree; /* internal */

struct SmkDecAudio {
    uint32_t rate;      /**< Samples per second, 0 if the track is absent */
    uint8_t channels;   /**< 1 or 2 */
    uint8_t bits;       /**< 8 or 16 */
    uint8_t packed;     /**< Huffman/DPCM packed */
};

struct SmkDec {
    uint32_t width;
    uint32_t height;
    uint32_t frames;
    uint32_t flags;         /**< Header flags: 1 ring frame, 2 Y interlaced, 4 Y doubled */
    uint32_t us_per_frame;  /**< Frame duration in microseconds */
    uint8_t version;        /**< 2 or 4 */
    struct SmkDecAudio audio[SMKDEC_TRACKS];
    /** Current frame image, width*height palette indices */
    uint8_t *image;
    /** Current palette, 256 RGB entries of 6-bit values (0..63) */
    uint8_t palette[768];
    /** Set when the last decoded frame changed the palette */
    int palette_changed;
    /** Number of the next frame smkdec_next_frame() decodes */
    uint32_t next_frame;
    /* internal */
    uint8_t *file;
    uint32_t file_size;
    uint32_t *frame_size;
    uint8_t *frame_type;
    uint32_t *frame_offs;
    struct SmkDecTree *tree[4];
};

/** Opens a Smacker file; reads it whole. Returns NULL on failure. */
struct SmkDec *smkdec_open(const char *fname);

/** Opens Smacker data in memory; takes ownership of buf (freed on close). */
struct SmkDec *smkdec_open_mem(uint8_t *buf, uint32_t size);

void smkdec_close(struct SmkDec *p_dec);

/** Decodes the next frame into image and palette. Returns 0 on success,
 * -1 on error or after the last frame. */
int smkdec_next_frame(struct SmkDec *p_dec);

/** Decodes the whole audio track into a newly allocated buffer (to be
 * freed with free()), interleaved samples in the track format. Returns
 * the size in bytes, or -1 on error. */
int32_t smkdec_decode_audio(struct SmkDec *p_dec, int track, uint8_t **p_buf);

/** Decodes the audio of one frame of a track (packed or raw) into out,
 * which must hold the unpacked size. Returns bytes written, 0 if the frame
 * has no data for that track, -1 on error. */
int32_t smkdec_frame_audio(struct SmkDec *p_dec, uint32_t frame, int track,
  uint8_t *out, uint32_t out_size);

/** Raw audio chunk of a frame and track (after its length field); for
 * packed tracks it starts with the unpacked size. Returns its length, 0 if
 * absent, -1 on error. */
int32_t smkdec_audio_chunk(struct SmkDec *p_dec, uint32_t frame, int track,
  const uint8_t **p_data);

#ifdef __cplusplus
};
#endif

#endif // BFSMACKLIB_SMKDEC_H_
/******************************************************************************/
