/*
 *
 */
#include <inttypes.h>
#include <SDL.h>

#include "QL_hardware.h"
#include "uqlx_cfg.h"
#include "QL68000.h"
#include "SDL2screen.h"
#include "qlmouse.h"
#include "qx_proto.h"
#include "QL_screen.h"

static SDL_Window *ql_window = NULL;
static uint32_t ql_windowid = 0;
static SDL_Surface *ql_screen = NULL;
static SDL_Renderer *ql_renderer = NULL;
static SDL_Texture *ql_texture = NULL;
static SDL_Rect dest_rect;
static SDL_DisplayMode sdl_mode;
static SDL_TimerID fiftyhz_timer;
const char *sdl_video_driver;
static char sdl_win_name[128];
static char ql_fullscreen = false;

SDL_atomic_t doPoll;
SDL_atomic_t screenUpdate;

struct QLcolor {
	int r;
	int g;
	int b;
};

struct QLcolor QLcolors[8] = {
	{ 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0xFF }, { 0xFF, 0x00, 0x00 },
	{ 0xFF, 0x00, 0xFF }, { 0x00, 0xFF, 0x00 }, { 0x00, 0xFF, 0xFF },
	{ 0xFF, 0xFF, 0x00 }, { 0xFF, 0xFF, 0xFF },
};

uint32_t SDLcolors[8];

static void QLSDLSetDestRect(int w, int h)
{
	float target_aspect = (float)qlscreen.xres / (float)qlscreen.yres;
	float window_aspect = (float)w / (float)h;
	float scale;

	if (target_aspect > window_aspect) {
		scale = (float)w / (float)qlscreen.xres;
		dest_rect.x = 0;
		dest_rect.y =
			ceilf((float)h - (float)qlscreen.yres * scale) / 2.0;
		dest_rect.w = w;
		dest_rect.h = ceilf((float)qlscreen.yres * scale);
	} else {
		scale = (float)h / (float)qlscreen.yres;
		dest_rect.x =
			ceilf((float)w - ((float)qlscreen.xres * scale)) / 2.0;
		dest_rect.y = 0;
		dest_rect.w = ceilf((float)qlscreen.xres * scale);
		dest_rect.h = h;
	}
}

int QLSDLScreen(void)
{
	uint32_t sdl_window_mode;
	int i, w, h;

	snprintf(sdl_win_name, 128, "sQLux - %s, %dK", QMD.sysrom, RTOP / 1024);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
		printf("SDL_Init Error: %s\n", SDL_GetError());
		exit(-1);
	}

	sdl_video_driver = SDL_GetCurrentVideoDriver();
	SDL_GetCurrentDisplayMode(0, &sdl_mode);

	printf("Video Driver %s xres %d yres %d\n", sdl_video_driver,
	       sdl_mode.w, sdl_mode.h);

	if (sdl_video_driver != NULL &&
		    (strcmp(sdl_video_driver, "x11") == 0) ||
	    (strcmp(sdl_video_driver, "cocoa") == 0) && sdl_mode.w >= 800 &&
		    sdl_mode.h >= 600) {
		sdl_window_mode = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
				  SDL_WINDOW_MAXIMIZED;
	} else {
		sdl_window_mode = SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	ql_window = SDL_CreateWindow(sdl_win_name, SDL_WINDOWPOS_CENTERED,
				     SDL_WINDOWPOS_CENTERED, qlscreen.xres,
				     qlscreen.yres, sdl_window_mode);

	if (ql_window == NULL) {
		printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
		return 0;
	}

	ql_windowid = SDL_GetWindowID(ql_window);

	ql_renderer = SDL_CreateRenderer(ql_window, -1,
					 SDL_RENDERER_ACCELERATED |
						 SDL_RENDERER_PRESENTVSYNC);

	SDL_GetRendererOutputSize(ql_renderer, &w, &h);

	QLSDLSetDestRect(w, h);

	SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");

	ql_screen = SDL_CreateRGBSurfaceWithFormat(
		0, qlscreen.xres, qlscreen.yres, 32, SDL_PIXELFORMAT_RGBA32);

	if (ql_screen == NULL) {
		printf("Error Creating Surface\n");
	}

	ql_texture = SDL_CreateTexture(ql_renderer, SDL_PIXELFORMAT_RGBA32,
				       SDL_TEXTUREACCESS_STREAMING,
				       ql_screen->w, ql_screen->h);

	for (i = 0; i < 8; i++)
		SDLcolors[i] = SDL_MapRGB(ql_screen->format, QLcolors[i].r,
					  QLcolors[i].g, QLcolors[i].b);

	SDL_AtomicSet(&doPoll, 0);
	fiftyhz_timer = SDL_AddTimer(20, QLSDL50Hz, NULL);
}

void QLSDLUpdateScreenByte(uint32_t offset, uint8_t data)
{
	int t1, t2, i, color;
	uint16_t dataword = 0;

	if (offset & 1) {
		offset--;
		dataword = (uint8_t)ReadByte(qlscreen.qm_lo + offset) << 8;
		dataword |= data & 0xFF;
	} else {
		dataword = (uint16_t)data << 8;
		dataword |=
			(uint8_t)ReadByte((qlscreen.qm_lo + offset) + 1) & 0xFF;
	}

	QLSDLUpdateScreenWord(offset, dataword);
}

void QLSDLUpdateScreenWord(uint32_t offset, uint16_t data)
{
	int t1, t2, i, color;
	uint32_t *pixel_ptr32;

	t1 = data >> 8;
	t2 = data & 0xFF;

	if (SDL_MUSTLOCK(ql_screen)) {
		SDL_LockSurface(ql_screen);
	}

	pixel_ptr32 = ql_screen->pixels + (offset * 16);

	if (display_mode == 8) {
		for (i = 0; i < 8; i += 2) {
			uint32_t x;

			color = ((t1 & 2) << 1) + ((t2 & 3)) + ((t1 & 1) << 3);

			x = SDLcolors[color];

			*(pixel_ptr32 + 7 - (i)) = x;
			*(pixel_ptr32 + 7 - (i + 1)) = x;

			t1 >>= 2;
			t2 >>= 2;
		}
	} else {
		for (i = 0; i < 8; i++) {
			uint32_t x;

			color = ((t1 & 1) << 2) + ((t2 & 1) << 1) +
				((t1 & 1) & (t2 & 1));

			x = SDLcolors[color];

			*(pixel_ptr32 + 7 - i) = x;

			t1 >>= 1;
			t2 >>= 1;
		}
	}

	if (SDL_MUSTLOCK(ql_screen)) {
		SDL_UnlockSurface(ql_screen);
	}

	SDL_AtomicSet(&screenUpdate, 1);
}

void QLSDLUpdateScreenLong(uint32_t offset, uint32_t data)
{
	QLSDLUpdateScreenWord(offset, data >> 16);
	QLSDLUpdateScreenWord(offset + 2, data & 0xFFFF);
}

void QLSDLUpdatePixelBuffer()
{
	uint8_t *scr_ptr = (void *)theROM + qlscreen.qm_lo;
	uint32_t *pixel_ptr32;
	int t1, t2, i, color;

	if (SDL_MUSTLOCK(ql_screen)) {
		SDL_LockSurface(ql_screen);
	}

	pixel_ptr32 = ql_screen->pixels;

	while (scr_ptr <
	       (uint8_t *)((void *)theROM + qlscreen.qm_lo + qlscreen.qm_len)) {
		t1 = *scr_ptr++;
		t2 = *scr_ptr++;

		if (display_mode == 8) {
			for (i = 0; i < 8; i += 2) {
				uint32_t x;

				color = ((t1 & 2) << 1) + ((t2 & 3)) +
					((t1 & 1) << 3);

				x = SDLcolors[color];

				*(pixel_ptr32 + 7 - (i)) = x;
				*(pixel_ptr32 + 7 - (i + 1)) = x;

				t1 >>= 2;
				t2 >>= 2;
			}
		} else {
			for (i = 0; i < 8; i++) {
				uint32_t x;

				color = ((t1 & 1) << 2) + ((t2 & 1) << 1) +
					((t1 & 1) & (t2 & 1));

				x = SDLcolors[color];

				*(pixel_ptr32 + 7 - i) = x;

				t1 >>= 1;
				t2 >>= 1;
			}
		}
		pixel_ptr32 += 8;
	}

	if (SDL_MUSTLOCK(ql_screen)) {
		SDL_UnlockSurface(ql_screen);
	}
}

void QLSDLRenderScreen(void)
{
	void *texture_buffer;
	int pitch;
	int w, h;
	SDL_PixelFormat pixelformat;

	if (!SDL_AtomicGet(&screenUpdate))
		return;

	SDL_UpdateTexture(ql_texture, NULL, ql_screen->pixels,
			  ql_screen->pitch);
	SDL_RenderClear(ql_renderer);
	SDL_RenderCopyEx(ql_renderer, ql_texture, NULL, &dest_rect, 0, NULL,
			 SDL_FLIP_NONE);
	SDL_RenderPresent(ql_renderer);

	SDL_AtomicSet(&screenUpdate, 0);
}

void SDLQLFullScreen(void)
{
	int w, h;

	ql_fullscreen ^= 1;

	if (ql_fullscreen)
		SDL_RestoreWindow(ql_window);
	SDL_SetWindowFullscreen(ql_window, ql_fullscreen);
	if (!ql_fullscreen)
		SDL_MaximizeWindow(ql_window);

	QLSDLProcessEvents();
}

/* Store the keys pressed */
unsigned int sdl_keyrow[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
int sdl_shiftstate, sdl_controlstate, sdl_altstate;

static void SDLQLKeyrowChg(int code, int press)
{
	int j;

	if (code > -1) {
		j = 1 << (code % 8);

		if (press)
			sdl_keyrow[7 - code / 8] |= j;
		else
			sdl_keyrow[7 - code / 8] &= ~j;
	}
}

struct SDLQLMap {
	SDL_Keycode sdl_kc;
	int code;
	int qchar;
};

static struct SDLQLMap sdlqlmap[] = { { SDLK_LEFT, 49, 0 },
				      { SDLK_UP, 50, 0 },
				      { SDLK_RIGHT, 52, 0 },
				      { SDLK_DOWN, 55, 0 },

				      { SDLK_F1, 57, 0 },
				      { SDLK_F2, 59, 0 },
				      { SDLK_F3, 60, 0 },
				      { SDLK_F4, 56, 0 },
				      { SDLK_F5, 61, 0 },

				      { SDLK_RETURN, 48, 0 },
				      { SDLK_SPACE, 54, 0 },
				      { SDLK_TAB, 19, 0 },
				      { SDLK_ESCAPE, 51, 0 },
				      //{SDLK_CAPSLOCK,     33, 0},
				      { SDLK_RIGHTBRACKET, 40, 0 },
				      { SDLK_5, 58, 0 },
				      { SDLK_4, 62, 0 },
				      { SDLK_7, 63, 0 },
				      { SDLK_LEFTBRACKET, 32, 0 },
				      { SDLK_z, 41, 0 },

				      { SDLK_PERIOD, 42, 0 },
				      { SDLK_c, 43, 0 },
				      { SDLK_b, 44, 0 },
				      { SDLK_BACKQUOTE, 45, 0 },

				      { SDLK_m, 46, 0 },
				      { SDLK_QUOTE, 47, 0 },
				      { SDLK_BACKSLASH, 53, 0 },
				      { SDLK_k, 34, 0 },
				      { SDLK_s, 35, 0 },
				      { SDLK_f, 36, 0 },
				      { SDLK_EQUALS, 37, 0 },
				      { SDLK_g, 38, 0 },
				      { SDLK_SEMICOLON, 39, 0 },
				      { SDLK_l, 24, 0 },
				      { SDLK_3, 25, 0 },
				      { SDLK_h, 26, 0 },
				      { SDLK_1, 27, 0 },
				      { SDLK_a, 28, 0 },
				      { SDLK_p, 29, 0 },
				      { SDLK_d, 30, 0 },
				      { SDLK_j, 31, 0 },
				      { SDLK_9, 16, 0 },
				      { SDLK_w, 17, 0 },
				      { SDLK_i, 18, 0 },
				      { SDLK_r, 20, 0 },
				      { SDLK_MINUS, 21, 0 },
				      { SDLK_y, 22, 0 },
				      { SDLK_o, 23, 0 },
				      { SDLK_8, 8, 0 },
				      { SDLK_2, 9, 0 },
				      { SDLK_6, 10, 0 },
				      { SDLK_q, 11, 0 },
				      { SDLK_e, 12, 0 },
				      { SDLK_0, 13, 0 },
				      { SDLK_t, 14, 0 },
				      { SDLK_u, 15, 0 },
				      { SDLK_x, 3, 0 },
				      { SDLK_v, 4, 0 },
				      { SDLK_SLASH, 5, 0 },
				      { SDLK_n, 6, 0 },
				      { SDLK_COMMA, 7, 0 },
				      /*
  {SDLK_Next,-1,220},
  {SDLK_Prior,-1,212},
  {SDLK_Home,-1,193},
  {SDLK_End,-1,201},
		 */
				      { 0, 0, 0 } };

void QLSDProcessKey(SDL_Keysym *keysym, int pressed)
{
	int i = 0;
	int mod = 0;

	/* Special case backspace */
	if ((keysym->sym == SDLK_BACKSPACE) && pressed) {
		queueKey(1 << 1, 49, 0);
		return;
	}

	switch (keysym->sym) {
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		sdl_shiftstate = pressed;
		break;
	case SDLK_LCTRL:
	case SDLK_RCTRL:
		sdl_controlstate = pressed;
		break;
	case SDLK_LALT:
	case SDLK_RALT:
		sdl_altstate = pressed;
		break;
	case SDLK_F11:
		if (pressed)
			SDLQLFullScreen();
		return;
	}

	mod = sdl_altstate | sdl_controlstate << 1 | sdl_shiftstate << 2;

	while (sdlqlmap[i].sdl_kc != 0) {
		if (keysym->sym == sdlqlmap[i].sdl_kc) {
			if (pressed)
				queueKey(mod, sdlqlmap[i].code, 0);
			SDLQLKeyrowChg(sdlqlmap[i].code, pressed);
		}
		i++;
	}
}

static void QLSDLProcessMouse(int x, int y)
{
	int qlx = 0, qly = 0;
	float x_ratio, y_ratio;

	if (SDL_GetWindowFlags(ql_window) & SDL_WINDOW_ALLOW_HIGHDPI) {
		x *= 2;
		y *= 2;
	}

	if (x < dest_rect.x) {
		qlx = 0;
	} else if (x > (dest_rect.w + dest_rect.x)) {
		qlx = qlscreen.xres - 1;
	} else {
		x_ratio = (float)dest_rect.w / (float)qlscreen.xres;

		x -= dest_rect.x;

		qlx = ((float)x / x_ratio);
	}

	if (y < dest_rect.y) {
		qly = 0;
	} else if (y > (dest_rect.h + dest_rect.y)) {
		qly = qlscreen.yres - 1;
	} else {
		y_ratio = (float)dest_rect.h / (float)qlscreen.yres;

		y -= dest_rect.y;

		qly = ((float)y / y_ratio);
	}

	QLMovePointer(qlx, qly);
}

int QLSDLProcessEvents(void)
{
	SDL_Event event;
	int keypressed;
	int w, h;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_KEYDOWN:
			QLSDProcessKey(&event.key.keysym, 1);
			break;
		case SDL_KEYUP:
			QLSDProcessKey(&event.key.keysym, 0);
			break;
		case SDL_QUIT:
			cleanup(0);
			break;
		case SDL_MOUSEMOTION:
			QLSDLProcessMouse(event.motion.x, event.motion.y);
			//inside=1;
			break;
		case SDL_MOUSEBUTTONDOWN:
			QLButton(event.button.button, 1);
			break;
		case SDL_MOUSEBUTTONUP:
			QLButton(event.button.button, 0);
			break;
		case SDL_WINDOWEVENT:
			if (event.window.windowID == ql_windowid) {
				switch (event.window.event) {
				case SDL_WINDOWEVENT_RESIZED:
					SDL_GetRendererOutputSize(ql_renderer,
								  &w, &h);
					QLSDLSetDestRect(w, h);
					SDL_AtomicSet(&screenUpdate, 1);
					QLSDLRenderScreen();
					break;
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					SDL_GetRendererOutputSize(ql_renderer,
								  &w, &h);
					QLSDLSetDestRect(w, h);
					SDL_AtomicSet(&screenUpdate, 1);
					QLSDLRenderScreen();
					break;
				}
			}
			break;
		default:
			break;
		}
	}

	QLSDLRenderScreen();
}

void QLSDLExit(void)
{
	SDL_Quit();
}

Uint32 QLSDL50Hz(Uint32 interval, void *param)
{
	SDL_AtomicSet(&doPoll, 1);

	schedCount = 0;

	return interval;
}
