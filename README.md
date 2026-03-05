# MazeRunner

A procedurally generated infinite maze exploration game written in C with Raylib. You draw a tiny 8×8 pixel pattern, and a Wave Function Collapse algorithm turns it into an ever-shifting infinite maze you can explore in your browser.

## How to Play

1. **Draw mode** — Paint an 8×8 tile using your mouse. Left-click = wall (black), right-click = floor (white). A default pattern is pre-loaded so you can start immediately.
2. Press **Enter** or click **Start Exploring** to generate the maze.
3. **Play mode** — Navigate the maze with **WASD** or **arrow keys**. The maze extends infinitely in all directions. Tiles that scroll offscreen are discarded and freshly regenerated if you return — the maze intentionally shifts to stay alive.
4. Press **ESC** to return to draw mode and create a new maze from a different pattern.

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

Each module has a standalone test. Visual tests open a window; the WFC test is headless.

```bash
make test-wfc      # headless — prints ASCII maze to stdout, no window
make test-draw     # visual — opens the draw canvas, left/right click to paint
make test-maze     # visual — maze scroll with crosshair camera, no player collision
make test-player   # visual — full player movement and wall collision
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
│   ├── draw_tool.h / .c   8×8 MS Paint-style canvas
│   ├── maze.h / maze.c    Rolling tile buffer, WFC-driven infinite scroll, circular render
│   └── player.h / player.c  Movement, wall collision, camera
├── tests/
│   ├── test_wfc.c
│   ├── test_draw.c
│   ├── test_maze.c
│   └── test_player.c
├── web/
│   ├── shell.html         Emscripten HTML template
│   ├── index.html         Generated — do not edit
│   ├── index.js           Generated — do not edit
│   └── index.wasm         Generated — do not edit
└── lib/
    └── raylib/            Cloned raylib source (gitignored)
```

---

## Controls

| Key | Action |
|-----|--------|
| WASD / Arrow keys | Move |
| ESC | Return to draw mode |
| Left-click (draw mode) | Paint wall |
| Right-click (draw mode) | Erase (floor) |
| Enter (draw mode) | Start exploring |
