SHELL  = /bin/bash
CC     = gcc
EMCC   = emcc

RAYLIB_SRC     = lib/raylib/src
RAYLIB_LIB     = lib/raylib/build/raylib/libraylib.a
RAYLIB_LIB_WEB = lib/raylib/build_web/raylib/libraylib.a

SRCS = src/main.c src/wfc.c src/draw_tool.c src/maze.c src/player.c src/enemy.c

CFLAGS   = -Wall -Wextra -O2 -I$(RAYLIB_SRC) -Isrc
LDFLAGS  = $(RAYLIB_LIB) -lm -ldl -lpthread -lGL -lX11 -lXrandr -lXinerama -lXcursor -lXi

WASM_CFLAGS  = -O2 -I$(RAYLIB_SRC) -Isrc -DPLATFORM_WEB -s USE_GLFW=3
WASM_LDFLAGS = $(RAYLIB_LIB_WEB) -s USE_GLFW=3 -s ASYNCIFY \
               -s TOTAL_MEMORY=67108864 \
               --shell-file web/shell.html \
               --preload-file assets

# ---- Main game ----
all: mazerunner

mazerunner: $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@

# ---- WASM ----
web: $(SRCS) web/shell.html
	source ~/emsdk/emsdk_env.sh && \
	$(EMCC) $(WASM_CFLAGS) $(SRCS) $(WASM_LDFLAGS) -o web/index.html

# ---- Tests (no raylib needed for wfc/maze logic tests) ----
TEST_CFLAGS = -Wall -Wextra -O2 -I$(RAYLIB_SRC) -Isrc

# WFC test: no raylib dependency
test-wfc: tests/test_wfc.c src/wfc.c
	$(CC) $(TEST_CFLAGS) $^ -lm -o tests/test_wfc
	./tests/test_wfc

# Draw tool test: needs raylib (opens a window)
test-draw: tests/test_draw.c src/draw_tool.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o tests/test_draw
	./tests/test_draw

# Maze test: needs raylib (visual test)
test-maze: tests/test_maze.c src/wfc.c src/maze.c src/draw_tool.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o tests/test_maze
	./tests/test_maze

# Player test: needs raylib
test-player: tests/test_player.c src/wfc.c src/maze.c src/player.c src/draw_tool.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o tests/test_player
	./tests/test_player

# Enemy test: visual — enemies chase player through maze using BFS
test-enemy: tests/test_enemy.c src/wfc.c src/maze.c src/player.c src/draw_tool.c src/enemy.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o tests/test_enemy
	./tests/test_enemy

clean:
	rm -f mazerunner src/*.o
	rm -f web/index.html web/index.js web/index.wasm web/index.data
	rm -f tests/test_wfc tests/test_draw tests/test_maze tests/test_player tests/test_enemy

.PHONY: all web clean test-wfc test-draw test-maze test-player test-enemy
