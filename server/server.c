#include <driftsync.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h> // for socklen_t under mingw


#ifdef _WIN32
#  ifdef _WIN64
#    define PRI_SIZET PRIu64
#  else
#    define PRI_SIZET PRIu32
#  endif
#else
#  define PRI_SIZET "zu"
#endif


static inline uint64_t
localTime()
{
	struct timespec time;
	if (clock_gettime(CLOCK_MONOTONIC, &time) != 0)
		return 0;

	return ((uint64_t)time.tv_sec * 1000 * 1000 * 1000 + time.tv_nsec) / 1000;
}

void
exit_handler()
{
	WSACleanup();
	exit(0);
}


bool
init_wsa()
{
	printf("init_wsa\n");
	
	WORD wVersionRequested;
    WSADATA wsaData;
    int err;

	/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
    wVersionRequested = MAKEWORD(2, 2);

	printf("calling WSAStartup\n");

    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        /* Tell the user that we could not find a usable */
        /* Winsock DLL.                                  */
        printf("WSAStartup failed with error: %d\n", err);
        return false;
    }
	
	printf("WSAStartup ok\n");

	/* Confirm that the WinSock DLL supports 2.2.*/
	/* Note that if the DLL supports versions greater    */
	/* than 2.2 in addition to 2.2, it will still return */
	/* 2.2 in wVersion since that is the version we      */
	/* requested.                                        */

    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        printf("Could not find a usable version of Winsock.dll\n");
        WSACleanup();
        return false;
    }
    else
	{
        printf("The Winsock 2.2 dll was found okay\n");
	}
	return true;
}

int
main(int argc, char *argv[])
{
	fprintf(stderr, "-2\n");
	
	int verbose = 0;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
			verbose = 1;
		else {
			printf("usage: %s [-v|--verbose]\n", argv[0]);
			exit(1);
		}
	}
	
	fprintf(stderr, "-1\n");

	signal(SIGINT, exit_handler);
	signal(SIGTERM, exit_handler);
	
	fprintf(stderr, "0\n");
	
	if (!init_wsa())
		exit(1);
	
	fprintf(stderr, "1\n");

	uint64_t sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET) {
		//printf("failed to create socket: %s\n", strerror(errno));
		
		int error = WSAGetLastError();
		fprintf(stderr, "failed to create socket: %d\n", error);
		
		return 1;
	}
	
	fprintf(stderr, "2\n");

	char reuse = 1;
	int result = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	if (result != 0) {
		fprintf(stderr, "failed to set address reuse socket option: %s\n",
			strerror(errno));
		// non-fatal
	}
	
	fprintf(stderr, "3\n");

	struct sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(DRIFTSYNC_PORT);
	result = bind(sock, (struct sockaddr *)&address, sizeof(address));
	if (result != 0) {
		fprintf(stderr, "failed to bind to local port: %s\n", strerror(errno));
		return 1;
	}

	fprintf(stderr, "Starting driftsync server on port %d\n", DRIFTSYNC_PORT);

	struct sockaddr_storage remote;
	struct driftsync_packet packet;
	uint64_t start_time = localTime();
	while (1) {
		socklen_t remoteLength = sizeof(remote);
		result = recvfrom(sock, (char *)&packet, sizeof(packet), 0,
			(struct sockaddr *)&remote, &remoteLength);

		if (result < 0) {
			fprintf(stderr, "failed to receive: %s\n", strerror(errno));
			continue;
		}

		if (result < (int)sizeof(packet)) {
			fprintf(stderr, "received incomplete packet of %d\n", result);
			continue;
		}

		if (packet.magic != DRIFTSYNC_MAGIC) {
			fprintf(stderr, "protocol mismatch\n");
			continue;
		}

		if ((packet.flags & DRIFTSYNC_FLAG_REPLY) != 0) {
			fprintf(stderr, "received reply packet\n");
			continue;
		}

		packet.flags |= DRIFTSYNC_FLAG_REPLY;
		packet.remote = localTime() - start_time;
		result = sendto(sock, (const char *)&packet, sizeof(packet), 0,
			(struct sockaddr *)&remote, remoteLength);

		if (verbose) {
			fprintf(stderr, "processed request packet, remote time %" PRI_SIZET
				", local time %" PRI_SIZET "\n", packet.local, packet.remote);
		}

		if (result < 0) {
			fprintf(stderr, "failed to send: %s\n", strerror(errno));
			continue;
		}

		if (result != (int)sizeof(packet)) {
			fprintf(stderr, "sent incomplete packet of %d\n", result);
			continue;
		}
	}

	return 0;
}
