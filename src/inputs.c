#include <poll.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "inputs.h"
#include "camera.h"
#include "render.h"
#include "utils.h"

/* Key indices for arrow keys in g_keysDown */
#define KEY_ARROW_UP    250
#define KEY_ARROW_DOWN  251
#define KEY_ARROW_RIGHT 252
#define KEY_ARROW_LEFT  253

/* Key state: true when held, false when released */
static bool g_keysDown[256] = {0};
static struct termios g_orig_termios;

/* -------------------------------------------------------------------------
 * Terminal setup
 * ---------------------------------------------------------------------- */

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
  raw.c_cc[VMIN]  = 0; /* non-blocking read */
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);

  /* Enable Kitty Keyboard Protocol (push mode, flags 1|2|8):
   *   1 – disambiguate escape codes
   *   2 – report event types (press/repeat/release)
   *   8 – report all keys as escape sequences */
  if (write(STDOUT_FILENO, "\x1b[>11u", 6) < 0) perror("write");
}

void disableRawMode(void) {
  /* Pop Kitty keyboard mode */
  if (write(STDOUT_FILENO, "\x1b[<u", 4) < 0) perror("write");
  tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
}

bool checkKittySupport(void) {
  /* Query the terminal for its active Kitty Keyboard Protocol flags.
   * A supporting terminal responds with: ESC [ ? <flags> u
   * A non-supporting terminal ignores the query and sends nothing. */
  if (write(STDOUT_FILENO, "\x1b[?u", 4) < 0) return false;

  struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
  if (poll(&pfd, 1, 150) <= 0) return false; /* timed out: no Kitty support */

  char buf[32];
  int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
  if (n < 4) return false;
  buf[n] = '\0';

  /* Validate response shape: ESC [ ? <digits> u */
  return buf[0] == '\x1b' && buf[1] == '[' && buf[2] == '?' && buf[n - 1] == 'u';
}

/* -------------------------------------------------------------------------
 * Action keys  (single-trigger: block selection, place/destroy, quit)
 * Returns false to signal game exit.
 * ---------------------------------------------------------------------- */

static bool handleActionKey(int key, Camera *cam, Chunk *world) {
  switch (key) {
    case '1': cam->selectedBlock = GRASS;       break;
    case '2': cam->selectedBlock = COBBLESTONE; break;
    case '3': cam->selectedBlock = SAND;        break;
    case 'm': case 'M': {
      TargetResult t = getTargetBlock(cam, world, 5.0f);
      if (t.hit) setBlock(world, t.pos.x, t.pos.y, t.pos.z, AIR);
      break;
    }
    case 'b': case 'B': {
      TargetResult t = getTargetBlock(cam, world, 5.0f);
      if (t.hit) setBlock(world, t.adjacent.x, t.adjacent.y, t.adjacent.z, cam->selectedBlock);
      break;
    }
    case '0':
      return false; /* exit */
  }
  return true;
}

/* -------------------------------------------------------------------------
 * Kitty Keyboard Protocol escape sequence parser
 *
 * Recognises two forms after reading the leading ESC [:
 *   CSI <keycode> [;<mod> [:<event>]] u   – regular keys
 *   CSI [<mod> [:<event>]] A|B|C|D        – arrow keys (with Kitty event field)
 *
 * event: 1 = press, 2 = repeat, 3 = release
 * ---------------------------------------------------------------------- */

static bool parseKittySequence(Camera *cam, Chunk *world) {
  char next;
  if (read(STDIN_FILENO, &next, 1) != 1 || next != '[') return true;

  /* Collect parameter bytes until the command byte (0x40–0x7E) */
  char param[32];
  int  len = 0;
  char cmd = 0;

  while (len < 31 && read(STDIN_FILENO, &param[len], 1) == 1) {
    if (param[len] >= 0x40 && param[len] <= 0x7E) {
      cmd = param[len];
      param[len] = '\0';
      break;
    }
    len++;
  }

  if (cmd == 'u') {
    /* Regular Kitty key: CSI <keycode>[;<mod>[:<event>]] u */
    int key   = atoi(param);
    char *col = strchr(param, ':');
    int event = col ? atoi(col + 1) : 1;

    if (key > 0 && key < 256) {
      g_keysDown[key] = (event != 3);
      if (event == 1 && !handleActionKey(key, cam, world)) return false;
    }

  } else if (cmd >= 'A' && cmd <= 'D') {
    /* Arrow key: CSI [<mod>[:<event>]] A|B|C|D */
    char *col = strchr(param, ':');
    int event = col ? atoi(col + 1) : 1;

    int key = (cmd == 'A') ? KEY_ARROW_UP    :
              (cmd == 'B') ? KEY_ARROW_DOWN   :
              (cmd == 'C') ? KEY_ARROW_RIGHT  : KEY_ARROW_LEFT;

    g_keysDown[key] = (event != 3);
  }

  return true;
}

/* -------------------------------------------------------------------------
 * Movement physics  (acceleration, inertia, damping, wall-sliding)
 * ---------------------------------------------------------------------- */

static void applyMovement(Camera *cam, Chunk *world) {
  float dx = cam->vel.x, dy = cam->vel.y, dz = cam->vel.z;
  if (dx == 0.0f && dy == 0.0f && dz == 0.0f) return;

  vec3 newPos = { cam->pos.x + dx, cam->pos.y + dy, cam->pos.z + dz };
  if (cameraCanMoveTo(world, newPos)) {
    cameraMove(cam, dx, dy, dz);
    return;
  }

  /* Wall sliding: try each axis independently */
  if (dx != 0.0f) {
    vec3 posX = { cam->pos.x + dx, cam->pos.y, cam->pos.z };
    if (cameraCanMoveTo(world, posX)) cameraMove(cam, dx, 0.0f, 0.0f);
    else cam->vel.x = 0.0f;
  }
  if (dy != 0.0f) {
    vec3 posY = { cam->pos.x, cam->pos.y + dy, cam->pos.z };
    if (cameraCanMoveTo(world, posY)) cameraMove(cam, 0.0f, dy, 0.0f);
    else cam->vel.y = 0.0f;
  }
  if (dz != 0.0f) {
    vec3 posZ = { cam->pos.x, cam->pos.y, cam->pos.z + dz };
    if (cameraCanMoveTo(world, posZ)) cameraMove(cam, 0.0f, 0.0f, dz);
    else cam->vel.z = 0.0f;
  }
}

/* -------------------------------------------------------------------------
 * Main input handler  (called once per frame)
 * Returns false when the game should exit.
 * ---------------------------------------------------------------------- */

bool handleInputs(Camera *cam, Chunk *world) {
  struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };

  /* Drain all pending bytes from stdin */
  while (poll(&pfd, 1, 0) > 0) {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) break;

    if (c == '\x1b') {
      if (!parseKittySequence(cam, world)) return false;
    } else {
      /* Kitty reports printable keys as plain chars on press events too */
      g_keysDown[(unsigned char)c] = true;
      if (!handleActionKey(c, cam, world)) return false;
    }
  }

  /* Derive movement intent from held-key state */
  float forwardAmt = (g_keysDown['w'] || g_keysDown['W'] ? 1.0f : 0.0f)
                   - (g_keysDown['s'] || g_keysDown['S'] ? 1.0f : 0.0f);

  float rightAmt   = (g_keysDown['d'] || g_keysDown['D'] ? 1.0f : 0.0f)
                   - (g_keysDown['a'] || g_keysDown['A'] ? 1.0f : 0.0f);

  float upAmt      = (g_keysDown['e'] || g_keysDown['E'] ? 1.0f : 0.0f)
                   - (g_keysDown['q'] || g_keysDown['Q'] ? 1.0f : 0.0f);

  float yawAmt     = (g_keysDown[KEY_ARROW_RIGHT] ? 1.0f : 0.0f)
                   - (g_keysDown[KEY_ARROW_LEFT]  ? 1.0f : 0.0f);

  float pitchAmt   = (g_keysDown[KEY_ARROW_UP]   ? 1.0f : 0.0f)
                   - (g_keysDown[KEY_ARROW_DOWN]  ? 1.0f : 0.0f);

  /* 1. Rotational inertia */
  if (yawAmt   != 0.0f) cam->yawVel   += yawAmt   * ROT_ACCEL;
  if (pitchAmt != 0.0f) cam->pitchVel += pitchAmt  * ROT_ACCEL;

  cam->yawVel   *= ROT_DAMPING;
  cam->pitchVel *= ROT_DAMPING;

  if (fabsf(cam->yawVel)   < 0.01f) cam->yawVel   = 0.0f;
  if (fabsf(cam->pitchVel) < 0.01f) cam->pitchVel = 0.0f;

  if (cam->yawVel != 0.0f || cam->pitchVel != 0.0f)
    cameraRotate(cam, cam->yawVel, cam->pitchVel);

  /* 2. Movement acceleration with diagonal normalisation */
  if (forwardAmt != 0.0f || rightAmt != 0.0f) {
    float mag = sqrtf(cam->forward.x * cam->forward.x +
                      cam->forward.z * cam->forward.z);
    if (mag < 0.001f) mag = 0.001f;

    float fwdX = cam->forward.x / mag;
    float fwdZ = cam->forward.z / mag;

    float ax = fwdX * forwardAmt + cam->right.x * rightAmt;
    float az = fwdZ * forwardAmt + cam->right.z * rightAmt;

    float aMag = sqrtf(ax * ax + az * az);
    if (aMag > 0.001f) { ax /= aMag; az /= aMag; }

    cam->vel.x += ax * MOVE_ACCEL;
    cam->vel.z += az * MOVE_ACCEL;
  }

  if (upAmt != 0.0f) cam->vel.y += upAmt * MOVE_ACCEL;

  /* 3. Velocity damping */
  cam->vel.x *= MOVE_DAMPING;
  cam->vel.y *= MOVE_DAMPING;
  cam->vel.z *= MOVE_DAMPING;

  if (fabsf(cam->vel.x) < 0.0005f) cam->vel.x = 0.0f;
  if (fabsf(cam->vel.y) < 0.0005f) cam->vel.y = 0.0f;
  if (fabsf(cam->vel.z) < 0.0005f) cam->vel.z = 0.0f;

  /* 4. Apply position with wall-sliding collision */
  applyMovement(cam, world);

  return true;
}
