#include <poll.h>
#include <math.h>
#include "inputs.h"
#include "camera.h"
#include "utils.h" 
#include "render.h"

static struct termios g_orig_termios;

void getTerminalSize(size_t *cols, size_t *rows) {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  *cols = w.ws_col ? w.ws_col : 80;
  *rows = w.ws_row ? w.ws_row : 24;
}

void enableRawMode(void) {
  tcgetattr(STDIN_FILENO, &g_orig_termios);
  atexit(disableRawMode);
  struct termios raw = g_orig_termios;
  cfmakeraw(&raw);
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void disableRawMode(void) {
  tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}

void handleInputs(Camera *cam, Chunk *world) {
    struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
    
    /* current frame holders */ 
    float forwardAmt = 0.0f;
    float rightAmt   = 0.0f;
    float upAmt      = 0.0f;
    float yawAmt     = 0.0f;
    float pitchAmt    = 0.0f;

    /* Flag for block actions */    
    bool actionDestroy = false;
    bool actionPlace   = false;

    /* empty the buffer */ 
    while (poll(&pfd, 1, 0) > 0) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) break;

        switch(c) {
            /* switching type of block */
            case '1':
              cam->selectedBlock = GRASS; 
              break; 
            case '2':
              cam->selectedBlock = COBBLESTONE; 
              break; 
            case '3':
              cam->selectedBlock = SAND; 
              break; 
            /* movement */
            case 'w': case 'W': forwardAmt += 1.0f; break;
            case 's': case 'S': forwardAmt -= 1.0f; break;
            case 'd': case 'D': rightAmt   += 1.0f; break;
            case 'a': case 'A': rightAmt   -= 1.0f; break;
            case 'e': case 'E': upAmt      += 1.0f; break; /* go up */ 
            case 'q': case 'Q': upAmt      -= 1.0f; break; /* go down */

            /* rotation (Escape Sequences) */
            case '\x1b': {
                char seq[2];
                if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                    if (seq[0] == '[') {
                        switch(seq[1]) {
                            case 'A': pitchAmt += 1.0f; break; /* up    arrow    */
                            case 'B': pitchAmt -= 1.0f; break; /* down  arrow    */
                            case 'C': yawAmt   += 1.0f; break; /* right arrow    */ 
                            case 'D': yawAmt   -= 1.0f; break; /* left  arrow    */
                        }
                    }
                }
                break;
            }

            /* block interactions */
            case 'm': case 'M': actionDestroy = true; break;
            case 'b': case 'B': actionPlace   = true;   break;

            /* close the program */
            case '0':
                disableRawMode();
                if (write(STDOUT_FILENO, "\033[?25h", 6) < 0) {}
                exit(0);
                break;
        }
    }

    /* --- 1. apply rotation --- */
    if (yawAmt != 0.0f || pitchAmt != 0.0f) {
        cameraRotate(cam, yawAmt * ROT_SPEED, pitchAmt * ROT_SPEED);
    }

    /* --- 2. apply movement --- */
    if (forwardAmt != 0.0f || rightAmt != 0.0f || upAmt != 0.0f) {
        /* x forward vector (xz plane) for constant speed */
        float mag = sqrtf(cam->forward.x * cam->forward.x + cam->forward.z * cam->forward.z);
        if (mag < 0.001f) mag = 0.001f;

        float fwdX = cam->forward.x / mag;
        float fwdZ = cam->forward.z / mag;

        /* final moving */
        float dx = (fwdX * forwardAmt + cam->right.x * rightAmt) * MOVE_SPEED;
        float dz = (fwdZ * forwardAmt + cam->right.z * rightAmt) * MOVE_SPEED;
        float dy = upAmt * MOVE_SPEED;
        
        vec3 newPos = {
          cam->pos.x + dx, 
          cam->pos.y + dy, 
          cam->pos.z + dz 
        }; 
        if (cameraCanMoveTo(world, newPos)) {
          cameraMove(cam, dx, dy, dz);
        }
    }

    /* --- 3. apply actions on blocks --- */
    if (actionDestroy) {
        TargetResult target = getTargetBlock(cam, world, 5.0f);
        if (target.hit) setBlock(world, target.pos.x, target.pos.y, target.pos.z, AIR);
    }
    if (actionPlace) {
        TargetResult target = getTargetBlock(cam, world, 5.0f);
        if (target.hit) setBlock(world, target.adjacent.x, target.adjacent.y, target.adjacent.z, cam->selectedBlock);
    }
}
