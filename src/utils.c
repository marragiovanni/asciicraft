#include "utils.h"

const char* BLOCK_COLORS[][FACE_COUNT] = {
  { ANSI_COLOR_RESET, ANSI_COLOR_RESET, ANSI_COLOR_RESET, ANSI_COLOR_RESET }, // AIR 
  { ANSI_GREEN_BRIGHT, ANSI_GREEN_DARK, ANSI_GREEN_MID, ANSI_GREEN_MIDLOW  }, // GRASS
  { ANSI_GRAY_BRIGHT, ANSI_GRAY_DARK, ANSI_GRAY_MID, ANSI_GRAY_MIDLOW      }, // COBBLESTONE
  { ANSI_SAND_BRIGHT, ANSI_SAND_DARK, ANSI_SAND_MID, ANSI_SAND_MIDLOW      }  // SAND
};

/* get the magnitude L of a vector and normalize each component dividing it for L */
vec3 vec3Normalize(vec3 v) {
  float L = sqrtf((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
  v.x = v.x / L; 
  v.y = v.y / L; 
  v.z = v.z / L; 

  return v;
}

// Cross product v and w : 
vec3 vec3Cross(vec3 v, vec3 w) {
  return (vec3) {
    (v.y * w.z) - (v.z * w.y), 
    (v.z * w.x) - (v.x * w.z),
    (v.x * w.y) - (v.y * w.x) 
  };   
}

float vec3Dot(vec3 v, vec3 w) {
  float dot = (v.x * w.x) + (v.y * w.y) + (v.z * w.z);
  return dot; 
}

float minTraj(float x, float y, float z) {
  if (x < y && x < z) return x; 
  else if (y < z) return y; 
  else return z; 
}


u8 getBlock(Chunk *world, int x, int y, int z) {
  /* Validation of the coordinates */
  if (x < 0 || x >= WORLD_SIZE_X || 
      y < 0 || y >= WORLD_SIZE_Y || 
      z < 0 || z >= WORLD_SIZE_Z ) return AIR; 

  /* Transform the global coordinates into local coordinates */
  int cx = x / CHUNK_SIZE; 
  int cy = y / CHUNK_SIZE; 
  int cz = z / CHUNK_SIZE; 

  int lx = x % CHUNK_SIZE; 
  int ly = y % CHUNK_SIZE; 
  int lz = z % CHUNK_SIZE;

  size_t chunkIdx = GET_CHUNK_INDEX(cx, cy, cz);
  Chunk *chunk = &world[chunkIdx];

  return chunk->voxels[GET_BLOCK_INDEX(lx, ly, lz)];     
}

void setBlock(Chunk *world, int x, int y, int z, u8 blockID) {
  if (x < 0 || x >= WORLD_SIZE_X || 
      y < 0 || y >= WORLD_SIZE_Y || 
      z < 0 || z >= WORLD_SIZE_Z ) return ;

  /* local coordinates of chunk from global coordinates */ 
  int cx = x / CHUNK_SIZE; 
  int cy = y / CHUNK_SIZE; 
  int cz = z / CHUNK_SIZE; 
  
  /* local coordinates of voxel from global coordinates */
  int lx = x % CHUNK_SIZE; 
  int ly = y % CHUNK_SIZE; 
  int lz = z % CHUNK_SIZE;

  size_t chunkIdx = GET_CHUNK_INDEX(cx, cy, cz);
  Chunk *chunk = &world[chunkIdx];

  size_t blockIdx = GET_BLOCK_INDEX(lx, ly, lz);
  u8 old = chunk->voxels[blockIdx]; 
  if (old == blockID) return; 
  
  /* if we are adding or removing a solid block we need to update the solidCount */
  if (blockID != AIR && old == AIR) chunk->solidCount++; 
  if (blockID == AIR && old != AIR) chunk->solidCount--; 
  
  /* update the chunk */
  chunk->voxels[blockIdx] = blockID;
}

