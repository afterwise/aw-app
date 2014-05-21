
#include "aw-app.h"

#include <stdio.h>

void app_main() {
	struct motion *m;
	unsigned i, n;

	app_init("test app");

	while (app_update()) {
		switch (app_keys[KEY_SPACE]) {
		case EVENT_BEGIN | EVENT_KEYDOWN:
			printf("Space is now down\n");
			break;

		case EVENT_END | EVENT_KEYDOWN:
			printf("Space is now up\n");
			break;
		}

		if (app_keys[KEY_ESCAPE] == (EVENT_BEGIN | EVENT_KEYDOWN))
			app_quit();

		for (i = 0, n = app_motioncount; i < n; ++i) {
			m = &app_motions[i];

			switch (m->event) {
			case EVENT_BEGIN | EVENT_HOVER:
				printf("begin hover %.02f %.02f\n", m->x, m->y);
				break;

			case EVENT_END | EVENT_HOVER:
				printf("end hover %.02f %.02f\n", m->x, m->y);
				break;

			case EVENT_BEGIN | EVENT_TOUCH:
				printf("begin touch %d\n", m->id);
				break;

			case EVENT_END | EVENT_TOUCH:
				printf("end touch %d\n", m->id);
				break;
			}
		}
	}

	app_end();
}

