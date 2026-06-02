#include "camera.h"
#include "utils.h"

/* Initialize the camera position */
void cameraInit( Camera* cam, vec3 initPos, float yaw, float pitch ) {
  // init pos, yaw, pitch 
  cam->pos = initPos; 
  cam->yaw = yaw; 
  cam->pitch = pitch; 
  cam->selectedBlock = COBBLESTONE; 

  cameraUpdateVectors(cam);
}

/* update rotational angles along y-axis and x-axis with a delta parameter.
 * This function is called by the Input handling system                    */
void cameraRotate(Camera *cam, float dyaw, float dpitch) {
  cam->yaw += dyaw; 
  cam->pitch += dpitch; 

  if (cam->pitch > 89.0f) cam->pitch = 89.0f; 
  if (cam->pitch < -89.0f) cam->pitch = -89.0f; 
    
  if (cam->yaw > 360.0f) cam->yaw -= 360.0f; 
  if (cam->yaw < 0.0f) cam->yaw += 360.0f; 

  cameraUpdateVectors(cam); 
}


/* update the position on camera */
void cameraMove(Camera *cam, float dx, float dy, float dz) {
  /* Calculate new position */
  vec3 newPos = {
    cam->pos.x + dx,
    cam->pos.y + dy,
    cam->pos.z + dz
  };

  /* Apply world boundary checks */
  if (newPos.x > WORLD_SIZE_X - 0.1f) newPos.x = WORLD_SIZE_X - 0.1f;
  if (newPos.x < 0.1f)  newPos.x = 0.1f;

  if (newPos.y > WORLD_SIZE_Y - 0.1f) newPos.y = WORLD_SIZE_Y - 0.1f;
  if (newPos.y < 0.1f)  newPos.y = 0.1f;

  if (newPos.z > WORLD_SIZE_Z - 0.1f) newPos.z = WORLD_SIZE_Z - 0.1f;
  if (newPos.z < 0.1f)  newPos.z  = 0.1f;

  /* Update position */
  cam->pos = newPos;
}

/* Check if camera can move to a given position (collision detection) */
int cameraCanMoveTo(Chunk *world, vec3 pos) {
  /* Check key points around the camera position to prevent clipping */
  /* Simplified collision detection: check center and feet */

  vec3 checkPoints[2] = {
    {pos.x, pos.y, pos.z},        /* Center at eye level */
    {pos.x, pos.y - 0.8f, pos.z}  /* Feet level */
  };

  for (int i = 0; i < 2; i++) {
    vec3 checkPos = checkPoints[i];

    /* Convert to integer coordinates for block checking */
    int blockX = (int)floorf(checkPos.x);
    int blockY = (int)floorf(checkPos.y);
    int blockZ = (int)floorf(checkPos.z);

    /* Get the block at this position */
    u8 block = getBlock(world, blockX, blockY, blockZ);
 
    /* If it's a solid block (not air), collision detected */
    if (block != AIR) {
      return 0; /* Cannot move here */
    }
  }

  return 1; /* Can move here */
}


/* update the forward, right and up vectors of the camera */
void cameraUpdateVectors(Camera *cam) {
  /* convert yaw and pitch from degrees to radiants*/  
  float radyaw = DEG2RAD(cam->yaw); 
  float radpitch = DEG2RAD(cam->pitch);
  
  /* get the forward components (z axis) and normalize the vector */
  cam->forward.x = cosf(radpitch) * sinf(radyaw);
  cam->forward.y = sinf(radpitch);
  cam->forward.z = cosf(radpitch) * cosf(radyaw); 
  cam->forward = vec3Normalize(cam->forward); 
  
  /* get the right and up vectors.
   * right: +y cross +z  =  +x vector 
   * up:    +z cross +x  =  +y vector  */
  vec3 worldUp = {0.0f, 1.0f, 0.0f};
  cam->right = vec3Normalize(vec3Cross(worldUp, cam->forward)); 
  cam->up = vec3Cross(cam->forward, cam->right); 
}


