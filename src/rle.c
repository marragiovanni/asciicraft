#include "rle.h"
#include <string.h>

/* -------------------------------------------------------------------------
 * Little endian helpers: the save file must not depend on the CPU it
 * was written on, so every u16 is stored byte by byte.
 * ---------------------------------------------------------------------- */

static void putU16(unsigned char *dst, uint16_t value) {
  dst[0] = (unsigned char)(value & 0xFF);
  dst[1] = (unsigned char)((value >> 8) & 0xFF);
}

static uint16_t getU16(const unsigned char *src) {
  return (uint16_t)(src[0] | (src[1] << 8));
}

/* -------------------------------------------------------------------------
 * File header
 * ---------------------------------------------------------------------- */

void rleEncodeHeader(unsigned char *out) {
  memcpy(out, RLE_MAGIC, RLE_MAGIC_LEN);
  out[RLE_MAGIC_LEN] = RLE_VERSION;

  /* world dimensions: a save from a world with different sizes
   * must be rejected instead of being loaded as garbage */
  putU16(out + RLE_MAGIC_LEN + 1, WORLD_CX);
  putU16(out + RLE_MAGIC_LEN + 3, WORLD_CY);
  putU16(out + RLE_MAGIC_LEN + 5, WORLD_CZ);
}

bool rleCheckHeader(const unsigned char *in) {
  return memcmp(in, RLE_MAGIC, RLE_MAGIC_LEN) == 0 &&
         in[RLE_MAGIC_LEN] == RLE_VERSION &&
         getU16(in + RLE_MAGIC_LEN + 1) == WORLD_CX &&
         getU16(in + RLE_MAGIC_LEN + 3) == WORLD_CY &&
         getU16(in + RLE_MAGIC_LEN + 5) == WORLD_CZ;
}

/* -------------------------------------------------------------------------
 * Compression: one pass over the chunk, pack equal voxels into u16 runs
 * ---------------------------------------------------------------------- */

size_t rleCompressChunk(const u8 *voxels, unsigned char *out) {
  unsigned char *payload = out + RLE_RECORD_HDR;
  size_t payloadSize = 0;
  size_t i = 0;
  bool raw = false;

  /* walk the chunk and extend the current run while the id stays equal */
  while (i < RLE_RAW_PAYLOAD) {
    u8 id = voxels[i];
    if (id > 0x0F) return 0; /* block id does not fit in 4 bits */

    size_t count = 1;
    while (i + count < RLE_RAW_PAYLOAD && voxels[i + count] == id)
      count++;
    i += count;

    /* runs would fill the raw size: RLE does not shrink this chunk,
     * stop now and store the voxels raw instead (never expands) */
    if (payloadSize + 2 >= RLE_RAW_PAYLOAD) {
      raw = true;
      break;
    }

    putU16(payload + payloadSize, RLE_RUN(count, id));
    payloadSize += 2;
  }

  if (raw) {
    out[0] = RLE_FLAG_RAW;
    putU16(out + 1, (uint16_t)RLE_RAW_PAYLOAD);
    memcpy(payload, voxels, RLE_RAW_PAYLOAD);
    return RLE_MAX_RECORD;
  }

  out[0] = 0;
  putU16(out + 1, (uint16_t)payloadSize);
  return RLE_RECORD_HDR + payloadSize;
}

/* -------------------------------------------------------------------------
 * Decompression: validate first, expand second. A malformed record is
 * rejected (0) instead of writing partial garbage into the world.
 * ---------------------------------------------------------------------- */

size_t rleDecompressChunk(const unsigned char *in, size_t inSize, u8 *out) {
  if (inSize < RLE_RECORD_HDR) return 0;

  u8 flags = in[0];
  size_t payloadSize = RLE_PAYLOAD_SIZE(in);

  if (payloadSize > RLE_RAW_PAYLOAD) return 0;          /* impossible: corrupt */
  if (inSize != RLE_RECORD_HDR + payloadSize) return 0; /* truncated record   */
  if (flags & ~RLE_FLAG_RAW) return 0;                  /* unknown flags      */

  const unsigned char *payload = in + RLE_RECORD_HDR;

  if (flags & RLE_FLAG_RAW) {
    if (payloadSize != RLE_RAW_PAYLOAD) return 0;
    memcpy(out, payload, RLE_RAW_PAYLOAD);
    return inSize;
  }

  if (payloadSize % 2 != 0) return 0; /* runs are u16 */

  /* expand every run, refusing anything that would overflow the chunk */
  size_t written = 0;
  for (size_t pos = 0; pos < payloadSize; pos += 2) {
    uint16_t run = getU16(payload + pos);
    size_t count = RLE_RUN_COUNT(run);

    if (written + count > RLE_RAW_PAYLOAD) return 0;
    memset(out + written, RLE_RUN_ID(run), count);
    written += count;
  }

  if (written != RLE_RAW_PAYLOAD) return 0; /* runs do not fill a chunk */
  return inSize;
}
