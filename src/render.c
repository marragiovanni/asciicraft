#include "render.h"
#include "utils.h"
#include <math.h>   // for INFINITY

/* Persistent buffers that resize automatically when the terminal window grows */
static unsigned char *g_frameBuffer    = NULL;
static u8            *g_colorBuffer    = NULL;
static char          *g_outputBuffer   = NULL;
static size_t         g_bufferCapacity = 0;

void renderCleanup(void) {
  free(g_frameBuffer);
  free(g_colorBuffer);
  free(g_outputBuffer);
  g_frameBuffer    = NULL;
  g_colorBuffer    = NULL;
  g_outputBuffer   = NULL;
  g_bufferCapacity = 0;
}

static bool ensureBuffers(size_t cells) {
  if (cells <= g_bufferCapacity && g_frameBuffer && g_colorBuffer && g_outputBuffer) {
    return true;
  }

  unsigned char *newFb  = realloc(g_frameBuffer, cells * sizeof(unsigned char));
  u8            *newCb  = realloc(g_colorBuffer, cells * sizeof(u8));
  char          *newOut = realloc(g_outputBuffer, cells * 16);
  
  /* if even one of the three realloc failed, we still need to update the global buffers with the pointers
   * that got a succesfull reallocation. Doing this we can call the clean up and free all the buffers 
   * in the correct way and close the program avoiding memory leaks in the system */
  if (!newFb || !newCb || !newOut) {
    if (newFb)  g_frameBuffer  = newFb;
    if (newCb)  g_colorBuffer  = newCb;
    if (newOut) g_outputBuffer = newOut;
    renderCleanup();
    return false;
  }

  g_frameBuffer    = newFb;
  g_colorBuffer    = newCb;
  g_outputBuffer   = newOut;
  g_bufferCapacity = cells;
  return true;
}

/* Main render loop */
bool render(Camera *cam, Chunk *world, size_t COLS, size_t ROWS) {
  
  /* terminal chars are not square, x component is smaller than y component, so we need this ratio coefficient */
  float ratio = (float)COLS/(float)ROWS * CHAR_ASPECT; 
  
  /* Ensure static persistent buffers have enough capacity */
  if (!ensureBuffers(COLS * ROWS)) {
    return false;  
  }
  
  for (size_t row = 0; row < ROWS; row++) {
    for (size_t col = 0; col < COLS; col++) {
      size_t idx = row * COLS + col; 
      vec3 rayDir = createRayVector(cam, col, row, COLS, ROWS, ratio); 
      DDAResult dda = digDifAnalysis(cam, world, rayDir);
      
      if (dda.hit) {
        g_frameBuffer[idx] = computeASCIIChar(dda, cam, rayDir, world, &g_colorBuffer[idx]);
      } else {
        g_frameBuffer[idx] = ' '; 
        g_colorBuffer[idx] = 0xFF; 
      }
    }
  }
  
  /*draw a cross finder to see where are we pointing at */
  size_t centerIdx = (ROWS / 2) * COLS + (COLS / 2); 
  g_frameBuffer[centerIdx] = '+'; 
  g_colorBuffer[centerIdx] = 0xFF; 


  /* show the selected block in the bottom left corner of the screen with a colored '#' */
  size_t uiIdx = (ROWS - 2) * COLS + 2;
  g_frameBuffer[uiIdx] = '[';
  g_colorBuffer[uiIdx] = 0xFF; /* Reset */

  g_frameBuffer[uiIdx + 1] = '#';
  g_colorBuffer[uiIdx + 1] = (cam->selectedBlock << 4) | FACE_TOP;

  g_frameBuffer[uiIdx + 2] = ']';
  g_colorBuffer[uiIdx + 2] = 0xFF; /* Reset */

  return printBuffer(g_frameBuffer, g_colorBuffer, COLS, ROWS);
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
  dda.dist = INFINITY;  

  /* get the ray starting point in map coordinates */
  ivec3 voxel = {
    (int)floorf(cam->pos.x),
    (int)floorf(cam->pos.y),
    (int)floorf(cam->pos.z)
  }; 

  /* Calculate step direction, tDelta, and initial tMax per axis safely */
  ivec3 step;
  vec3 tDelta;
  vec3 tMax;

  if (rayDir.x > 0.0f) {
    step.x = 1;
    tDelta.x = 1.0f / rayDir.x;
    tMax.x = (floorf(cam->pos.x) + 1.0f - cam->pos.x) * tDelta.x;
  } else if (rayDir.x < 0.0f) {
    step.x = -1;
    tDelta.x = fabsf(1.0f / rayDir.x);
    tMax.x = (cam->pos.x - floorf(cam->pos.x)) * tDelta.x;
  } else {
    step.x = 0;
    tDelta.x = INFINITY;
    tMax.x = INFINITY;
  }

  if (rayDir.y > 0.0f) {
    step.y = 1;
    tDelta.y = 1.0f / rayDir.y;
    tMax.y = (floorf(cam->pos.y) + 1.0f - cam->pos.y) * tDelta.y;
  } else if (rayDir.y < 0.0f) {
    step.y = -1;
    tDelta.y = fabsf(1.0f / rayDir.y);
    tMax.y = (cam->pos.y - floorf(cam->pos.y)) * tDelta.y;
  } else {
    step.y = 0;
    tDelta.y = INFINITY;
    tMax.y = INFINITY;
  }

  if (rayDir.z > 0.0f) {
    step.z = 1;
    tDelta.z = 1.0f / rayDir.z;
    tMax.z = (floorf(cam->pos.z) + 1.0f - cam->pos.z) * tDelta.z;
  } else if (rayDir.z < 0.0f) {
    step.z = -1;
    tDelta.z = fabsf(1.0f / rayDir.z);
    tMax.z = (cam->pos.z - floorf(cam->pos.z)) * tDelta.z;
  } else {
    step.z = 0;
    tDelta.z = INFINITY;
    tMax.z = INFINITY;
  }

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
    /* World boundaries check directly on voxel coordinates */
    if (voxel.x < 0 || voxel.y < 0 || voxel.z < 0 ||
        voxel.x >= WORLD_SIZE_X || voxel.y >= WORLD_SIZE_Y || voxel.z >= WORLD_SIZE_Z)
      break; 

    /* Current chunk coordinates (safe from negative truncation) */
    int cx = voxel.x / CHUNK_SIZE; 
    int cy = voxel.y / CHUNK_SIZE; 
    int cz = voxel.z / CHUNK_SIZE; 

    size_t chunkIdx = GET_CHUNK_INDEX(cx, cy, cz); 
    Chunk *currentChunk = &world[chunkIdx];

    /* Level 1: check if the chunk is empty */
    if (currentChunk->solidCount > 0) {
      /* Level 2: voxel‑by‑voxel DDA inside this chunk */
      while (1) {
        /* Check if we stepped outside world boundaries */
        if (voxel.x < 0 || voxel.y < 0 || voxel.z < 0 ||
            voxel.x >= WORLD_SIZE_X || voxel.y >= WORLD_SIZE_Y || voxel.z >= WORLD_SIZE_Z)
          break;

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
        if (voxel.x < 0 || voxel.y < 0 || voxel.z < 0 ||
            voxel.x >= WORLD_SIZE_X || voxel.y >= WORLD_SIZE_Y || voxel.z >= WORLD_SIZE_Z)
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

  /* 1. Calculate the exact intersection point of the ray with a voxel */
  float hitX = cam->pos.x + rayDir.x * dda.dist;
  float hitY = cam->pos.y + rayDir.y * dda.dist; 
  float hitZ = cam->pos.z + rayDir.z * dda.dist;

  /* 2. local coordinates inside the voxel [0.0, 1.0] */
  float fx = hitX - floorf(hitX); 
  float fy = hitY - floorf(hitY); 
  float fz = hitZ - floorf(hitZ);

  /* 3. find the bumped face (0=TOP, 1=BOTTOM, 2=SIDE_X, 3=SIDE_Z) */
  u8 faceIndex = FACE_SIDE_X;
  if (dda.hitFace == 1) {
    if (dda.step.y < 0) {
      faceIndex = FACE_TOP;
    } else {
      faceIndex = FACE_BOTTOM;
    }
  } else if (dda.hitFace == 0) {
    faceIndex = FACE_SIDE_X;
  } else {
    faceIndex = FACE_SIDE_Z;
  }

  /* 4. verify if the hit point is on the edge of a block */ 
  float edge = 0.01f; 
  bool isEdge = false; 
  if (dda.hitFace == 1) {
    if (fx < edge || fx > 1.0f - edge || fz < edge || fz > 1.0f - edge) isEdge = true; 
  } else if (dda.hitFace == 0) {
    if (fy < edge || fy > 1.0f - edge || fz < edge || fz > 1.0f - edge) isEdge = true; 
  } else {
    if (fx < edge || fx > 1.0f - edge || fy < edge || fy > 1.0f - edge) isEdge = true; 
  }

  /* 5. pack the color with the correct face */
  *outColor = (blockID << 4) | (faceIndex & 0x0F);

  /* 6. if it's an edge, return an empty space (to make cubes more visible) */
  if (isEdge) {
    return ' ';
  }

  /* 7. otherwise return ASCII char mapped to the bumped face */
  switch (faceIndex) {
    case FACE_TOP:    return '#';
    case FACE_BOTTOM: return '.';
    case FACE_SIDE_X: return '|';
    case FACE_SIDE_Z: return ':';
    default:          return '#';
  }
}


/* Build the full output string with ANSI escape sequences and do a single write */
bool printBuffer(unsigned char *fb, u8 *colorBuffer, size_t COLS, size_t ROWS) {
  size_t fbSize = COLS * ROWS;
  if (!ensureBuffers(fbSize)) return false;
 
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
      memcpy(g_outputBuffer + pos, esc, escLen);
      pos += escLen;
      currentColor = packed;
    }
    g_outputBuffer[pos++] = fb[i];
  }
 
  memcpy(g_outputBuffer + pos, ANSI_COLOR_RESET, strlen(ANSI_COLOR_RESET));
  pos += strlen(ANSI_COLOR_RESET);
 
  return write(STDOUT_FILENO, g_outputBuffer, pos) == (ssize_t)pos;
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

