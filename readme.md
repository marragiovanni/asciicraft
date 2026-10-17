# asciicraft

Minecraft-like voxel world that runs entirely inside a terminal, using only POSIX library. Everything is calculated by the CPU and rendered using ASCII characters and ANSI color sequences. [demo youtube](https://www.youtube.com/watch?v=lmFrXYUtxcc)

![Demo](demo.gif)

### World Structure
The world is managed as a one-dimensional array of **Chunks**. Each chunk is a 16x16x16 cube of voxels, where each individual voxel takes up just one byte to store its ID (Air, Grass, Cobblestone, or Sand).

I implemented a small optimization called `solidCount` for each chunk. Essentially, I keep track of how many solid blocks are in a section. If a chunk contains only air, the rendering algorithm skips it instantly. This saves a massive amount of CPU cycles during raycasting. The terrain generation itself is handled by 2D Perlin noise, which determines the height and biome for every column.

### Rendering System: Two-Level DDA
To "draw" the view on the terminal, I cast a ray for every single character on the screen. Instead of checking every single coordinate in space (which would be way too heavy for a CPU), I use the **DDA (Digital Differential Analyzer)** algorithm.

To keep it fast, the process happens on two levels:
1. **Chunk Level**: The ray travels through the world by "jumping" from chunk to chunk. If it hits a chunk where `solidCount > 0`, it enters the second level.
2. **Voxel Level**: The ray then checks the individual blocks inside that specific chunk until it hits something solid.

Once a collision happens, I calculate which face of the cube was hit (X, Y, or Z) to apply different shading. Finally, I pick the best ASCII character based on the cube face.

### Input Handling and Fluidity
Handling terminal input in C is tricky because the terminal usually waits for the "Enter" key to read data. This is called Canonical Mode.
To fix this, I set the terminal to Raw Mode and use poll(). This means the terminal reads every input instantly after the key is pressed.
However, Raw Mode alone wasn't enough. In a standard terminal, arrow keys and special keys are delivered as multi-byte escape sequences — and more importantly, there is no concept of a key being released. This makes it impossible to know which keys are currently held down, which breaks smooth movement entirely and make it impossible to move the character diagonally (at least you don't want to quickly alternate between w (or s) and a (or d).

To solve this, I use the Kitty Keyboard Protocol. It is an extension supported by modern terminals that changes how keyboard events are reported: every key generates its own sequence with an explicit event type, that can be 'press', 'repeat', or 'release'. This makes it possible to maintain an accurate map of which keys are currently held at any moment, and to detect multiple keys pressed simultaneously without any conflict. At startup, the program queries the terminal to check if it supports the protocol. If it doesn't, it exits immediately with an error message. Sorry :(
Movement is physics-based rather than instant. Held keys apply acceleration to a velocity vector, and that velocity decays each frame through a damping factor — so the camera smoothly coasts to a stop instead of cutting out instantly. Rotation works the same way with its own inertia system. The movement on the horizontal plane is also normalized, so walking diagonally is never faster than walking straight. Collision uses wall-sliding: if the full movement is blocked, each axis is retried independently so the camera slides along surfaces instead of stopping dead.

### Interaction
To destroy or place blocks, I reuse the DDA logic by casting an invisible ray from the center of the view, with a maximum range of 5 blocks.
* Pressing **M** removes the targeted block.
* Pressing **B** places a new block on the face adjacent to the one you hit.
* The `[ # ]` indicator in the bottom-left shows the color of your currently selected material.

### Controls
| Key | Action |
|-------|--------|
| **W / S** | Forward / Backward |
| **A / D** | Left / Right |
| **E / Q** | Up / Down |
| **Arrow Keys** | Rotate View |
| **M / B** | Destroy / Place Block |
| **1 / 2 / 3** | Select Material (Grass, Stone, Sand) |
| **0** | Exit |

### Project Structure
* `src/main.c`: Main loop and procedural generation.
* `src/render.c`: Rendering core (DDA) and video buffer management.
* `src/camera.c`: Movement, rotation, and collision logic.
* `src/inputs.c`: Raw Mode setup and input processing.
* `src/utils.c`: Helpers for coordinates, colors, and voxel management.
* `src/noise.c`: Perlin noise implementation.

### Building and Running
You'll need a GCC compiler, a POSIX-compliant environment (Linux or macOS), and now a terminal that supports the **Kitty Keyboard Protocol**. 
Examples of that includes: kitty, ghostty, wezterm, foot. 
There are many other terminals that support it, you can check the list [here.](https://sw.kovidgoyal.net/kitty/keyboard-protocol/)

```bash
make
./mc
```

### Advice:
A smaller font-size or a zoom-out on your terminal is recommended because smaller and numerous chars produce a better effect, but this affects performance too. 
