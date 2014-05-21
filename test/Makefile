
export CFLAGS += -Wall -Wextra
export LDFLAGS += -Wl,-arch -Wl,x86_64 -framework IOKit -framework QuartzCore -framework Cocoa -framework OpenGL

test: test.o ../libaw-app.a libglfw3.a
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -I.. -c $< -o $@

../libaw-app.a:
	$(MAKE) -C..

libglfw3.a: glfw/build/src/libglfw3.a
	ln -s $^ $@

glfw/build/src/libglfw3.a: glfw/build/Makefile
	cd glfw/build && \
	$(MAKE) && \
	touch -c $@

glfw/build/Makefile: glfw/CMakeLists.txt
	mkdir glfw/build && \
	cd glfw/build && \
	cmake .. && \
	touch -c $@

glfw/CMakeLists.txt:
	git clone git@github.com:glfw/glfw.git && \
	touch -c $@

.PHONY: clean
clean:
	rm -f test test.o libglfw*.a

