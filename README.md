# MazeRunner

**[Play it in your browser →](https://nadarenator.github.io/mazerunner/)**

A procedurally generated infinite maze exploration game written in C with Raylib. You draw a tiny 8×8 pixel pattern, and a Wave Function Collapse algorithm turns it into an ever-shifting infinite maze you can explore in your browser. You play as a hooded explorer carrying a flickering wooden torch. Your hunger increases constantly; find and collect the green orbs scattered through the maze to stay alive — and avoid the flaming skulls that spawn from the darkness and chase you down. Watch out for spike traps hidden in the floor that periodically shoot up from their holes.

## How to Play

1. **Draw mode** — Paint an 8×8 tile using your mouse. Left-click = paint, right-click = erase. A default pattern is pre-loaded so you can start immediately.
   - **Wall mode** (black): paints solid walls that block movement.
   - **Orb mode** (green): paints orb spawn points. Orbs must be placed as single isolated pixels — no two orbs can be adjacent.
   - **Enemy mode** (red): paints enemy spawn points. Same isolation rule applies.
   - **Spike mode** (stone floor with holes): paints spike trap tiles. Same isolation rule applies.
   - Press **G** to cycle between modes (Wall → Orb → Enemy → Spike), or click a colour swatch to select directly.
2. Press **Enter** or click **Start Exploring** to generate the maze.
3. **Play mode** — Navigate with **WASD** or **arrow keys**. The maze extends infinitely; tiles that scroll offscreen are discarded and freshly regenerated if you return.
   - Your **hunger bar** (bottom of screen) drains continuously. Walk over a **green orb** to restore 50% hunger.
   - **Flaming skulls** spawn from within your torch radius and chase you using shortest-path (BFS). They materialise frozen (ice-blue with icicles) for 1 second, then ignite and begin chasing. They move at half your speed. Contact means **Game Over**.
   - **Spike traps** look like regular floor tiles with 9 small holes. They cycle every 3 seconds: holes glow amber briefly as a warning, then metallic spikes shoot up for 1 second. Standing on raised spikes costs **50% hunger** (enemies are unaffected).
   - Hunger hits zero → **Game Over**. Press **Enter**, **Space**, or **ESC** to try again.
4. Press **ESC** at any time to return to draw mode and start fresh.

A circular torch viewport (radius 280px) limits your vision. Everything beyond it is black.

---

## Installation

### 1. System dependencies (Ubuntu 24.04)

```bash
sudo apt install -y build-essential git python3 cmake \
    libgl1-mesa-dev libglu1-mesa-dev libx11-dev \
    libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

### 2. Emscripten SDK (for WASM builds)

```bash
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
source ~/emsdk/emsdk_env.sh

# Optional: add to shell profile so you don't need to source it manually
echo 'source ~/emsdk/emsdk_env.sh' >> ~/.bashrc
```

### 3. Raylib (built inside the project, not committed)

```bash
# Clone
git clone --depth=1 https://github.com/raysan5/raylib.git lib/raylib

# Native desktop build
cmake -S lib/raylib -B lib/raylib/build -DBUILD_SHARED_LIBS=OFF -DPLATFORM=Desktop
cmake --build lib/raylib/build -j$(nproc)

# WASM build (run after sourcing emsdk)
emcmake cmake -S lib/raylib -B lib/raylib/build_web \
    -DPLATFORM=Web -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF
cmake --build lib/raylib/build_web -j$(nproc)
```

---

## Build

### Native desktop

```bash
make
```

### WebAssembly

Make sure emsdk is sourced first (or it will be sourced automatically by the Makefile if `~/emsdk` exists).

```bash
source ~/emsdk/emsdk_env.sh
make web
```

Output goes to `web/index.html`, `web/index.js`, `web/index.wasm`.

### Clean

```bash
make clean
```

---

## Run

### Native

```bash
./mazerunner
```

### Browser (WASM)

The WASM build **must** be served over HTTP — opening `index.html` directly via `file://` will not work due to browser security restrictions on WebAssembly.

**Start the server:**
```bash
cd web && python3 -m http.server 8080
```

Then open **http://localhost:8080/index.html** in your browser.

**Stop the server:**
Press `Ctrl+C` in the terminal where the server is running, or:
```bash
pkill -f "http.server 8080"
```

---

## Tests

Each module has a standalone test. The WFC test is headless; all others open a window.

```bash
make test-wfc      # headless — pattern extraction, generation, orb pattern tests
make test-draw     # visual — draw canvas with wall/orb/enemy paint modes and adjacency checks
make test-maze     # visual — maze scroll with orb tiles shown as green dots
make test-player   # visual — full player movement, health decay, and orb pickup
make test-enemy    # visual — BFS enemies chasing player through maze
```

All visual tests exit with **ESC**.

---

## Project Structure

```
mazerunner/
├── Makefile
├── src/
│   ├── main.c             Game entry point, state machine, Emscripten loop
│   ├── wfc.h / wfc.c      Wave Function Collapse — pattern extraction + generation
│   ├── draw_tool.h / .c   8×8 canvas with wall/orb/enemy paint modes
│   ├── maze.h / maze.c    Rolling tile buffer, WFC-driven infinite scroll, orb/enemy tracking, circular render
│   ├── player.h / player.c  Movement, wall collision, camera
│   └── enemy.h / enemy.c  BFS-chasing enemies, spawn/cull/render
├── tests/
│   ├── test_wfc.c
│   ├── test_draw.c
│   ├── test_maze.c
│   ├── test_player.c
│   └── test_enemy.c
├── web/
│   ├── shell.html         Emscripten HTML template
│   ├── index.html         Generated — do not edit
│   ├── index.js           Generated — do not edit
│   └── index.wasm         Generated — do not edit
└── lib/
    └── raylib/            Cloned raylib source (gitignored)
```

---

## Credits

| Asset | Author | Source |
|---|---|---|
| Skull Monster Sprite Sheet | dogchicken | [opengameart.org/content/skull-monster-sprite-sheet](https://opengameart.org/content/skull-monster-sprite-sheet) |

---

## Controls

| Key / Input | Action |
|-------------|--------|
| WASD / Arrow keys | Move |
| ESC (play mode) | Return to draw mode |
| ESC / ENTER / SPACE (game over) | Return to draw mode |
| Left-click (draw mode) | Paint current mode (wall, orb, enemy, or spike) |
| Right-click (draw mode) | Erase (floor) |
| G (draw mode) | Cycle paint mode: wall → orb → enemy → spike |
| Enter (draw mode) | Start exploring |
