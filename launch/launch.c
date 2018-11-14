/* swc: launch/launch.c
 *
 * Copyright (c) 2013, 2014, 2016 Michael Forney
 *
 * Based in part upon weston-launch.c from weston which is:
 *
 *     Copyright © 2012 Benjamin Franzke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "protocol.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/major.h>
#include <linux/vt.h>
#include <xf86drm.h>

#ifndef DRM_MAJOR
#define DRM_MAJOR 226
#endif

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array)[0])

pid_t child_pid;

static struct {
	int socket;
	int input_fds[128];
	unsigned num_input_fds;
	int drm_fds[16];
	unsigned num_drm_fds;
	int tty_fd;
	bool active;
} launcher;

static struct {
	bool altered;
	int vt;
	long kb_mode;
	long console_mode;
} original_vt_state;

static bool nflag;
static int sigfd[2];

static noreturn void __attribute__((format(printf, 1, 2)))
die(const char *format, ...);

static noreturn void usage(const char *name)
{
	fprintf(stderr, "usage: %s [-n] [-t tty] [--] server [args...]\n", name);
	exit(2);
}

static void
start_devices(void)
{
	unsigned index;

	for (index = 0; index < launcher.num_drm_fds; ++index) {
		if (drmSetMaster(launcher.drm_fds[index]) < 0)
			die("failed to set DRM master");
	}
}

static void
stop_devices(bool fatal)
{
	unsigned index;

	for (index = 0; index < launcher.num_drm_fds; ++index) {
		if (drmDropMaster(launcher.drm_fds[index]) < 0 && fatal)
			die("failed to drop DRM master");
	}

	for (index = 0; index < launcher.num_input_fds; ++index) {
		if (ioctl(launcher.input_fds[index], EVIOCREVOKE, 0) < 0 && errno != ENODEV && fatal)
			die("failed to revoke input device:");
		close(launcher.input_fds[index]);
	}

	launcher.num_input_fds = 0;
}

static void
cleanup(void)
{
	struct vt_mode mode = {.mode = VT_AUTO };

	if (!original_vt_state.altered)
		return;

	/* Cleanup VT */
	ioctl(launcher.tty_fd, VT_SETMODE, &mode);
	ioctl(launcher.tty_fd, KDSETMODE, original_vt_state.console_mode);
	ioctl(launcher.tty_fd, KDSKBMODE, original_vt_state.kb_mode);

	/* Stop devices before switching the VT to make sure we have released the DRM
	 * device before the next session tries to claim it. */
	stop_devices(false);
	ioctl(launcher.tty_fd, VT_ACTIVATE, original_vt_state.vt);

	if (child_pid)
		kill(child_pid, SIGTERM);
}

noreturn void __attribute__((format(printf, 1, 2)))
die(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);

	if (format[0] && format[strlen(format) - 1] == ':')
		fprintf(stderr, " %s", strerror(errno));
	fputc('\n', stderr);

	cleanup();
	exit(EXIT_FAILURE);
}

static void
activate(void)
{
	struct swc_launch_event event = {.type = SWC_LAUNCH_EVENT_ACTIVATE };

	start_devices();
	send(launcher.socket, &event, sizeof(event), 0);
	launcher.active = true;
}

static void
deactivate(void)
{
	struct swc_launch_event event = {.type = SWC_LAUNCH_EVENT_DEACTIVATE };

	send(launcher.socket, &event, sizeof(event), 0);
	stop_devices(true);
	launcher.active = false;
}

static void
handle_signal(int sig)
{
	write(sigfd[1], (char[]){sig}, 1);
}

static void
handle_socket_data(int socket)
{
	char buffer[BUFSIZ];
	struct swc_launch_request *request = (void *)&buffer;
	struct swc_launch_event response;
	int fd = -1;
	struct stat st;
	ssize_t size;

	size = receive_fd(socket, &fd, buffer, sizeof(buffer));

	if (size == -1 || size == 0)
		return;

	response.type = SWC_LAUNCH_EVENT_RESPONSE;
	response.serial = request->serial;

	switch (request->type) {
	case SWC_LAUNCH_REQUEST_OPEN_DEVICE:
		if (buffer[size - 1] != '\0') {
			fprintf(stderr, "path is not NULL terminated\n");
			goto fail;
		}

		if (stat(request->path, &st) == -1) {
			fprintf(stderr, "stat %s: %s\n", request->path, strerror(errno));
			goto fail;
		}

		switch (major(st.st_rdev)) {
		case INPUT_MAJOR:
			if (!launcher.active)
				goto fail;

			if (launcher.num_input_fds == ARRAY_LENGTH(launcher.input_fds)) {
				fprintf(stderr, "too many input devices opened\n");
				goto fail;
			}
			break;
		case DRM_MAJOR:
			if (launcher.num_drm_fds == ARRAY_LENGTH(launcher.drm_fds)) {
				fprintf(stderr, "too many DRM devices opened\n");
				goto fail;
			}
			break;
		default:
			fprintf(stderr, "device is not an input device\n");
			goto fail;
		}

		fd = open(request->path, request->flags);

		if (fd == -1) {
			fprintf(stderr, "open %s: %s\n", request->path, strerror(errno));
			goto fail;
		}

		switch (major(st.st_rdev)) {
		case INPUT_MAJOR:
			launcher.input_fds[launcher.num_input_fds++] = fd;
			break;
		case DRM_MAJOR:
			launcher.drm_fds[launcher.num_drm_fds++] = fd;
			break;
		}

		break;
	case SWC_LAUNCH_REQUEST_ACTIVATE_VT:
		if (!launcher.active)
			goto fail;

		if (ioctl(launcher.tty_fd, VT_ACTIVATE, request->vt) == -1)
			fprintf(stderr, "failed to activate VT %d: %s\n", request->vt, strerror(errno));
		break;
	default:
		fprintf(stderr, "unknown request %u\n", request->type);
		goto fail;
	}

	response.success = true;
	goto done;

fail:
	response.success = false;
	fd = -1;
done:
	send_fd(socket, fd, &response, sizeof(response));
}

static void
find_vt(char *vt, size_t size)
{
	char *vtnr;

	/* If we are running from an existing X or wayland session, always open a new
	 * VT instead of using the current one. */
	if (getenv("DISPLAY") || getenv("WAYLAND_DISPLAY") || !(vtnr = getenv("XDG_VTNR"))) {
		int tty0_fd, vt_num;

		tty0_fd = open("/dev/tty0", O_RDWR);
		if (tty0_fd == -1)
			die("open /dev/tty0:");
		if (ioctl(tty0_fd, VT_OPENQRY, &vt_num) != 0)
			die("VT open query failed:");
		close(tty0_fd);
		if (snprintf(vt, size, "/dev/tty%d", vt_num) >= size)
			die("VT number is too large");
	} else {
		if (snprintf(vt, size, "/dev/tty%s", vtnr) >= size)
			die("XDG_VTNR is too long");
	}
}

static int
open_tty(const char *tty_name)
{
	char *stdin_tty;
	int fd;

	/* Check if we are already running on the desired VT */
	if ((stdin_tty = ttyname(STDIN_FILENO)) && strcmp(tty_name, stdin_tty) == 0)
		return STDIN_FILENO;

	fd = open(tty_name, O_RDWR | O_NOCTTY);
	if (fd < 0)
		die("open %s:", tty_name);

	return fd;
}

static void
setup_tty(int fd)
{
	struct stat st;
	int vt;
	struct vt_stat state;
	struct vt_mode mode = {
		.mode = VT_PROCESS,
		.relsig = SIGUSR1,
		.acqsig = SIGUSR2
	};

	if (fstat(fd, &st) == -1)
		die("failed to stat TTY fd:");

	vt = minor(st.st_rdev);

	if (major(st.st_rdev) != TTY_MAJOR || vt == 0)
		die("not a valid VT");

	if (ioctl(fd, VT_GETSTATE, &state) == -1)
		die("failed to get the current VT state:");

	original_vt_state.vt = state.v_active;

	if (ioctl(fd, KDGKBMODE, &original_vt_state.kb_mode))
		die("failed to get keyboard mode:");

	if (ioctl(fd, KDGETMODE, &original_vt_state.console_mode))
		die("failed to get console mode:");

	if (ioctl(fd, KDSKBMODE, K_OFF) == -1)
		die("failed to set keyboard mode to K_OFF:");

	if (ioctl(fd, KDSETMODE, KD_GRAPHICS) == -1) {
		perror("failed to set console mode to KD_GRAPHICS");
		goto error0;
	}

	if (ioctl(fd, VT_SETMODE, &mode) == -1) {
		perror("failed to set VT mode");
		goto error1;
	}

	if (vt == original_vt_state.vt)
		activate();
	else if (!nflag) {
		if (ioctl(fd, VT_ACTIVATE, vt) == -1) {
			perror("failed to activate VT");
			goto error2;
		}

		if (ioctl(fd, VT_WAITACTIVE, vt) == -1) {
			perror("failed to wait for VT to become active");
			goto error2;
		}
	}

	original_vt_state.altered = true;

	return;

error2:
	mode = (struct vt_mode){.mode = VT_AUTO };
	ioctl(fd, VT_SETMODE, &mode);
error1:
	ioctl(fd, KDSETMODE, original_vt_state.console_mode);
error0:
	ioctl(fd, KDSKBMODE, original_vt_state.kb_mode);
	exit(EXIT_FAILURE);
}

static void
run(int fd) {
	struct pollfd fds[] = {
		{.fd = fd, .events = POLLIN},
		{.fd = sigfd[0], .events = POLLIN},
	};
	int status;
	char sig;

	for (;;) {
		if (poll(fds, ARRAY_LENGTH(fds), -1) < 0) {
			if (errno == EINTR)
				continue;
			die("poll:");
		}
		if (fds[0].revents)
			handle_socket_data(fd);
		if (fds[1].revents) {
			if (read(sigfd[0], &sig, 1) <= 0)
				continue;
			switch (sig) {
			case SIGCHLD:
				if (!child_pid)
					break;
				wait(&status);
				cleanup();
				exit(WEXITSTATUS(status));
			case SIGUSR1:
				deactivate();
				ioctl(launcher.tty_fd, VT_RELDISP, 1);
				break;
			case SIGUSR2:
				ioctl(launcher.tty_fd, VT_RELDISP, VT_ACKACQ);
				activate();
				break;
			}
		}
	}
}

int
main(int argc, char *argv[])
{
	int option;
	int sock[2];
	char *vt = NULL, buf[64];
	struct sigaction action = {
		.sa_handler = handle_signal,
		.sa_flags = SA_RESTART,
	};
	sigset_t set;
	posix_spawnattr_t attr;

	while ((option = getopt(argc, argv, "nt:")) != -1) {
		switch (option) {
		case 'n':
			nflag = true;
			break;
		case 't':
			vt = optarg;
			break;
		default:
			usage(argv[0]);
		}
	}

	if (argc - optind < 1)
		usage(argv[0]);

	if (socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, sock) == -1)
		die("socketpair:");

	launcher.socket = sock[0];

	if (fcntl(sock[0], F_SETFD, FD_CLOEXEC) == -1)
		die("failed set CLOEXEC on socket:");

	if (pipe2(sigfd, O_CLOEXEC) == -1)
		die("pipe:");
	if (sigaction(SIGCHLD, &action, NULL) == -1)
		die("sigaction SIGCHLD:");
	if (sigaction(SIGUSR1, &action, NULL) == -1)
		die("sigaction SIGUSR1:");
	if (sigaction(SIGUSR2, &action, NULL) == -1)
		die("sigaction SIGUSR2:");

	sigfillset(&set);
	sigdelset(&set, SIGCHLD);
	sigdelset(&set, SIGUSR1);
	sigdelset(&set, SIGUSR2);
	sigprocmask(SIG_SETMASK, &set, NULL);

	if (!vt) {
		find_vt(buf, sizeof(buf));
		vt = buf;
	}

	fprintf(stderr, "running on %s\n", vt);
	launcher.tty_fd = open_tty(vt);
	setup_tty(launcher.tty_fd);

	sprintf(buf, "%d", sock[1]);
	setenv(SWC_LAUNCH_SOCKET_ENV, buf, 1);

	if (posix_spawnattr_setflags(&attr, POSIX_SPAWN_RESETIDS|POSIX_SPAWN_SETSIGMASK) != 0)
		die("failed to set spawnattr flags:");
	sigemptyset(&set);
	if (posix_spawnattr_setsigmask(&attr, &set) != 0)
		die("failed to set spawnattr sigmask:");
	if (posix_spawnp(&child_pid, argv[optind], NULL, &attr, argv + optind, environ) != 0)
		die("failed to spawn server:");

	run(sock[0]);

	return EXIT_SUCCESS;
}
