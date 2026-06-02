#include "render.h"
#include "utils.h"
#include <math.h>   // for INFINITY

/* Main render loop */
void render(Camera *cam, Chunk *world, size_t COLS, size_t ROWS) {
  
  /* terminal chars are not square, x component is smaller than y component, so we need this ratio coefficient */
  float ratio = (float)COLS/(float)ROWS * CHAR_ASPECT; 
  
  /* We go through each char of terminal screen and in every iteration we create a vector in direction of that point. 
   * Then we execute DDA: 
   * - if the ray hit a voxel, we read the ID in the world array and write the char in the buffer.
   * - if not, we just write ' ' in the buffer */
  unsigned char *frameBuffer = malloc(sizeof(unsigned char) * COLS * ROWS);
  if (!frameBuffer) {
    exit(1); 
  }
  u8 *colorBuffer = malloc(sizeof(u8) * COLS * ROWS); 
  if (!colorBuffer) {
    free(frameBuffer); 
    exit(1);
  }; 
  
  for (size_t col = 0; col < COLS; col++) {
    for (size_t row = 0; row < ROWS; row++) {
      size_t idx = row * COLS + col; 
      vec3 rayDir = createRayVector(cam, col, row, COLS, ROWS, ratio); 
      DDAResult dda = digDifAnalysis(cam, world, rayDir);
      
      if (dda.hit) {
        frameBuffer[idx] = computeASCIIChar(dda, cam, rayDir, world, &colorBuffer[idx]);
      } else {
        frameBuffer[idx] = ' '; 
        colorBuffer[idx] = 0xFF; 
      }
    }
  }
  
  /*draw a cross finder to see where are we pointing at */
  size_t centerIdx = (ROWS / 2) * COLS + (COLS / 2); 
  frameBuffer[centerIdx] = '+'; 
  colorBuffer[centerIdx] = 0xFF; 


  /* show the selected block in the bottom left corner of the screen with a colored '#' */
  size_t uiIdx = (ROWS - 2) * COLS + 2;
  frameBuffer[uiIdx] = '[';
  colorBuffer[uiIdx] = 0xFF; /* Reset */

  frameBuffer[uiIdx + 1] = '#';
  colorBuffer[uiIdx + 1] = (cam->selectedBlock << 4) | FACE_TOP;

  frameBuffer[uiIdx + 2] = ']';
  colorBuffer[uiIdx + 2] = 0xFF; /* Reset */

  printBuffer(frameBuffer, colorBuffer, COLS, ROWS);
  free(frameBuffer); 
  free(colorBuffer);
}

/* Two-level Digital Differential Analysis (DDA)
 *
 * Level 1 : walks the ray through the chunk grid (each step = entire chunk, CHUNK_SIZE voxels) 
 * If a chunk has solidCount == 0, it means it's empty. We skip the chunk. 
 * If a chunk is non empty, we drop into Level 2. 
 *
 * Level 2: we are now into a non empty chunk. We do the classic single-voxel DDA, but constrained into the current chunk. 
 * The ray walks voxel per voxel until it either hits a solid block (returns the data of the block) or exits the chunk boundary 
 * (returns to level 1). 
 */
DDAResult digDifAnalysis(Camera *cam, Chunk *world, vec3 rayDir) {
  DDAResult dda = {0}; 
  dda.hit = 0; 
  dda.dist = 1e30f; 

  /* get the ray starting point in map coordinates */
  ivec3 voxel = {
    (int)floorf(cam->pos.x),
    (int)floorf(cam->pos.y),
    (int)floorf(cam->pos.z)
  }; 

  /* Calculate the step direction: +1 or -1 */
  ivec3 step = {
    ((rayDir.x > 0) << 1) - 1,
    ((rayDir.y > 0) << 1) - 1,
    ((rayDir.z > 0) << 1) - 1,
  };

  /* delta distance to cross one voxel on each axis. Protect against division by zero. */
  vec3 tDelta;
  tDelta.x = (rayDir.x == 0.0f) ? INFINITY : fabsf(1.0f / rayDir.x);
  tDelta.y = (rayDir.y == 0.0f) ? INFINITY : fabsf(1.0f / rayDir.y);
  tDelta.z = (rayDir.z == 0.0f) ? INFINITY : fabsf(1.0f / rayDir.z);
  
  /* tMax = distance to the next voxel boundary along each axis */
  float tmx, tmy, tmz; 
  tmx = (step.x > 0) ? (floorf(cam->pos.x) + 1.0f - cam->pos.x) * tDelta.x 
                     : (cam->pos.x - floorf(cam->pos.x)) * tDelta.x;  
  tmy = (step.y > 0) ? (floorf(cam->pos.y) + 1.0f - cam->pos.y) * tDelta.y 
                     : (cam->pos.y - floorf(cam->pos.y)) * tDelta.y; 
  tmz = (step.z > 0) ? (floorf(cam->pos.z) + 1.0f - cam->pos.z) * tDelta.z 
                     : (cam->pos.z - floorf(cam->pos.z)) * tDelta.z; 
  vec3 tMax = {tmx, tmy, tmz};

  int hitFace = -1;   /* 0 = X, 1 = Y, 2 = Z */
  int totalSteps = 0; /* count both chunk level and voxel‑level steps */

  /* Check if the starting point is already inside a solid block */
  u8 startBlock = getBlock(world, voxel.x, voxel.y, voxel.z);
  if (startBlock != AIR) {
    dda.hit = 1;
    dda.voxel = voxel;
    dda.hitFace = 0;           /* default to X face */
    dda.step = step;
    dda.dist = 0.0f;           /* distance zero because we start inside */
    return dda;
  }

  /* Hierarchical loop */
  while (totalSteps < CHUNK_DDA_STEPS * CHUNK_SIZE) {
    /* Current chunk coordinates */
    int cx = voxel.x / CHUNK_SIZE; 
    int cy = voxel.y / CHUNK_SIZE; 
    int cz = voxel.z / CHUNK_SIZE; 

    /* World boundaries check */
    if (cx < 0 || cy < 0 || cz < 0 || cx >= WORLD_CX || cy >= WORLD_CY || cz >= WORLD_CZ)
      break; 

    size_t chunkIdx = GET_CHUNK_INDEX(cx, cy, cz); 
    Chunk *currentChunk = &world[chunkIdx];

    /* Level 1: check if the chunk is empty */
    if (currentChunk->solidCount > 0) {
      /* Level 2: voxel‑by‑voxel DDA inside this chunk */
      while (1) {
        /* Check if we are still inside the same chunk */
        if ((voxel.x / CHUNK_SIZE) != cx || 
            (voxel.y / CHUNK_SIZE) != cy || 
            (voxel.z / CHUNK_SIZE) != cz) 
          break; 
      
        u8 blockID = getBlock(world, voxel.x, voxel.y, voxel.z); 
        if (blockID != AIR) {
          dda.hit = 1; 
          dda.voxel = voxel; 
          dda.hitFace = hitFace; 
          dda.step = step; 
          /* distance is the t value at which the intersection occurred */
          if (hitFace == 0) dda.dist = tMax.x - tDelta.x;
          else if (hitFace == 1) dda.dist = tMax.y - tDelta.y;
          else dda.dist = tMax.z - tDelta.z;
          return dda; 
        }

        /* Standard DDA step to the next voxel */
        if (tMax.x < tMax.y && tMax.x < tMax.z) {
          voxel.x += step.x; hitFace = 0; tMax.x += tDelta.x; 
        } else if (tMax.y < tMax.z) {
          voxel.y += step.y; hitFace = 1; tMax.y += tDelta.y; 
        } else {
          voxel.z += step.z; hitFace = 2; tMax.z += tDelta.z; 
        }
        totalSteps++;
      }
    } else {
      /* Chunk is empty: skip it and go to the next chunk using DDA */
      while ((voxel.x / CHUNK_SIZE) == cx && 
             (voxel.y / CHUNK_SIZE) == cy && 
             (voxel.z / CHUNK_SIZE) == cz) {
        if (tMax.x < tMax.y && tMax.x < tMax.z) {
          voxel.x += step.x; hitFace = 0; tMax.x += tDelta.x; 
        } else if (tMax.y < tMax.z) {
          voxel.y += step.y; hitFace = 1; tMax.y += tDelta.y; 
        } else {
          voxel.z += step.z; hitFace = 2; tMax.z += tDelta.z; 
        }
        totalSteps++;
        
        /* Safety break for extreme boundaries */
        if (voxel.x < -1 || voxel.y < -1 || voxel.z < -1 ||
            voxel.x > WORLD_SIZE_X || voxel.y > WORLD_SIZE_Y || voxel.z > WORLD_SIZE_Z)
          break; 
      }
    }
  }
  
  return dda; 
}

vec3 createRayVector(Camera *cam, size_t col, size_t row, size_t COLS, size_t ROWS, float ratio) { 
    float ndcX = ((float)col + 0.5f) / (float)COLS; 
    float ndcY = ((float)row + 0.5f) / (float)ROWS; 

    float screenX = 2.0f * ndcX - 1.0f;  
    float screenY = 1.0f - 2.0f * ndcY;

    /* Apply aspect ratio correction for non‑square terminal characters */
    screenX *= ratio; 

    float rx = cam->right.x,   ry = cam->right.y,   rz = cam->right.z;
    float ux = cam->up.x,      uy = cam->up.y,      uz = cam->up.z;
    float fx = cam->forward.x, fy = cam->forward.y, fz = cam->forward.z;

    vec3 rayDir = {
      (screenX * rx) + (screenY * ux) + (FOCAL_LENGTH * fx),
      (screenX * ry) + (screenY * uy) + (FOCAL_LENGTH * fy),
      (screenX * rz) + (screenY * uz) + (FOCAL_LENGTH * fz)
    }; 
    
    return vec3Normalize(rayDir); 
}

/* Compute the ASCII char for a hit voxel and pack blockID + faceIndex into *outColor */
unsigned char computeASCIIChar(DDAResult dda, Camera *cam, vec3 rayDir, Chunk *world, u8 *outColor) { 
  u8 blockID = getBlock(world, dda.voxel.x, dda.voxel.y, dda.voxel.z); 

  /* Calculate the position of the voxel with P = O + D * t 
   * P = point of the voxel that we target 
   * O = origin of the ray 
   * D = direction of the ray 
   * t = distance length */
  float hitX = cam->pos.x + rayDir.x * dda.dist;
  float hitY = cam->pos.y + rayDir.y * dda.dist; 
  float hitZ = cam->pos.z + rayDir.z * dda.dist;

  /* Local coordinates inside the voxel */
  float fx = hitX - floorf(hitX); 
  float fy = hitY - floorf(hitY); 
  float fz = hitZ - floorf(hitZ);

  float edge = 0.01f; 
  bool isEdge = false; 
  
  /* check if it's a border */
  if (dda.hitFace == 1) {
    if (fx < edge || fx > 1.0f - edge || fz < edge || fz > 1.0f - edge) isEdge = true; 
  } else if (dda.hitFace == 0) {
    if (fy < edge || fy > 1.0f - edge || fz < edge || fz > 1.0f - edge) isEdge = true; 
  } else {
    if (fx < edge || fx > 1.0f - edge || fy < edge || fy > 1.0f - edge) isEdge = true; 
  }

  char shade;  
  u8 faceIndex = 0;
  
  if (isEdge) {
    shade = ' ';
    if (faceIndex == 0) dda.hitFace = FACE_SIDE_X; 
    else if (faceIndex == 1) dda.hitFace = FACE_TOP; 
    else if (faceIndex == 2) dda.hitFace = FACE_SIDE_Z; 
    *outColor = (blockID << 4) | (faceIndex & 0x0F);
    return shade; 
  }

  /* Shading for the inside of the faces */
  if (dda.hitFace == 1) {
    if (dda.step.y < 0) { 
      shade = '#';                /* Top face */
      faceIndex = FACE_TOP;
    } else {
      shade = '.';                /* Bottom face */ 
      faceIndex = FACE_BOTTOM;
    }
  } else if (dda.hitFace == 0) {
    shade = '|';                  /* side x face */ 
    faceIndex = FACE_SIDE_X;
  } else {
    shade = ':';                  /* side z face */ 
    faceIndex = FACE_SIDE_Z;
  }

  
  *outColor = (blockID << 4) | (faceIndex & 0x0F); 
  return shade; 
}

/* Build the full output string with ANSI escape sequences and do a single write */
bool printBuffer(unsigned char *fb, u8 *colorBuffer, size_t COLS, size_t ROWS) {
  size_t fbSize      = COLS * ROWS;
  size_t outCapacity = fbSize * 16; /* generous headroom for escape sequences */
  char  *outputBuffer = malloc(outCapacity);
  if (!outputBuffer) return false;
 
  size_t pos          = 0;
  u8     currentColor = 0xFE; /* impossible value, forces the first emit */
 
  for (size_t i = 0; i < fbSize; i++) {
    u8 packed = colorBuffer[i];
 
    if (packed != currentColor) {
      const char *esc;
      if (packed == 0xFF) {
        esc = ANSI_COLOR_RESET;
      } else {
        u8 blockID   = (packed >> 4) & 0x0F;
        u8 faceIndex =  packed       & 0x0F;
        if (blockID < BLOCK_COLORS_COUNT && faceIndex < FACE_COUNT) {
          esc = BLOCK_COLORS[blockID][faceIndex];
        } else {
          esc = ANSI_COLOR_RESET;
        }
      }
      size_t escLen = strlen(esc);
      memcpy(outputBuffer + pos, esc, escLen);
      pos += escLen;
      currentColor = packed;
    }
    outputBuffer[pos++] = fb[i];
  }
 
  memcpy(outputBuffer + pos, ANSI_COLOR_RESET, strlen(ANSI_COLOR_RESET));
  pos += strlen(ANSI_COLOR_RESET);
 
  bool ok = write(STDOUT_FILENO, outputBuffer, pos) == (ssize_t)pos;
  free(outputBuffer);
  return ok;
}

TargetResult getTargetBlock(Camera *cam, Chunk *world, float maxDist) {
	TargetResult res = {0}; 

	DDAResult dda = digDifAnalysis(cam, world, cam->forward); 

	if (dda.hit && dda.dist <= maxDist) {
		res.hit = 1; 
		res.pos = dda.voxel; 

		res.adjacent = dda.voxel; 
		if (dda.hitFace == 0) res.adjacent.x -= dda.step.x; 
		if (dda.hitFace == 1) res.adjacent.y -= dda.step.y; 
		if (dda.hitFace == 2) res.adjacent.z -= dda.step.z; 
	}
	
	return res; 
}

