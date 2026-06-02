#ifndef CAMERA_H
#define CAMERA_H

#include "utils.h"
#include <math.h>

#define MOVE_SPEED 0.35f 
#define ROT_SPEED  3.0f
#define FOV        69.9f 
#define FOCAL_LENGTH 1.0f

/* Collision detection constants */
#define CAMERA_RADIUS 0.2f  /* Camera collision radius */
#define CAMERA_HEIGHT 1.8f  /* Camera height for collision checking */

typedef struct {
  vec3 pos;         // camera position 
  float yaw;        // horizontal rot in deg (around y-axis) 
  float pitch;      // vertical rot in deg   (around x-axis) 
  vec3 forward;     // forward direction 
  vec3 right;       // right direction 
  vec3 up;          // up direction
  u8 selectedBlock; // type of block selected
} Camera; 

/* initialize the camera state */
void cameraInit( Camera* cam, vec3 initPos, float yaw, float pitch );  

/* update the rotational angles of the camera, this function get called by handleInput() @inputs.h */
void cameraRotate(Camera *cam, float dyaw, float dpitch);
/* update the position vector of the camera, this function also get called by handleInput() @inputs.h */
void cameraMove(Camera *cam, float dx, float dy, float dz);

/* Check if a position is valid (not colliding with solid blocks) */
int cameraCanMoveTo(Chunk *world, vec3 pos);

/* It's important to get the forward, right and up vectors always updated with the yaw and pitch angles */
void cameraUpdateVectors(Camera *cam); 


#endif 
