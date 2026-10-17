#ifndef INPUTS_H
#define INPUTS_H

#include <termios.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "camera.h"
#include "utils.h"

/* Query the current terminal dimensions (falls back to 80×24). */
void getTerminalSize(size_t *cols, size_t *rows);

/* Switch stdin to raw mode and push the Kitty Keyboard Protocol.
 * Registers disableRawMode() via atexit(). */
void enableRawMode(void);

/* Pop the Kitty Keyboard Protocol and restore the original terminal mode. */
void disableRawMode(void);

/* Query the terminal for Kitty Keyboard Protocol support.
 * Must be called after enableRawMode(). Returns true if supported.
 * Blocks for up to 150 ms waiting for the terminal's response. */
bool checkKittySupport(void);

/* Process all pending input and advance camera physics for one frame.
 * Returns false when the game should quit. */
bool handleInputs(Camera *cam, Chunk *world);

#endif
