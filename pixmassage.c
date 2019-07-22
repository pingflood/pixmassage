#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include "font.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>

#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/fb.h>

#include <stdio.h>
#include <stdlib.h>

#include <linux/limits.h>

static const int SDL_WAKEUPEVENT = SDL_USEREVENT+1;

#ifndef TARGET_RETROFW
	#define system(x) printf(x); printf("\n")
#endif

#ifndef TARGET_RETROFW
	#define DBG(x) printf("%s:%d %s %s\n", __FILE__, __LINE__, __func__, x);
#else
	#define DBG(x)
#endif


#define WIDTH  320
#define HEIGHT 240

#define GPIO_BASE		0x10010000
#define PAPIN			((0x10010000 - GPIO_BASE) >> 2)
#define PBPIN			((0x10010100 - GPIO_BASE) >> 2)
#define PCPIN			((0x10010200 - GPIO_BASE) >> 2)
#define PDPIN			((0x10010300 - GPIO_BASE) >> 2)
#define PEPIN			((0x10010400 - GPIO_BASE) >> 2)
#define PFPIN			((0x10010500 - GPIO_BASE) >> 2)

#define BTN_X			SDLK_SPACE
#define BTN_A			SDLK_LCTRL
#define BTN_B			SDLK_LALT
#define BTN_Y			SDLK_LSHIFT
#define BTN_L			SDLK_TAB
#define BTN_R			SDLK_BACKSPACE
#define BTN_START		SDLK_RETURN
#define BTN_SELECT		SDLK_ESCAPE
#define BTN_BACKLIGHT	SDLK_3
#define BTN_POWER		SDLK_END
#define BTN_UP			SDLK_UP
#define BTN_DOWN		SDLK_DOWN
#define BTN_LEFT		SDLK_LEFT
#define BTN_RIGHT		SDLK_RIGHT
#define GPIO_TV			SDLK_WORLD_0
#define GPIO_MMC		SDLK_WORLD_1
#define GPIO_USB		SDLK_WORLD_2
#define GPIO_PHONES		SDLK_WORLD_3

const int	HAlignLeft		= 1,
			HAlignRight		= 2,
			HAlignCenter	= 4,
			VAlignTop		= 8,
			VAlignBottom	= 16,
			VAlignMiddle	= 32;

SDL_RWops *rw;
TTF_Font *font = NULL;
SDL_Surface *screen = NULL;
SDL_Rect bgrect;
SDL_Rect ptrect;
SDL_Event event;
SDL_TimerID timer_running = NULL;

SDL_Color txtColor = {200, 200, 220};
SDL_Color titleColor = {200, 200, 0};
SDL_Color subTitleColor = {0, 200, 0};
SDL_Color powerColor = {200, 0, 0};

int running = 0;

static char buf[1024];

uint8_t *keys;

extern uint8_t rwfont[];

int draw_text(int x, int y, const char buf[64], SDL_Color txtColor, int align) {
	DBG("");

	SDL_Surface *msg = TTF_RenderText_Blended(font, buf, txtColor);

	if (align & HAlignCenter) {
		x -= msg->w / 2;
	} else if (align & HAlignRight) {
		x -= msg->w;
	}

	if (align & VAlignMiddle) {
		y -= msg->h / 2;
	} else if (align & VAlignTop) {
		y -= msg->h;
	}

	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	rect.w = msg->w;
	rect.h = msg->h;
	SDL_BlitSurface(msg, NULL, screen, &rect);
	SDL_FreeSurface(msg);
	return msg->w;
}

void draw_background(const char buf[64]) {
	DBG("");
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));

	// title
	draw_text(310, 4, "RetroFW", titleColor, VAlignBottom | HAlignRight);
	draw_text(10, 4, buf, titleColor, VAlignBottom);

	if (running)
		draw_text(310, 230, "START: Stop", txtColor, VAlignMiddle | HAlignRight);
	else {
		draw_text(310, 230, "START: Start", txtColor, VAlignMiddle | HAlignRight);
		draw_text(10, 230, "<^v>: Move    L/R: Size    SELECT: Exit", txtColor, VAlignMiddle | HAlignLeft);
	}
}

void draw_point() {
	SDL_FillRect(screen, &ptrect, SDL_MapRGB(screen->format, 200, 200, 200));
}

static int draw_massage(void *ptr) {
	int i = 0;
	while (running) {
		i++;
		if (i > 3) i = 1;

		SDL_FillRect(screen, &ptrect, SDL_MapRGB(screen->format, 255 * (i == 1), 255 * (i == 2), 255 * (i == 3)));
		SDL_Flip(screen);
		SDL_Delay(30);
	}
	return 1;
}

void quit(int err) {
	DBG("");
	system("sync");
	if (font) TTF_CloseFont(font);
	font = NULL;
	SDL_Quit();
	TTF_Quit();
	exit(err);
}

void pushEvent() {
	SDL_Event user_event;
	user_event.type = SDL_KEYUP;
	SDL_PushEvent(&user_event);
}

int main(int argc, char* argv[]) {
	DBG("");
	signal(SIGINT, &quit);
	signal(SIGSEGV,&quit);
	signal(SIGTERM,&quit);

	char title[64] = "";
	keys = SDL_GetKeyState(NULL);

	sprintf(title, "PIX MASSAGE");
	printf("%s\n", title);

	setenv("SDL_NOMOUSE", "1", 1);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
		printf("Could not initialize SDL: %s\n", SDL_GetError());
		return -1;
	}
	SDL_PumpEvents();
	SDL_ShowCursor(0);

	screen = SDL_SetVideoMode(WIDTH, HEIGHT, 16, SDL_SWSURFACE);
	SDL_EnableKeyRepeat(200, 3);

	if (TTF_Init() == -1) {
		printf("failed to TTF_Init\n");
		return -1;
	}
	rw = SDL_RWFromMem(rwfont, sizeof(rwfont));
	font = TTF_OpenFontRW(rw, 1, 8);
	TTF_SetFontHinting(font, TTF_HINTING_NORMAL);
	TTF_SetFontOutline(font, 0);

	ptrect.w = 8;
	ptrect.h = 8;
	ptrect.x = (WIDTH - ptrect.w) / 2;
	ptrect.y = (HEIGHT - ptrect.h) / 2;

	int loop = 1;
	do {
		draw_background(title);

		draw_point();

		SDL_Flip(screen);
		while (SDL_WaitEvent(&event)) {
			if (event.type == SDL_KEYDOWN && keys[BTN_START]) {
				if (running) {
					running = 0;
				} else {
					running = 1;
					SDL_Thread *thread = SDL_CreateThread(draw_massage, (void *)NULL);
				}
			} else if (running) {
				// do nothing
			} else if (event.type == SDL_KEYDOWN) {
				if (keys[BTN_SELECT]) {
					loop = 0;
					break;
				}
				if (keys[BTN_LEFT]) ptrect.x -= 1;
				if (keys[BTN_RIGHT]) ptrect.x += 1;
				if (keys[BTN_UP]) ptrect.y -= 1;
				if (keys[BTN_DOWN]) ptrect.y += 1;

				if (keys[BTN_R]) {
					ptrect.w += 1;
					ptrect.h += 1;
				};
				if (keys[BTN_L]) {
					ptrect.w -= 1;
					ptrect.h -= 1;
				};

				if (ptrect.w < 1) ptrect.w = 1;
				if (ptrect.h < 1) ptrect.h = 1;

				if (ptrect.w > WIDTH) ptrect.w = WIDTH;
				if (ptrect.h > HEIGHT) ptrect.h = HEIGHT;

				if (ptrect.w > ptrect.h) ptrect.w = ptrect.h;
				if (ptrect.h > ptrect.w) ptrect.h = ptrect.w;

				if (ptrect.x < 0) ptrect.x = 0;
				if (ptrect.x + ptrect.w > WIDTH) ptrect.x = WIDTH - ptrect.w;
				if (ptrect.y < 0) ptrect.y = 0;
				if (ptrect.y + ptrect.h > HEIGHT) ptrect.y = HEIGHT - ptrect.h;
				break;
			}

			if (event.type == SDL_KEYUP) {
				break;
			}
		}
	} while (loop);

	quit(0);
	return 0;
}
