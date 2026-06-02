#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "camera.h"

#define CHAR_ASPECT 0.5f

/* Maximum number of chunk steps (1st level of DDA) 
 * Each step skips an entire CHUNK_SIZE worth of voxels, so 
 * this covers CHUNK_DDA_STEPS * CHUNK_SIZE voxels along any axis */
#define CHUNK_DDA_STEPS 64

/* Maximum number of voxel steps (2nd level of DDA) 
 * Each step skip a voxel and go to the nearest next, all inside a single chunk. 
 * CHUNK_SIZE * 2 is enough to cross any chunk even on a diagonal ray */
#define VOXEL_DDA_STEPS (CHUNK_SIZE * 2) 

/* This is the value that DDA return, it has all the data to render the voxel in the correct way  */
typedef struct {
  int hit;          /* 1 = hit / 0 = no hit */
  ivec3 voxel;      /* voxel coordinates    */
  int hitFace;      /* 0=x , 1=y , 2=z      */
  ivec3 step;       /* distinguish -X from +X ecc...  */
  float dist;       /* distance of ray      */ 
} DDAResult; 


/* We get terminal dimension. For each char of the terminal (col x row), we create a normalized vector that is directed to the char .
 * Then we check the position in the array world and we choose which ASCII char to render based on the ID value of the block in that position */

/* render one frame: cast one ray for each terminal character, fill the framebuffer and colorBuffer, 
 * then flush both with printBuffer.  */
void render(Camera *cam, Chunk *world, size_t COLS, size_t ROWS);
 
/* build the normalised ray direction for terminal character (col, row) */
vec3 createRayVector(Camera *cam, size_t col, size_t row,
                     size_t COLS, size_t ROWS, float aspectRatio);
 
/* Two-level DDA:
 *   level 1 - walk chunk by chunk, skip empty chunks (solidCount==0)
 *   level 2 - walk voxel by voxel inside non-empty chunks           */
DDAResult digDifAnalysis(Camera *cam, Chunk *world, vec3 rayDir);
 
/* given a hit DDAResult, choose the ASCII character and pack blockID + faceIndex into *outColor for printBuffer */
unsigned char computeASCIIChar(DDAResult dda, Camera *cam, vec3 rayDir, Chunk *world, u8 *outColor);

/* function to calculate the target block to destroy or set */
TargetResult getTargetBlock(Camera *cam, Chunk *world, float maxDist); 


/* assemble the final output string with ANSI escapes and write it to stdout in a single write() call to avoid flickering */
bool printBuffer(unsigned char *fb, u8 *colorBuffer, size_t COLS, size_t ROWS);


#endif 
