#ifndef WORLD_H
#define WORLD_H

#include "utils.h"
#include <stdbool.h>

#define WORLD_SAVE_FILE "world.sav"

/* Fill the world with procedurally generated terrain. */
void initWorld(Chunk *world);

/* RLE-compress every chunk and write the world to WORLD_SAVE_FILE.
 * Returns true on success, false on I/O error. */
bool saveWorld(const Chunk *world);

/* Read WORLD_SAVE_FILE (header + RLE records) back into *world and rebuild
 * solidCount. Returns true on success, false if the file is missing,
 * corrupt, or was saved with another version / world size. The world is
 * left untouched when loading fails. */
bool loadWorld(Chunk *world);

#endif
