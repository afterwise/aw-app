
#ifndef AW_APP_H
#define AW_APP_H

#if !_MSC_VER
# include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* runtime */

enum {
	APP_QUIT = 1 << 0,
	APP_FOCUS = 1 << 1,
	APP_PAUSE = 1 << 2,

	APP_RESIZE = 1 << 8,
	APP_LOWMEM = 1 << 9
};

extern void *app_window;
extern unsigned app_width;
extern unsigned app_height;
extern float app_aspect;
extern unsigned app_state;

void app_init(const char *caption);
void app_end();

bool app_update();
void app_quit();

/* user main */

extern void app_main();

/* input events */

#ifndef APP_MAXMOTIONS
# define APP_MAXMOTIONS (4)
#endif

enum {
	/* events codes */
	EVENT_BEGIN = 0x1 << 0,
	EVENT_END = 0x2 << 0,
	EVENT_CANCEL = 0x4 << 0,

	/* key flags */
	EVENT_KEYDOWN = 0x1 << 4,

	/* motion flags */
	EVENT_HOVER = 0x1 << 4,
	EVENT_TOUCH = 0x2 << 4,
};

enum {
	KEY_BACKSPACE = 0x08,
	KEY_RETURN = 0x0a,
	KEY_SPACE = 0x20,
	KEY_TAB = 0x09,
	KEY_ESCAPE = 0x1b,
	KEY_RIGHT = 0x1c,
	KEY_LEFT = 0x1d,
	KEY_DOWN = 0x1e,
	KEY_UP = 0x1f,
};

struct motion {
	unsigned char id;
	unsigned char event;
	float x, y;
	float dx, dy;
};

extern unsigned char app_keys[128];

extern unsigned app_motioncount;
extern struct motion app_motions[APP_MAXMOTIONS];

/* pad state */

#ifndef APP_MAXPADS
# define APP_MAXPADS (1)
#endif

enum {
	PAD_X,
	PAD_Y,
	PAD_Z,
	PAD_NUMAXES
};

enum {
	PAD_SELECT,
	PAD_L3,
	PAD_R3,
	PAD_START,
	PAD_UP,
	PAD_DOWN,
	PAD_LEFT,
	PAD_RIGHT,
	PAD_L2,
	PAD_R2,
	PAD_L1,
	PAD_R1,
	PAD_TRIANGLE,
	PAD_CROSS,
	PAD_SQUARE,
	PAD_CIRCLE,
	PAD_NUMBUTTONS
};

enum {
	PAD_LSTICK,
	PAD_RSTICK,
	PAD_NUMSTICKS
};

struct pad_stick {
	float x, y;
	float deadzone;
};

struct pad {
	unsigned char valid;
	unsigned short held;
	unsigned short pressed;
	unsigned short released;
	float pressure[PAD_NUMBUTTONS];
	struct pad_stick sticks[PAD_NUMSTICKS];
	float tilt[PAD_NUMAXES];
	float gyro;
};

extern struct pad app_pads[APP_MAXPADS];

bool pad_intercepted();
bool pad_connected(unsigned i);

/* user interaction */

enum ui_state {
	UI_COLD,
	UI_HOT,
	UI_TOUCHED,
	UI_RELEASED
};

const struct motion *ui_motion;

enum ui_state ui_state(unsigned id, float x, float y, float w, float h);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AW_APP_H */

