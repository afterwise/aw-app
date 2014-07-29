
#include "aw-app.h"
#include "aw-arith.h"

#if __APPLE__
# include <TargetConditionals.h>
#endif

#if __ANDROID__
# include "aw-thread.h"
#endif

#if __CELLOS_LV2__
# include <cell/pad.h>
# include <sysutil/sysutil_common.h>
#endif

#if _WIN32
# include <XInput.h>
#endif

#if _WIN32 || __linux__ || __APPLE__ && !TARGET_OS_IPHONE
# define GLFW_INCLUDE_NONE
# include <GLFW/glfw3.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *app_window;
unsigned app_width;
unsigned app_height;
float app_aspect;

static unsigned stateghost;
unsigned app_state;

static unsigned char keyghost[128];
unsigned char app_keys[128];

unsigned app_motioncount;
static struct motion motionghost[APP_MAXMOTIONS];
struct motion app_motions[APP_MAXMOTIONS];

#if __CELLOS_LV2__
static CellPadInfo2 pad_info;
static CellPadData pad_data[APP_MAXPADS];
#elif _WIN32
static XINPUT_STATE pad_data[APP_MAXPADS];
#endif

struct pad app_pads[APP_MAXPADS];

const struct motion *ui_motion;
static unsigned ui_active;
static unsigned ui_hot;

/* plumbing */

#if __ANDROID__
static thread_id_t thread;
static sema_id_t sema;
static ANativeActivity *activity;
static ALooper *looper;
static AInputQueue *inputq;

static void handle_destroy(ANativeActivity *a) {
	(void) a;
}

static void handle_start(ANativeActivity *a) {
	(void) a;
}

static void handle_stop(ANativeActivity *a) {
	(void) a;
}

static void handle_pause(ANativeActivity *a) {
	(void) a;

	stateghost |= APP_PAUSE;
}

static void handle_resume(ANativeActivity *a) {
	(void) a;

	stateghost &= ~APP_PAUSE;
}

static void handle_focus(ANativeActivity *a, int focus) {
	if (focus)
		stateghost |= APP_FOCUS;
	else
		stateghost &= ~APP_FOCUS;
}

static void *handle_save(ANativeActivity *a, size_t *size) {
	(void) a;
}

static void handle_wcreate(ANativeActivity *a, ANativeWindow *w) {
	(void) a;
}

static void handle_wdestroy(ANativeActivity *a, ANativeWindow *w) {
	(void) a;
}

static void handle_wresize(ANativeActivity *a, ANativeWindow *w) {
	app_width = ANativeWindow_getWidth(w);
	app_height = ANativeWindow_getHeight(w);
	app_aspect = (float) app_width / (float) app_height;
	stateghost |= APP_RESIZE;
}

static void handle_qcreate(ANativeActivity *a, AInputQueue *q) {
	(void) a;

	if (inputq != NULL)
		AInputQueue_detachLooper(inputq);

	inputq = q;
	AInputQueue_attachLooper(q, looper, 0, NULL, NULL);
}

static void handle_qdestroy(ANativeActivity *a, AInputQueue *q) {
	(void) a;

	if (inputq == q) {
		AInputQueue_detachLooper(q);
		inputq = NULL;
	}
}

static void handle_lowmem(ANativeActivity *a) {
	(void) a;

	stateghost |= APP_LOWMEM;
}

static bool handle_key(AInputEvent *e) {
	unsigned key = AKeyEvent_getKeyCode(e);
	unsigned action = AKeyEvent_getAction(e);
	unsigned x =
		EVENT_KEYDOWN |
		(action == AKEY_EVENT_ACTION_DOWN ? EVENT_BEGIN : 0) |
		(action == AKEY_EVENT_ACTION_UP ? EVENT_END : 0);
	bool handled = true;

	if (key >= AKEYCODE_0 && key <= AKEYCODE_9)
		keyghost['0' + (key - AKEYCODE_0)] = x;
	else if (key >= AKEYCODE_A && key <= AKEYCODE_Z)
		keyghost['A' + (key - AKEYCODE_A)] = x;
	else if (key == AKEYCODE_BACKSPACE)
		keyghost[KEY_BACKSPACE] = x;
	else if (key == AKEYCODE_ENTER)
		keyghost[KEY_RETURN] = x;
	else if (key == AKEYCODE_SPACE)
		keyghost[KEY_SPACE] = x;
	else if (key == AKEYCODE_TAB)
		keyghost[KEY_TAB] = x;
	else if (key == AKEYCODE_ESCAPE)
		keyghost[KEY_ESCAPE] = x;
	else if (key == AKEYCODE_DPAD_UP)
		keyghost[KEY_UP] = x;
	else if (key == AKEYCODE_DPAD_DOWN)
		keyghost[KEY_DOWN] = x;
	else if (key == AKEYCODE_DPAD_LEFT)
		keyghost[KEY_LEFT] = x;
	else if (key == AKEYCODE_DPAD_RIGHT)
		keyghost[KEY_RIGHT] = x;
	else
		handled = false;

	return handled;
}

static bool handle_motion(AInputEvent *e) {
	int i, n = AMotionEvent_getPointerCount(e);
	bool handled = true;

	if (n > APP_MAXMOTIONS)
		n = APP_MAXMOTIONS;

	for (i = 0; i < n; ++i) {
		motionghost[i].id = AMotionEvent_getPointerId(e, i);
		motionghost[i].x = AMotionEvent_getX(e, i);
		motionghost[i].y = AMotionEvent_getY(e, i);

		switch (AMotionEvent_getAction(e) & AMOTION_EVENT_ACTION_MASK) {
		case AMOTION_EVENT_ACTION_DOWN:
			motionghost[i].state = EVENT_BEGIN | EVENT_TOUCH;
			break;

		case AMOTION_EVENT_ACTION_UP:
			motionghost[i].state = EVENT_END | EVENT_TOUCH;
			break;

		case AMOTION_EVENT_ACTION_CANCEL:
			motionghost[i].state = EVENT_CANCEL | EVENT_TOUCH;
			break;

		case AMOTION_EVENT_ACTION_MOVE:
			motionghost[i].state = EVENT_TOUCH;
			break;

		case AMOTION_EVENT_ACTION_HOVER_ENTER:
			motionghost[i].state = EVENT_BEGIN | EVENT_HOVER;
			break;

		case AMOTION_EVENT_ACTION_HOVER_EXIT:
			motionghost[i].state = EVENT_END | EVENT_HOVER;
			break;

		case AMOTION_EVENT_ACTION_HOVER_MOVE:
			motionghost[i].state = EVENT_HOVER;
			break;
		}
	}

	return handled;
}
#elif TARGET_OS_IPHONE
/* XXX */
#elif _WIN32 || __linux__ || TARGET_OS_MAC
static void handle_key(GLFWwindow *win, int key, int scancode, int action, int mods) {
	unsigned x =
		EVENT_KEYDOWN |
		(action == GLFW_PRESS ? EVENT_BEGIN : 0) |
		(action == GLFW_RELEASE ? EVENT_END : 0);

	(void) win;
	(void) scancode;
	(void) mods;

	if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
		keyghost['0' + (key - GLFW_KEY_0)] = x;
	else if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
		keyghost['A' + (key - GLFW_KEY_A)] = x;
	else if (key == GLFW_KEY_BACKSPACE)
		keyghost[KEY_BACKSPACE] = x;
	else if (key == GLFW_KEY_ENTER)
		keyghost[KEY_RETURN] = x;
	else if (key == GLFW_KEY_SPACE)
		keyghost[KEY_SPACE] = x;
	else if (key == GLFW_KEY_TAB)
		keyghost[KEY_TAB] = x;
	else if (key == GLFW_KEY_ESCAPE)
		keyghost[KEY_ESCAPE] = x;
	else if (key >= GLFW_KEY_UP)
		keyghost[KEY_UP] = x;
	else if (key >= GLFW_KEY_DOWN)
		keyghost[KEY_DOWN] = x;
	else if (key >= GLFW_KEY_LEFT)
		keyghost[KEY_LEFT] = x;
	else if (key >= GLFW_KEY_RIGHT)
		keyghost[KEY_RIGHT] = x;
}

static void handle_mousebutton(GLFWwindow *win, int button, int action, int mods) {
	unsigned i;

	(void) win;
	(void) mods;

	for (i = 0; i < app_motioncount; ++i)
		if (motionghost[i].id == button + 1)
			break;

	motionghost[i].id = button + 1;
	motionghost[i].event = (action == GLFW_PRESS ? EVENT_BEGIN : EVENT_END) | EVENT_TOUCH;

	if (i == app_motioncount) {
		motionghost[i].x = motionghost[i - 1].x;
		motionghost[i].y = motionghost[i - 1].y;
		++app_motioncount;
	}
}

static void handle_cursorpos(GLFWwindow *win, double x, double y) {
	unsigned i;

	(void) win;

	if (motionghost[0].event == 0)
		motionghost[0].event = EVENT_BEGIN | EVENT_HOVER;

	for (i = 0; i < app_motioncount; ++i) {
		motionghost[i].dx = (float) x - motionghost[i].x;
		motionghost[i].dy = (float) y - motionghost[i].y;
		motionghost[i].x = (float) x;
		motionghost[i].y = (float) y;
	}
}

static void handle_cursorenter(GLFWwindow *win, int enter) {
	unsigned i;

	(void) win;

	for (i = 0; i < app_motioncount; ++i)
		if (motionghost[i].id == 0)
			break;

	if (i == app_motioncount)
		app_motioncount++;

	motionghost[i].id = 0;
	motionghost[i].event = (enter ? EVENT_BEGIN : EVENT_END) | EVENT_HOVER;
}
#elif __CELLOS_LV2__
static void sysutil_handler(unsigned long long, status, unsigned long long param, void *data) {
	if (status == CELL_SYSUTIL_REQUEST_EXITGAME)
		app_quit();
}
#endif

/* entry points */

#if __ANDROID__
void ANativeActivity_onCreate(ANativeActivity *a, void *data, size_t size) {
	a->callbacks->onDestroy = &android_destroy;
	a->callbacks->onStart = &android_start;
	a->callbacks->onStop = &android_stop;
	a->callbacks->onPause = &android_pause;
	a->callbacks->onResume = &android_resume;
	a->callbacks->onWindowFocusChanged = &android_focus;
	a->callbacks->onSaveInstanceState = &android_save;
	a->callbacks->onNativeWindowCreated = &android_wcreate;
	a->callbacks->onNativeWindowDestroyed = &android_wdestroy;
	a->callbacks->onNativeWindowResized = &android_resize;
	a->callbacks->onInputQueueCreated = &android_qcreated;
	a->callbacks->onInputQueueDestroyed = &android_qdestroy;
	a->callbacks->onLowMemory = &android_lowmem;

	activity = a;
	sema = sema_create();
	thread = thread_spawn((thread_start_t) &app_main, THREAD_NORMAL_PRIORITY, 16 * 1024, 0);

	sema_acquire(sema, 1);
}
#elif TARGET_OS_IPHONE
/* XXX */
#elif _WIN32
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, char *args, int show) {
	(void) inst;
	(void) prev;
	(void) args;
	(void) show;

	app_main();
	return 0;
}
#elif __linux__ || TARGET_OS_MAC || __CELLOS_LV2__
int main(int argc, char *argv[]) {
	(void) argc;
	(void) argv;

	app_main();
	return 0;
}
#endif

/* user functions */

void app_init(const char *caption) {
#if __ANDROID__
	looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
	sema_release(sema, 1);
#elif TARGET_OS_IPHONE
/* XXX */
#elif _WIN32 || __linux__ || TARGET_OS_MAC
	app_motioncount = 1;

	if (!glfwInit())
		fprintf(stderr, "glfwInit: failed\n"), abort();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	if ((app_window = glfwCreateWindow(640, 480, caption, NULL, NULL)) == NULL)
		fprintf(stderr, "glfwCreateWindow: failed\n"), abort();

	glfwMakeContextCurrent(app_window);
	glfwSetKeyCallback(app_window, &handle_key);
	glfwSetMouseButtonCallback(app_window, &handle_mousebutton);
	glfwSetCursorPosCallback(app_window, &handle_cursorpos);
	glfwSetCursorEnterCallback(app_window, &handle_cursorenter);
#elif __CELLOS_LV2__
	cellSysutilRegisterCallback(0, &sysutil_handler, NULL);
        cellPadInit(APP_MAXPADS);
#endif
}

void app_end() {
#if __ANDROID__
	ANativeActivity_finish(activity);
#elif TARGET_OS_IPHONE
/* XXX */
#elif _WIN32 || __linux__ || TARGET_OS_MAC
	glfwTerminate();
#elif __CELLOS_LV2__
        cellPadEnd();
#endif
}

static void update_keys() {
	unsigned i, x;

	for (i = 0; i < 128; ++i) {
		x = keyghost[i];
		app_keys[i] = x;

		x = ((x & EVENT_END) == 0 ? x & ~0xf : 0);
		keyghost[i] = x;
	}
}

static void update_motions() {
	struct motion m;
	unsigned i;

	for (i = 0; i < app_motioncount; ++i) {
		memcpy(&m, &motionghost[i], sizeof m);

		if ((m.event & 0xf) == EVENT_BEGIN)
			motionghost[i].event &= ~0xf;
		else if ((m.event & 0xf) == EVENT_END)
			motionghost[i].event = 0;

		motionghost[i].dx = 0.f;
		motionghost[i].dy = 0.f;

		memcpy(&app_motions[i], &m, sizeof m);
	}
}

#if _WIN32 || __CELLOS_LV2__
static void update_stick(struct pad_stick *stick, float x, float y) {
	float halfpi = 1.5707963268f, quarterpi = 0.7853981634f;
	float eps = .001f, sqreps = .000001f;
	float mag, invmag, sqrmag = x * x + y * y;
	float tx = 0.f, ty = 0.f;
	float a, b, k, r;
	float dz;

	/* square input -> circular input */

	if (sqrmag >= sqreps) {
		mag = sqrtf(sqrmag);
		invmag = 1.f / mag;

		/* quadrant angle */

		a = acosf(abs_f32(invmag * y));
		b = sel_f32(quarterpi - a, a, abs_f32(halfpi - a));
		r = b / quarterpi;
		k = .164213f * r * mag;
		tx = sel_f32(x, x - k, x + k);
		ty = sel_f32(y, y - k, y + k);
	}

	/* circular dead zone */

	dz = stick->deadzone;
	sqrmag = tx * tx + ty * ty;
	x = 0.f;
	y = 0.f;

	if (sqrmag >= dz * dz && sqrmag >= sqreps)
		if (1.f / dz >= eps) {
			mag = sqrtf(sqrmag);
			x = tx * ((1.f - dz / mag) / (1.f / dz));
			y = ty * ((1.f - dz / mag) / (1.f / dz));
		}

	stick->x = x;
	stick->y = y;
}
#endif /* _WIN32 || __CELLOS_LV2__ */

#if __CELLOS_LV2__
static void update_buttonpressure_l(unsigned i, struct pad *pad) {
	float a = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_LEFT];
	float b = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_RIGHT];
	float c = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_UP];
	float d = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_DOWN];

	a *= .0039215686f; /* a = a / 255 */
	b *= .0039215686f;
	c *= .0039215686f;
	c *= .0039215686f;

	pad->pressure[PAD_LEFT] = a;
	pad->pressure[PAD_RIGHT] = b;
	pad->pressure[PAD_UP] = c;
	pad->pressure[PAD_DOWN] = d;
}

static void update_buttonpressure_r(unsigned i, struct pad *pad) {
	float a = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_TRIANGLE];
	float b = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_CIRCLE];
	float c = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_CROSS];
	float d = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_SQUARE];

	a *= .0039215686f; /* a = a / 255 */
	b *= .0039215686f;
	c *= .0039215686f;
	c *= .0039215686f;

	pad->pressure[PAD_TRIANGLE] = a;
	pad->pressure[PAD_CIRCLE] = b;
	pad->pressure[PAD_CROSS] = c;
	pad->pressure[PAD_SQUARE] = d;
}

static void update_buttonpressure_t(unsigned i, struct pad *pad) {
	float a = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_L1];
	float b = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_R1];
	float c = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_L2];
	float d = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_PRESS_R2];

	a *= .0039215686f; /* a = a / 255 */
	b *= .0039215686f;
	c *= .0039215686f;
	c *= .0039215686f;

	pad->pressure[PAD_L1] = a;
	pad->pressure[PAD_R1] = b;
	pad->pressure[PAD_L2] = c;
	pad->pressure[PAD_R2] = d;
}

static void update_buttons(unsigned i, struct pad *pad, bool pressure) {
	unsigned washeld = pad->held;
	unsigned b;

	pad->held =
		(pad_data[i].button[CELL_PAD_BTN_OFFSET_DIGITAL1] & 0xff) |
		((pad_data[i].button[CELL_PAD_BTN_OFFSET_DIGITAL2] & 0xff) << 8);

	pad->pressed = (pad->held ^ washeld) & pad->held;
	pad->released = (pad->held ^ washeld) & washeld;

	if (pressure) {
		update_buttonpressure_l(i, pad);
		update_buttonpressure_r(i, pad);
		update_buttonpressure_t(i, pad);
	} else
		for (b = 1; b < (1 << PAD_NUMBUTTONS); b <<= 1)
			pad->pressure[b] = (pad->held & b ? 1.f : 0.f);
}

static void update_sticks(unsigned i, struct pad *pad) {
	float lx = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X];
	float ly = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y];
	float rx = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X];
	float ry = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y];

	lx *= .007843137f; /* lx = lx * (2 / 255) */
	ly *= .007843137f;
	rx *= .007843137f;
	ry *= .007843137f;

	lx = clamp_f32(lx - 1.f, -1.f, 1.f);
	ly = clamp_f32(ly - 1.f, -1.f, 1.f);
	rx = clamp_f32(rx - 1.f, -1.f, 1.f);
	ry = clamp_f32(ry - 1.f, -1.f, 1.f);

	update_stick(&pad->sticks[PAD_LSTICK], lx, ly);
	update_stick(&pad->sticks[PAD_RSTICK], rx, ry);
}

static void update_axes(unsigned i, struct pad *pad) {
	float x = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_SENSOR_X];
	float y = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_SENSOR_Y];
	float z = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_SENSOR_Z];
	float g = (float) pad_data[i].button[CELL_PAD_BTN_OFFSET_SENSOR_G];

	x *= .0009775171f; /* x = x / 1023 */
	y *= .0009775171f;
	z *= .0009775171f;
	g *= .0009775171f;

	pad->tilt[PAD_X] = x;
	pad->tilt[PAD_Y] = y;
	pad->tilt[PAD_Z] = z;
	pad->gyro = g;
}

static void update_pad(unsigned i) {
	struct pad *pad = &app_pads[i];
	bool connected = pad_connected(i);
	bool changed = !!(pad_info.port_status[i] & CELL_PAD_STATUS_ASSIGN_CHANGES);
	bool pressure = !!(pad_info.port_setting[i] & CELL_PAD_SETTING_PRESS_ON);

	if (connected) {
		if (changed && !pressure)
			if (!!(pad_info.device_capability[i] & CELL_PAD_CAPABILITY_PRESS_MODE))
				if (!cellPadSetPortSetting(
						i, pad_info.port_setting[i] | CELL_PAD_SETTING_PRESS_ON))
					pressure = true;

		if (cellPadGetData(i, &pad_data[i]) < 0)
			connected = false;
	}

	if (connected || pad->valid)
		if (connected && !pad_data[i].button[0]) {
			pad->valid = 1;
			pad->pressed = 0;
			pad->released = 0;

			if (pad_data[i].len > 0) {
				update_buttons(i, pad);
				update_sticks(i, pad);
				update_axes(i, pad);
			}
		} else
			memset(pad, 0, sizeof *pad);
}
#endif /* __CELLOS_LV2__ */

#if _WIN32
static void update_buttons(unsigned i, struct pad *pad) {
	unsigned washeld = pad->held;
	unsigned xheld = pad_data[i].Gamepad.wButtons;

	pad->held = 0;

	if (xheld & XINPUT_GAMEPAD_DPAD_LEFT)
		pad->held |= PAD_LEFT;

	if (xheld & XINPUT_GAMEPAD_DPAD_RIGHT)
		pad->held |= PAD_RIGHT;

	if (xheld & XINPUT_GAMEPAD_DPAD_UP)
		pad->held |= PAD_UP;

	if (xheld & XINPUT_GAMEPAD_DPAD_DOWN)
		pad->held |= PAD_DOWN;

	if (xheld & XINPUT_GAMEPAD_START)
		pad->held |= PAD_START;

	if (xheld & XINPUT_GAMEPAD_BACK)
		pad->held |= PAD_SELECT;

	if (xheld & XINPUT_GAMEPAD_LEFT_THUMB)
		pad->held |= PAD_L3;

	if (xheld & XINPUT_GAMEPAD_RIGHT_THUMB)
		pad->held |= PAD_R3;

	if (xheld & XINPUT_GAMEPAD_LEFT_SHOULDER)
		pad->held |= PAD_L1;

	if (xheld & XINPUT_GAMEPAD_RIGHT_SHOULDER)
		pad->held |= PAD_R1;

	if (xheld & XINPUT_GAMEPAD_A)
		pad->held |= PAD_CROSS;

	if (xheld & XINPUT_GAMEPAD_B)
		pad->held |= PAD_CIRCLE;

	if (xheld & XINPUT_GAMEPAD_X)
		pad->held |= PAD_SQUARE;

	if (xheld & XINPUT_GAMEPAD_Y)
		pad->held |= PAD_TRIANGLE;

        pad->pressed = (pad->held ^ washeld) & pad->held;
        pad->released = (pad->held ^ washeld) & washeld;
}

static void update_triggers(unsigned i, struct pad *pad) {
	float l, r;

	l = (float) pad_data[i].Gamepad.bLeftTrigger;
	r = (float) pad_data[i].Gamepad.bRightTrigger;

	l *= .0039215686f; /* l = l / 255 */
	r *= .0039215686f;

	l = sel_f32(l - XINPUT_GAMEPAD_TRIGGER_THRESHOLD, l, 0f);
	r = sel_f32(r - XINPUT_GAMEPAD_TRIGGER_THRESHOLD, r, 0f);

	pad->pressure[PAD_L2] = l;
	pad->pressure[PAD_R2] = r;
}

static void update_sticks(unsigned i, struct pad *pad) {
	float lx = (float) pad_data[i].Gamepad.sThumbLX;
	float ly = (float) pad_data[i].Gamepad.sThumbLY;
	float rx = (float) pad_data[i].Gamepad.sThumbRX;
	float ry = (float) pad_data[i].Gamepad.sThumbRY;

	lx *= .000000239361f; /* lx = lx / 32767 * (2 / 255) */
	ly *= .000000239361f;
	rx *= .000000239361f;
	ry *= .000000239361f;

	lx = clamp_f32(lx - 1.f, -1.f, 1.f);
	ly = clamp_f32(ly - 1.f, -1.f, 1.f);
	rx = clamp_f32(rx - 1.f, -1.f, 1.f);
	ry = clamp_f32(ry - 1.f, -1.f, 1.f);

	update_stick(&pad->sticks[PAD_LSTICK], lx, ly);
	update_stick(&pad->sticks[PAD_RSTICK], rx, ry);
}

static void update_pad(unsigned i) {
	struct pad *pad = &app_pads[i];

	if (XInputGetState(i, &pad_data[i]) == ERROR_SUCCESS) {
		pad->valid = 1;

		update_buttons(i, pad);
		update_triggers(i, pad);
		update_sticks(i, pad);
	} else if (pad->valid)
		memset(pad, 0, sizeof *pad);
}
#endif

static void update_pads() {
#if _WIN32 || __CELLOS_LV2__
	struct pad *pad;
	unsigned i;

# if __CELLOS_LV2__
	cellPadGetInfo2(&pad_info);

	if (pad_intercepted())
		return;
# endif

	for (i = 0; i < APP_MAXPADS; ++i)
		update_pad(i);
#endif /* _WIN32 || __CELLOS_LV2__ */
}

static void update_ui() {
	unsigned i, n;

	ui_motion = NULL;

	for (i = 0, n = app_motioncount; i < n; ++i)
		if (ui_motion == NULL && (app_motions[i].event & EVENT_HOVER) != 0)
			ui_motion = &app_motions[i];
		else if (ui_motion != NULL && (ui_motion->event & EVENT_HOVER) != 0 &&
				(app_motions[i].event & EVENT_TOUCH) != 0)
			ui_motion = &app_motions[i];

	ui_hot = 0;

	if (ui_motion == NULL || (ui_motion->event & EVENT_TOUCH) == 0)
		ui_active = 0;
}

static bool update_state() {
	unsigned state = app_state;

	app_state = stateghost;
	stateghost &= ~0xffff0000;

	return !(state & APP_QUIT);
}

bool app_update() {
#if __ANDROID__
	AInputEvent *e;
	bool handled;

	if (inputq != NULL)
		while (ALooper_pollAll(0, NULL, NULL, NULL) >= 0)
			if (AInputQueue_getEvent(inputq, &e) >= 0)
				if (!AInputQueue_preDispatchEvent(inputq, e)) {
					switch (AInputEvent_getType(e)) {
					case AINPUT_EVENT_TYPE_KEY:
						handled = handle_key(e);
						break;

					case AINPUT_EVENT_TYPE_MOTION:
						handled = handle_motion(e);
						break;

					default:
						handled = false;
					}

					AInputEvent_finishEvent(inputq, e, handled);
				}
#elif TARGET_OS_IPHONE
#elif _WIN32 || __linux__ || TARGET_OS_MAC
	glfwPollEvents();
	glfwGetFramebufferSize(app_window, (int *) &app_width, (int *) &app_height);
	app_aspect = (float) app_width / (float) app_height;

	if (glfwWindowShouldClose(app_window))
		app_quit();
#elif __CELLOS_LV2__
        cellSysutilCheckCallback();
#endif

	update_keys();
	update_motions();
	update_pads();
	update_ui();

	return update_state();
}

void app_quit() {
	stateghost |= APP_QUIT;
}

bool pad_intercepted() {
#if __CELLOS_LV2__
	return !!(pad_info.system_info & CELL_PAD_INFO_INTERCEPTED);
#else
	return false;
#endif
}

bool pad_connected(unsigned i) {
#if __CELLOS_LV2__
	return !!(pad_info.port_status[i] & CELL_PAD_STATUS_CONNECTED);
#else
	(void) i;
	return false;
#endif
}

enum ui_state ui_state(unsigned id, float x, float y, float w, float h) {
	if (ui_motion != NULL)
		if (ui_motion->x >= x && ui_motion->x < x + w &&
				ui_motion->y >= y && ui_motion->y < y + h) {
			ui_hot = id;

			if (ui_motion->event == (EVENT_BEGIN | EVENT_TOUCH) && ui_active == 0)
				ui_active = id;
		}

	if (ui_active != id)
		return (ui_hot != id) ? UI_COLD : UI_HOT;

	if (ui_motion->event != (EVENT_END | EVENT_TOUCH))
		return UI_TOUCHED;

	return (ui_hot != id) ? UI_COLD : UI_RELEASED;
}

