#ifndef _SDL2SCREEN_H
#define _SDL2SCREEN_H

#include <SDL2/SDL.h>

int QLSDLScreen(void);
void QLSDLRenderScreen(void);
int QLSDLProcessEvents(void);
void QLSDLExit(void);
Uint32 QLSDL50Hz(Uint32 interval, void *param);
void QLSDLUpdatePixelBuffer(void);
void QLSDLUpdateScreenByte(uint32_t, uint8_t);
void QLSDLUpdateScreenWord(uint32_t, uint16_t);
void QLSDLUpdateScreenLong(uint32_t, uint32_t);


extern unsigned int sdl_keyrow[8];
extern int sdl_shiftstate,sdl_controlstate, sdl_altstate;

extern SDL_atomic_t doPoll;

#endif

