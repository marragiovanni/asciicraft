#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "world.h"
#include "noise.h"
#include "utils.h"
#include "rle.h"

/* -------------------------------------------------------------------------
 * World initialisation  (procedural terrain generation)
 * ---------------------------------------------------------------------- */

void initWorld(Chunk *world) {
  size_t totalChunks = WORLD_CX * WORLD_CY * WORLD_CZ;

  /* start from a known state: every voxel is air and solidCount is 0.
   * setBlock adjusts solidCount incrementally, so it must begin valid —
   * malloc'd memory is not guaranteed to be zeroed. */
  memset(world, 0, sizeof(Chunk) * totalChunks);

  for (size_t x = 0; x < WORLD_SIZE_X; x++) {
    for (size_t z = 0; z < WORLD_SIZE_Z; z++) {
      float h = perlin2D(x * 0.025f, z * 0.025f);
      int height = (int)(h * 12.0f) + 24; /* range between 12 and 36 */

      float b = perlin2D(x * 0.005f, z * 0.005f);
      int biome;
      if (b < -0.2f)     biome = 0; /* sand     */
      else if (b < 0.3f) biome = 1; /* lowland  */
      else               biome = 2; /* mountains */

      for (int y = 0; y < WORLD_SIZE_Y; y++) {
        u8 block = AIR;

        if (y <= height) {
          if (y == height) {
            if (biome == 0)      block = SAND;
            else if (biome == 1) block = GRASS;
            else                 block = COBBLESTONE;
          } else if (y > height - 3) {
            block = GRASS;
          } else {
            block = COBBLESTONE;
          }
        }
        setBlock(world, x, y, z, block);
      }
    }
  }
}

/* -------------------------------------------------------------------------
 * Save / Load  (the world is stored as one RLE record per chunk, see rle.h)
 * ---------------------------------------------------------------------- */

bool saveWorld(const Chunk *world) {
  size_t totalChunks = WORLD_CX * WORLD_CY * WORLD_CZ;
  unsigned char header[RLE_HEADER_SIZE];
  bool ok = true;

  FILE *f = fopen(WORLD_SAVE_FILE, "wb");
  if (!f) {
    perror("saveWorld: fopen");
    return false;
  }

  rleEncodeHeader(header);
  ok = fwrite(header, 1, RLE_HEADER_SIZE, f) == RLE_HEADER_SIZE;

  /* one compressed record per chunk, in the same order as the chunk array */
  for (size_t i = 0; i < totalChunks && ok; i++) {
    unsigned char record[RLE_MAX_RECORD];
    size_t bytes = rleCompressChunk(world[i].voxels, record);
    ok = bytes > 0 && fwrite(record, 1, bytes, f) == bytes;
  }

  fclose(f);

  if (!ok) {
    perror("saveWorld: fwrite");
    return false;
  }

  return true;
}

bool loadWorld(Chunk *world) {
  size_t totalChunks = WORLD_CX * WORLD_CY * WORLD_CZ;
  unsigned char header[RLE_HEADER_SIZE];
  bool ok = true;

  FILE *f = fopen(WORLD_SAVE_FILE, "rb");
  if (!f) {
    /* Not an error if the file simply doesn't exist yet */
    return false;
  }

  ok = fread(header, 1, RLE_HEADER_SIZE, f) == RLE_HEADER_SIZE &&
       rleCheckHeader(header);

  /* decode into a copy first: a corrupt file must leave the world intact */
  Chunk *loaded = ok ? malloc(sizeof(Chunk) * totalChunks) : NULL;
  if (ok && !loaded) ok = false;

  for (size_t i = 0; i < totalChunks && ok; i++) {
    unsigned char record[RLE_MAX_RECORD];

    /* read the record header first so we know how many payload bytes follow */
    if (fread(record, 1, RLE_RECORD_HDR, f) != RLE_RECORD_HDR) { ok = false; break; }

    size_t payloadSize = RLE_PAYLOAD_SIZE(record);
    if (payloadSize > RLE_RAW_PAYLOAD) { ok = false; break; } /* corrupt size */

    if (fread(record + RLE_RECORD_HDR, 1, payloadSize, f) != payloadSize ||
        rleDecompressChunk(record, RLE_RECORD_HDR + payloadSize, loaded[i].voxels) == 0) {
      ok = false;
      break;
    }

    /* solidCount is not stored: rebuild it from the voxels we just decoded */
    loaded[i].solidCount = 0;
    for (size_t v = 0; v < sizeof(loaded[i].voxels); v++)
      if (loaded[i].voxels[v] != AIR) loaded[i].solidCount++;
  }

  fclose(f);

  if (!ok) {
    fprintf(stderr, "\r\n[world] " WORLD_SAVE_FILE " is corrupt or was saved with another version\r\n");
    free(loaded);
    return false;
  }

  memcpy(world, loaded, sizeof(Chunk) * totalChunks);
  free(loaded);
  return true;
}
