#ifndef CAMERA_H
#define CAMERA_H

#include "utils.h"
#include <math.h>

#define MOVE_ACCEL   0.06f 
#define MOVE_DAMPING 0.80f 
#define ROT_ACCEL    0.85f
#define ROT_DAMPING  0.75f
#define FOCAL_LENGTH 1.0f

/* Collision detection constants */
#define CAMERA_HEIGHT 1.8f  /* Camera height for collision checking */

typedef struct {
  vec3 pos;         // camera position 
  vec3 vel;         // camera movement velocity
  float yaw;        // horizontal rot in deg (around y-axis) 
  float pitch;      // vertical rot in deg   (around x-axis) 
  float yawVel;     // rotational velocity (yaw)
  float pitchVel;   // rotational velocity (pitch)
  vec3 forward;     // forward direction 
  vec3 right;       // right direction 
  vec3 up;          // up direction
  u8 selectedBlock; // type of block selected
} Camera; 

/* initialize the camera state */
void cameraInit( Camera* cam, vec3 initPos, float yaw, float pitch );  

/* update the rotational angles of the camera */
void cameraRotate(Camera *cam, float dyaw, float dpitch);
/* update the position vector of the camera */
void cameraMove(Camera *cam, float dx, float dy, float dz);

/* Check if a position is valid (not colliding with solid blocks) */
int cameraCanMoveTo(Chunk *world, vec3 pos);

/* Update camera rotational inertia, directional acceleration with diagonal
 * normalization, velocity damping, and multi-axis wall-sliding collision */
void cameraUpdatePhysics(Camera *cam, Chunk *world,
                         float forwardAmt, float rightAmt, float upAmt,
                         float yawAmt, float pitchAmt);

/* It's important to get the forward, right and up vectors always updated with the yaw and pitch angles */
void cameraUpdateVectors(Camera *cam); 


#endif 
