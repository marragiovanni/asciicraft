#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define TARGET_FPS 60
#define FRAME_TIME_NS (1000000000L / TARGET_FPS)

/* external libs */
#include "utils.h" 
#include "camera.h" 
#include "inputs.h"
#include "render.h"
#include "noise.h"

/* Create the first state of the world */
void initWorld(Chunk *world) {
  for (size_t x = 0; x < WORLD_SIZE_X; x++) {
    for (size_t z = 0; z < WORLD_SIZE_Z; z++) {
      float h = perlin2D(x * 0.025f, z * 0.025f);
      int height = (int)(h * 12.0f) + 24; /* range between 12 and 36*/

      float b = perlin2D(x * 0.005f, z * 0.005f); 
      int biome; 
      if (b < -0.2f)     biome = 0; // sand 
      else if (b < 0.3f) biome = 1; // lowland 
      else               biome = 2; // mountains 
      
      for (int y = 0; y < WORLD_SIZE_Y; y++) {
        u8 block = AIR; 

        if (y <= height) {
          if (y == height) {
            if (biome == 0) block = SAND; // temporary 
            else if (biome == 1) block = GRASS; 
            else block = COBBLESTONE; 
          }
          else if (y > height - 3) {
            block = GRASS; 
          }
          else {
            block = COBBLESTONE; 
          }
        } 
        setBlock(world, x, y, z, block);
      }
    }
  }
}


int main(void) {
  size_t COLS = 0;  
  size_t ROWS = 0; 
  getTerminalSize(&COLS, &ROWS);

  /* create world and check for errors */
  size_t totalChunks = WORLD_CX * WORLD_CY * WORLD_CZ; 
  Chunk *world = malloc(sizeof(Chunk) * totalChunks); 
  if (!world) return 1;             
  initWorld(world);

  Camera cam; 
  vec3 initPos = {20.0f, 20.0f, 20.0f}; 
  cameraInit(&cam, initPos, 45.0f, -20.0f );
  
  /* terminal in raw mode */
  enableRawMode();
  
  // hide cursor 
  if (write(STDOUT_FILENO, "\033[?25l", 6) < 0) {
    perror("write"); 
  };

  bool isRunning = true;
  int exitCode = 0;

  // clean the screen
  if (write(STDOUT_FILENO, "\033[2J", 4) < 0) {
    perror("write");
  };

  /* Require Kitty Keyboard Protocol — exit cleanly if unsupported */
  if (!checkKittySupport()) {
    if (write(STDOUT_FILENO, "\033[?25h", 6) < 0) perror("write"); /* restore cursor */
    fprintf(stderr,
            "\r\nError: Kitty Keyboard Protocol is not supported by this terminal.\r\n"
            "Supported terminals include: Kitty, Ghostty, WezTerm, Foot, Alacritty, iTerm2, Rio, etc.\r\n"
            "For a complete list of compliant terminals, see:\r\n"
            "https://sw.kovidgoyal.net/kitty/keyboard-protocol/\r\n\r\n");
    isRunning = false;
    exitCode = 1;
  }

  struct timespec frameStart, frameEnd;

  while(isRunning) {
    clock_gettime(CLOCK_MONOTONIC, &frameStart);

    getTerminalSize(&COLS, &ROWS);   

    // get back to the first pos of the screen - avoid flickering 
    if (write(STDOUT_FILENO, "\033[H", 3) < 0) { 
      perror("write");
    };
    
    if (!render(&cam, world, COLS, ROWS)) {
      fprintf(stderr, "Error: out of memory during rendering\n"); 
      isRunning = false; 
      exitCode = 1; 
      break; 
    } 

    isRunning = handleInputs(&cam, world); 

    /* Cap frame rate to TARGET_FPS */ 
    clock_gettime(CLOCK_MONOTONIC, &frameEnd);
    long elapsedNs = (frameEnd.tv_sec - frameStart.tv_sec) * 1000000000L +
                     (frameEnd.tv_nsec - frameStart.tv_nsec);
    if (elapsedNs < FRAME_TIME_NS) {
      struct timespec sleepTime;
      long sleepNs = FRAME_TIME_NS - elapsedNs;
      sleepTime.tv_sec = sleepNs / 1000000000L;
      sleepTime.tv_nsec = sleepNs % 1000000000L;
      nanosleep(&sleepTime, NULL);
    } 
  }

  /* Clean and exit */ 
  disableRawMode(); 
  if (write(STDOUT_FILENO, "\033[?25h", 6) < 0) {
    perror("write");
  }; 

  renderCleanup();
  free(world); 
  return exitCode;  
}
