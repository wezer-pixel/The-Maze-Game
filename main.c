#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_keyboard.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_scancode.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#define ASSERT(cond, ...)                                                      \
  if (!cond) {                                                                 \
    fprintf(stderr, __VA_ARGS__);                                              \
    exit(1);                                                                   \
  }

#define MIN(x, y) (x < y ? x : y)
#define MAX(x, y) (x > y ? x : y)

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define MAP_SIZE 16
const uint8_t MAP[MAP_SIZE * MAP_SIZE] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 2, 0, 1,
	1, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 2, 0, 1, 
	1, 0, 0, 0, 0, 0, 3, 2, 2, 2, 2, 2, 2, 2, 0, 1,
	1, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 2, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 2, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 2, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};

#define PI 3.14159265f
const float playerFOV = (PI / 2.0f);
const float maxDepth = 20.0f;

typedef enum {NorthSouth, EastWest} Side;
typedef struct {
    float x, y;
} Vec2F;
typedef struct {
    int x, y;
} Vec2I;
typedef struct {
	SDL_Window* window;
	SDL_Renderer* renderer;
	bool quit;
} State;
typedef struct {
	Vec2F pos;
	Vec2F dir;
    Vec2F plane;
} Player;
typedef struct{
    uint8_t r,g,b,a;
} ColorRGBA;
ColorRGBA RGBA_Red   = {.r = 0xFF, .g = 0x00, .b = 0x00, .a = 0xFF};
ColorRGBA RGBA_LightGreen = {.r = 0x00, .g = 0xFF, .b = 0x00, .a = 0xFF};
ColorRGBA RGBA_LightBlue = {.r = 0x00, .g = 0x00, .b = 0xFF, .a = 0xFF};

int xy2index(int x, int y, int w) {
    return y * w + x;
}

void render(State *state, Player* player) {
    for (int x = 0; x < SCREEN_WIDTH; ++x) {
        // calculate ray position and direction
        float cameraX = 2 * x / (float)SCREEN_WIDTH - 1; // x-coordinate in camera space
        Vec2F rayDir = {
            .x = player->dir.x + player->plane.x * cameraX,
            .y = player->dir.y + player->plane.y * cameraX,
        };

        // wich box of the map we're in 
        Vec2I mapBox = {
            .x = (int)player->pos.x, 
            .y = (int)player->pos.y
        };
        // Length of ray from current position to next x- or y-side
        Vec2F sideDist = {};
        // Lenth of ray from one x- or y-side to next x- or y-side 
        Vec2F deltaDist = {
            .x = (rayDir.x == 0) ? 1e30 : fabsf(1 / rayDir.x),
            .y = (rayDir.y == 0) ? 1e30 : fabsf(1 / rayDir.y),
        };
        float perpWallDist;
        // What direction to step in x- or y-direction (either +1 or -1)
        Vec2I stepDir = {};

        bool hit = false; // was there a wall hit
        Side side; // was a NorthSouth or EastWest wall hit

        // calculate stepDir and initial sideDist
        if (rayDir.x < 0) {
            stepDir.x = -1;
            sideDist.x = (player->pos.x - mapBox.x) * deltaDist.x;
        } else {
            stepDir.x = 1;
            sideDist.x = (mapBox.x + 1.0f - player->pos.x) * deltaDist.x;
        }
        if (rayDir.y < 0) {
            stepDir.y = -1;
            sideDist.y = (player->pos.y - mapBox.y) * deltaDist.y;
        } else {
            stepDir.y = 1;
            sideDist.y = (mapBox.y + 1.0f - player->pos.y) * deltaDist.y;
        }

        // DDA
        while (!hit) {
            // jump to next map square
            if (sideDist.x < sideDist.y) {
                sideDist.x += deltaDist.x;
                mapBox.x += stepDir.x;
                side = EastWest;
            } else {
                sideDist.y += deltaDist.y;
                mapBox.y += stepDir.y;
                side = NorthSouth;
            }
            // check if ray has hit a wall
            if (MAP[xy2index(mapBox.x, mapBox.y, MAP_SIZE)] > 0) {
                hit = true;
            }
        }

        // Calculate the distance projceted on camera direction
        // (Euclidian distance would give fisheye effect)
        switch (side) {
            case EastWest:
                perpWallDist = (sideDist.x - deltaDist.x);
                break;
            case NorthSouth:
                perpWallDist = (sideDist.y - deltaDist.y);
                break;
        }

        // Calculate height of line to draw on screen 
        int lineHeight = (int)(SCREEN_HEIGHT / perpWallDist);

        // calculate lowest and highest pixel to fill in current stripe
        int drawStart = -lineHeight / 2 + SCREEN_HEIGHT / 2;
        if (drawStart < 0) drawStart = 0;
        int drawEnd = lineHeight / 2 + SCREEN_HEIGHT / 2; 
        if (drawEnd >= SCREEN_HEIGHT) drawEnd = SCREEN_HEIGHT;

        // choose wall color
        ColorRGBA color;
        switch (MAP[xy2index(mapBox.x, mapBox.y, MAP_SIZE)]) {
            case 1: color = RGBA_Red; break;
            case 2: color = RGBA_LightGreen; break;
            case 3: color = RGBA_LightBlue; break;
        }

        // give x and y sides different brightness
        if (side == NorthSouth) {
            color.r /= 2; 
            color.g /= 2; 
            color.b /= 2; 
        }

        // Draw
        SDL_SetRenderDrawColor(state->renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLine(state->renderer, x, drawStart, x, drawEnd);

	}

}

// rotate player
void rotatePlayer(Player* player, int direction) {
	const float
		rotateSpeed = 0.025;

    float rotSpeed = rotateSpeed * direction;
    // Rotate player's direction and plane vectors
    Vec2F oldDir = player->dir;
    player->dir.x = player->dir.x * cosf(rotSpeed) - player->dir.y * sinf(rotSpeed);
    player->dir.y = oldDir.x * sinf(rotSpeed) + player->dir.y * cosf(rotSpeed);

    Vec2F oldPlane = player->plane;
    player->plane.x = player->plane.x * cosf(rotSpeed) - player->plane.y * sinf(rotSpeed);
    player->plane.y = oldPlane.x * sinf(rotSpeed) + player->plane.y * cosf(rotSpeed);
}


int main(void) {
	ASSERT(!SDL_Init(SDL_INIT_VIDEO),
		   "SDL failed to initialize; %s\n",
		   SDL_GetError());
	State state = {
        .quit = false,
    };
	state.window =
		SDL_CreateWindow("The Maze",
						 SDL_WINDOWPOS_CENTERED_DISPLAY(0),
						 SDL_WINDOWPOS_CENTERED_DISPLAY(0),
						 SCREEN_WIDTH,
						 SCREEN_HEIGHT,
						 SDL_WINDOW_ALLOW_HIGHDPI);
	ASSERT(state.window,
		   "failed to create SDL window: %s\n",
		   SDL_GetError());

	state.renderer =
		SDL_CreateRenderer(state.window,
						   -1,
						   SDL_RENDERER_PRESENTVSYNC);
	ASSERT(state.renderer,
		   "failed to create SDL renderer: %s\n",
		   SDL_GetError());

    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
    SDL_SetRelativeMouseMode(true);

	Player player = {
        .pos = {.x =  4.0f, .y =  4.0f},
        .dir = {.x = -1.0f, .y =  0.0f},
        .plane = {.x = 0.0f, .y = 0.66f},
    };

	const float
		rotateSpeed = 0.025,
		moveSpeed = 0.05;
	
	while (!state.quit) {
		SDL_Event event;
        int mouse_xrel = 0;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
		        case SDL_QUIT:
		        	state.quit = true;
		        	break;

                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_q: // Rotate left
                            // Rotate player's direction and plane vectors
                            rotatePlayer(&player, 1);
                            break;
                        case SDLK_e: // Rotate right
                            // Rotate player's direction and plane vectors
                            rotatePlayer(&player, -1);
                            break;
                    break;
                    }
                break;
            }
		}

        const uint8_t* keystate = SDL_GetKeyboardState(NULL);
        if (keystate[SDL_SCANCODE_ESCAPE]) state.quit = true;

        Vec2F deltaPos = {
            .x = player.dir.x * moveSpeed,
            .y = player.dir.y * moveSpeed,
        };
        if (keystate[SDL_SCANCODE_W]) { // forwards
            if (MAP[xy2index(
                        player.pos.x + deltaPos.x, 
                        player.pos.y, 
                        MAP_SIZE)] == 0) {
                player.pos.x += deltaPos.x;
            }
            if (MAP[xy2index(
                        player.pos.x, 
                        player.pos.y + deltaPos.y, 
                        MAP_SIZE)] == 0) {
                player.pos.y += deltaPos.y;
            }
        }
        if (keystate[SDL_SCANCODE_S]) { // backwards
            if (MAP[xy2index(
                        player.pos.x - deltaPos.x, 
                        player.pos.y, 
                        MAP_SIZE)] == 0) {
                player.pos.x -= deltaPos.x;
            }
            if (MAP[xy2index(
                        player.pos.x, 
                        player.pos.y - deltaPos.y, 
                        MAP_SIZE)] == 0) {
                player.pos.y -= deltaPos.y;
            }
        }
        if (keystate[SDL_SCANCODE_A]) { // strafe left
            if (MAP[xy2index(
                        player.pos.x - deltaPos.y, 
                        player.pos.y, 
                        MAP_SIZE)] == 0) {
                player.pos.x -= deltaPos.y;
            }
            if (MAP[xy2index(
                        player.pos.x, 
                        player.pos.y - -deltaPos.x, 
                        MAP_SIZE)] == 0) {
                player.pos.y -= -deltaPos.x;
            }
        }
        if (keystate[SDL_SCANCODE_D]) { // strafe right
            if (MAP[xy2index(
                        player.pos.x - -deltaPos.y, 
                        player.pos.y, 
                        MAP_SIZE)] == 0) {
                player.pos.x -= -deltaPos.y;
            }
            if (MAP[xy2index(
                        player.pos.x, 
                        player.pos.y - deltaPos.x, 
                        MAP_SIZE)] == 0) {
                player.pos.y -= deltaPos.x;
            }
        }

        SDL_SetRenderDrawColor(state.renderer, 0x18, 0x18, 0x18, 0xFF);
        SDL_RenderClear(state.renderer);

		render(&state, &player);

		SDL_RenderPresent(state.renderer);
	}

	SDL_DestroyRenderer(state.renderer);
	SDL_DestroyWindow(state.window);
	return 0;
}
