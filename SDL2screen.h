#ifndef _SDL2SCREEN_H
#define _SDL2SCREEN_H

int QLSDLScreen(void);
int QLSDLRenderScreen(void);
void QLSDLProcessEvents(void);
void QLSDLExit(void);

extern int sdl_keyrow[8];
extern int sdl_shiftstate,sdl_controlstate, sdl_altstate;

#endif

