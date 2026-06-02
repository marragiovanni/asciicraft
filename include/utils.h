#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>     /* keep it */
#include <unistd.h>
#include <math.h>

/* --- ANSI COLORS --- */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GRAY    "\x1b[90m"
#define ANSI_COLOR_RESET   "\x1b[0m"

/* greens for GRASS shading: top (bright) → side_x (mid) → side_z (mid-low) → bottom (dark) */
#define ANSI_GREEN_BRIGHT  "\x1b[92m"   /* top face      */
#define ANSI_GREEN_MID     "\x1b[32m"   /* side x face   */
#define ANSI_GREEN_MIDLOW  "\x1b[2;32m" /* side z face   */
#define ANSI_GREEN_DARK    "\x1b[2;32m" /* bottom face   */

 
/* grays for COBBLESTONE shading: top (light) → side_x (mid) → side_z (mid-low) → bottom (dark) */
#define ANSI_GRAY_BRIGHT   "\x1b[37m"   /* top face      */
#define ANSI_GRAY_MID      "\x1b[90m"   /* side x face   */
#define ANSI_GRAY_MIDLOW   "\x1b[2;37m" /* side z face   */
#define ANSI_GRAY_DARK     "\x1b[2;37m" /* bottom face   */
 
/* yellows for SAND shading: top (bright) → side_x (mid) → side_z (mid-low) → bottom (dark) */
#define ANSI_SAND_BRIGHT   "\x1b[93m"   /* top face      */
#define ANSI_SAND_MID      "\x1b[33m"   /* side x face   */
#define ANSI_SAND_MIDLOW   "\x1b[2;33m" /* side z face   */
#define ANSI_SAND_DARK     "\x1b[2;33m" /* bottom face   */


/* --- FACE INDICES for BLOCK_COLORS second dimension --- 
 * Must match the face shading logic in computeASCIIChar:
 * 0 = top    (hitFace==1 && step.y < 0)
 * 1 = bottom (hitFace==1 && step.y > 0)
 * 2 = side_x (hitFace==0)
 * 3 = side_z (hitFace==2)                */
#define FACE_TOP    0
#define FACE_BOTTOM 1
#define FACE_SIDE_X 2
#define FACE_SIDE_Z 3
#define FACE_COUNT  4

extern const char* BLOCK_COLORS[][FACE_COUNT]; 


#define BLOCK_COLORS_COUNT 3

typedef uint8_t u8; 
typedef struct {
  float x; 
  float y;  
  float z; 
} vec3; 

typedef struct {
  int x; 
  int y; 
  int z; 
} ivec3; 

#define CHUNK_SIZE 16 // number of voxels inside a chunk (dimension of a chunk) 
#define WORLD_CX 16 // number of chunks on the x axis    (width of the world)  
#define WORLD_CY 4  // number of chunks on the y axis    (height of the world) 
#define WORLD_CZ 16 // number of chunks on the z axis    (depth of the  world)
#define WORLD_SIZE_X (WORLD_CX * CHUNK_SIZE) 
#define WORLD_SIZE_Y (WORLD_CY * CHUNK_SIZE)
#define WORLD_SIZE_Z (WORLD_CZ * CHUNK_SIZE)


/* get the mono-dimensional index of a voxel inside a single chunk using local coordinates (0...CHUNK_SIZE-1) */
#define GET_BLOCK_INDEX(lx, ly, lz) ((lz) * (CHUNK_SIZE) * (CHUNK_SIZE) + (ly) * (CHUNK_SIZE) + (lx)) 
/* get the mono-dimensional index of a chunk inside the world array using the three-components system */
#define GET_CHUNK_INDEX(cx, cy, cz) ((cz) * (WORLD_CY) * (WORLD_CX) + (cy) * (WORLD_CX) + (cx))

#define DEG2RAD(deg) ((deg) * M_PI / 180.0f)

/* --- IDs --- */
typedef enum {
  AIR = 0,
  GRASS = 1,
  COBBLESTONE = 2,
  SAND = 3,
} BlockID;

/* --- Chunk --- */
typedef struct {
  u8 voxels[CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE]; 
  int solidCount; /* number of solid blocks in the chunk */
} Chunk;


typedef struct {
	int hit; 
	ivec3 pos; 
	ivec3 adjacent; 
} TargetResult; 



/* --- Block access --- 
 * These functions handle the conversion from absolute world coordinates 
 * to chunk index + local voxel index automatically. 
 * getBlock returns AIR (0) for out-of-bounds coordinates */
u8 getBlock(Chunk *world, int x, int y, int z); 
void setBlock(Chunk *world, int x, int y, int z, u8 blockID); 

/* ---  math tools --- */
vec3 vec3Normalize(vec3 v); 
vec3 vec3Cross(vec3 v, vec3 w); 
float vec3Dot(vec3 v, vec3 w);
float minTraj(float x, float y, float z); 

#endif 
