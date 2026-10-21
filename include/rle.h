#ifndef RLE_H
#define RLE_H

#include "utils.h"
#include <stddef.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------
 * RLE (run-length encoding) used by saveWorld / loadWorld.
 *
 * File layout:
 *   [Header]  magic "MCR1" (4B) | version (1B) | WORLD_CX/Y/Z (3 x u16 LE)
 *   [Records] one record per chunk, in the same order as the chunk array:
 *
 *     u8  flags         bit0 = payload holds raw voxels (RLE did not help)
 *     u16 payloadSize   bytes that follow, little endian
 *     payload           RLE runs as u16 LE:  (count-1) << 4 | blockID
 *                       or CHUNK_SIZE^3 raw bytes when RLE_FLAG_RAW is set
 *
 * The count-1 encoding makes 12 bits represent 1..4096 voxels, so a whole
 * chunk of identical blocks (4096 voxels) fits in a single run. When RLE
 * would not shrink a chunk, the raw voxels are stored instead: a record can
 * therefore never grow past RLE_MAX_RECORD bytes.
 * ---------------------------------------------------------------------- */

#define RLE_MAGIC       "MCR1"
#define RLE_MAGIC_LEN   4
#define RLE_VERSION     1
#define RLE_HEADER_SIZE (RLE_MAGIC_LEN + 1 + 6) /* magic + version + 3 x u16 */

#define RLE_FLAG_RAW    0x01

/* record header: flags byte + u16 payload size */
#define RLE_RECORD_HDR  3

/* worst case payload: the uncompressed voxels of a chunk */
#define RLE_RAW_PAYLOAD (CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE)

/* max bytes rleCompressChunk writes for a single chunk record */
#define RLE_MAX_RECORD  (RLE_RECORD_HDR + RLE_RAW_PAYLOAD)

/* payload size stored in the first bytes of a record header */
#define RLE_PAYLOAD_SIZE(hdr) ((size_t)(hdr)[1] | ((size_t)(hdr)[2] << 8))

/* --- one u16 run: high 12 bits = count-1 (encodes 1..4096), low 4 bits = block ID --- */
#define RLE_RUN(count, id)  ((uint16_t)((((count) - 1) << 4) | ((id) & 0x0F)))
#define RLE_RUN_COUNT(run)  ((size_t)(((run) >> 4) & 0x0FFF) + 1)
#define RLE_RUN_ID(run)     ((u8)((run) & 0x0F))

/* pack the file header (magic, version, world dims) into out.
 * out must hold RLE_HEADER_SIZE bytes. Dimensions are stored little endian
 * so a world built with different sizes is rejected at load time. */
void rleEncodeHeader(unsigned char *out);

/* check a header read from a file against the values this binary expects.
 * Returns false if the file is not ours, from another version, or the
 * world dimensions do not match. */
bool rleCheckHeader(const unsigned char *in);

/* Compress one chunk (RLE_RAW_PAYLOAD voxels) into out.
 * out must hold RLE_MAX_RECORD bytes. Returns the number of bytes written,
 * or 0 if the voxels hold a block ID that does not fit in 4 bits. */
size_t rleCompressChunk(const u8 *voxels, unsigned char *out);

/* Expand a full record (RLE_RECORD_HDR + payload bytes) into out.
 * out must hold RLE_RAW_PAYLOAD voxels. Returns the number of bytes
 * consumed from in, or 0 if the record is malformed. */
size_t rleDecompressChunk(const unsigned char *in, size_t inSize, u8 *out);

#endif
