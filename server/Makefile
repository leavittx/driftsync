driftsync_server:
	gcc -pedantic \
		-Wall -Wextra -Werror -Wno-variadic-macros \
		-D__USE_MINGW_ANSI_STDIO=1 \
		-I ../include \
		-O3 ${ARGS} \
		-o driftsync_server \
		server.c \
		-lws2_32
