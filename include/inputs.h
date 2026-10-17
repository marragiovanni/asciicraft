#ifndef INPUTS_H
#define INPUTS_H

#include <termios.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "utils.h"
#include "camera.h"


void getTerminalSize(size_t* cols, size_t* rows); 
void enableRawMode(void); 
void disableRawMode(void); 

bool handleInputs(Camera *cam, Chunk *world); 

#endif 
