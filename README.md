# MazeRunner

**[Play it in your browser →](https://nadarenator.github.io/mazerunner/)**

Draw a tiny 8×8 pixel tile. A Wave Function Collapse algorithm uses it as a seed to generate an infinite maze around you in real time. Collect food orbs to stave off hunger, dodge spike traps, and outrun flaming skull enemies that chase you through the dark. Written in C with Raylib, compiled to WebAssembly.

---

## How to Play

![Gameplay](assets/gameplay.gif)

Paint an 8×8 tile in the canvas editor (walls, orb spawns, enemy spawns, spike traps), then press **Enter** to generate your maze. Navigate with **WASD** or arrow keys. Your hunger drains constantly — walk over green orbs to restore it. Skull enemies spawn within your torch radius and chase you via BFS. Spike traps give a brief amber warning before raising. Either kills you when your hunger hits zero or an enemy catches you.

Press **G** to cycle paint modes. **ESC** returns to the editor at any time.

---

## How it works

### Wave Function Collapse
The core of the maze generator is a hand-rolled overlapping WFC implementation in C (`src/wfc.c`). On startup it extracts every unique 3×3 patch from your 8×8 input tile (with pixel-wrapping at the borders), counts their frequencies, and builds adjacency bitmasks encoding which patches can legally sit next to each other in each of the four cardinal directions. Generation then collapses cells one at a time: pick the cell with the fewest remaining valid patterns, choose one weighted by frequency, and propagate constraints outward.

Pixel values `0`–`4` (floor, wall, orb, enemy, spike) flow through the algorithm as opaque data. WFC has no knowledge of their semantics; orb and enemy density, and the isolation constraint you paint, reproduce naturally from whatever structure you drew.

### Infinite scrolling maze
The maze is a rolling 44×26-tile buffer, just large enough to fill the screen with a one-tile margin. The player is always at the buffer's centre. The moment the player crosses a tile boundary, the buffer shifts: one column or row is discarded off the back edge and a fresh column or row is generated at the front using WFC, seeded from the patterns already sitting at that edge. Tiles that scroll off are gone; if you backtrack, new tiles are generated in their place. There is no stored world state. The maze is purely a sliding window of WFC output.

### Enemy AI
Skull enemies spawn from floor tiles within your torch radius, materialising through a two-phase freeze animation (ghost hover, then skull solidifying) before they start moving. Pathfinding is 8-directional BFS on the tile grid, recalculated each frame, with no corner-cutting through diagonal wall gaps. Enemies die if they're standing on a spike tile when the spikes raise.

### WebAssembly & leaderboard
The game compiles to WebAssembly via Emscripten (`make web`) and runs entirely in the browser. The Survival mode leaderboard uses Firebase Firestore's REST API directly, with no SDK. Scores are fetched and submitted from JavaScript in the Emscripten HTML shell. The top-10 cache is kept in JS memory so C can query it synchronously via `EM_ASM_INT` without stalling the game loop. A cinematic intro animation (slot-machine pattern reveal, screen shake, and a radial wave that physically forms the maze around you) plays each time Survival mode starts.

---

## Installation & usage

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
git clone --depth=1 https://github.com/raysan5/raylib.git lib/raylib

# Native desktop build
cmake -S lib/raylib -B lib/raylib/build -DBUILD_SHARED_LIBS=OFF -DPLATFORM=Desktop
cmake --build lib/raylib/build -j$(nproc)

# WASM build (run after sourcing emsdk)
emcmake cmake -S lib/raylib -B lib/raylib/build_web \
    -DPLATFORM=Web -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF
cmake --build lib/raylib/build_web -j$(nproc)
```

### Build & run (native)

```bash
make
./mazerunner
```

### Build & run (browser)

```bash
source ~/emsdk/emsdk_env.sh
make web
cd web && python3 -m http.server 8080
```

Then open **http://localhost:8080/index.html**. The WASM build must be served over HTTP — `file://` won't work due to browser security restrictions.

---

## Credits

| Asset | Author | Source |
|---|---|---|
| Skull Monster Sprite Sheet | dogchicken | [opengameart.org/content/skull-monster-sprite-sheet](https://opengameart.org/content/skull-monster-sprite-sheet) |
| The Adventurer — Female | Sscary | [sscary.itch.io/the-adventurer-female](https://sscary.itch.io/the-adventurer-female) |
