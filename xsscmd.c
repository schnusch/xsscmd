/*
 * --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * schnusch <schnusch@users.noreply.github.com> wrote these files. As long as
 * you retain this notice you can do whatever you want with this stuff. If we
 * meet some day, and you think this stuff is worth it, you can buy me a beer
 * in return.
 * --------------------------------------------------------------------------
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/extensions/scrnsaver.h>

struct parsed_args {
	const char *argv0;
	time_t timeout;
	unsigned int verbose;
	const char *idle_cmd;
	const char *wake_cmd;
};

static int parse_args(struct parsed_args *args, int argc, char **argv) {
	*args = (struct parsed_args){
		.argv0 = argv[0],
		.timeout = 300,
		.verbose = 0,
		.idle_cmd = NULL,
		.wake_cmd = NULL,
	};

	optind = 1;
	for(int opt; (opt = getopt(argc, argv, ":t:v")) != -1;) {
		switch(opt) {
		case 't':
			if(errno == ERANGE) {
				errno = 0;
			}
			char *end;
			unsigned long t = strtoul(optarg, &end, 0);
			if(__builtin_add_overflow(t, 0, &args->timeout)) {
				// simulate ERANGE if the integer does not fit inside time_t
				t = ULONG_MAX;
				errno = ERANGE;
			} else if(*end == 's') {
				// allow a trailing 's' after integer
				++end;
			}
			if((t == ULONG_MAX && errno == ERANGE) || *end) {
				// ERANGE or unexpected trailing data after integer
				fprintf(stderr, "%s: unknown argument <timeout>: %s: %s\n",
					argv[0], optarg, strerror(*end ? EINVAL : errno));
				goto usage;
			}
			break;
		case 'v':
			++args->verbose;
			break;
		case '?':
			fprintf(stderr, "%s: unknown argument: -%c\n", argv[0], optopt);
			goto usage;
		case ':':
			fprintf(stderr, "%s: missing argument for -%c\n", argv[0], optopt);
			goto usage;
		}
	}
	if(optind == argc) {
		fprintf(stderr, "%s: missing argument: <idle_cmd>\n", argv[0]);
		goto usage;
	}
	args->idle_cmd = argv[optind++];
	if(optind < argc) {
		args->wake_cmd = argv[optind++];
	}
	if(optind < argc) {
		fprintf(stderr, "%s: unexpected trailing argument: %s\n", argv[0], argv[optind]);
	usage:
		fprintf(stderr, "Usage: %s [-v] [-t <timeout>] <idle_cmd> [<wake_cmd>]\n", argv[0]);
		return -1;
	}

	return 0;
}

static int timespec_sub(struct timespec *a, const struct timespec *b) {
	int overflow = 0;
	if(a->tv_nsec < b->tv_nsec) {
		a->tv_nsec += 1000000000;
		overflow |= __builtin_sub_overflow(a->tv_sec, 1, &a->tv_sec);
	}
	overflow |= __builtin_sub_overflow(a->tv_nsec, b->tv_nsec, &a->tv_nsec);
	overflow |= __builtin_sub_overflow(a->tv_sec, b->tv_sec, &a->tv_sec);
	return overflow;
}

static void timespec_sub_zero(struct timespec *a, const struct timespec *b) {
	if(timespec_sub(a, b) || a->tv_sec < 0) {
		a->tv_sec = 0;
		a->tv_nsec = 0;
	}
}

static int runcmd(const char *cmd, const struct parsed_args *args) {
	if(args->verbose >= 1) {
		fprintf(stderr, "%s: running: %s\n", args->argv0, cmd);
	}
	int rc = system(cmd);
	if(rc < 0) {
		fprintf(stderr, "%s: cannot run: %s: %s\n", args->argv0, cmd, strerror(errno));
		return -1;
	} else if(rc != 0 || args->verbose >= 1) {
		fprintf(stderr, "%s: $? = %d\n", args->argv0,
			WIFEXITED(rc) ? WEXITSTATUS(rc) :
			WIFSIGNALED(rc) ? WTERMSIG(rc) + 128 :
			-1);
	}
	return 0;
}

int main(int argc, char **argv) {
	struct parsed_args args;
	if(parse_args(&args, argc, argv) < 0) {
		return 2;
	}

	Display *dpy = XOpenDisplay(NULL);
	if(!dpy) {
		fprintf(stderr, "%s: cannot open display\n", args.argv0);
		return 1;
	}

	XScreenSaverInfo *info = XScreenSaverAllocInfo();
	if(!info) {
		fprintf(stderr, "%s: cannot allocate XScreenSaverInfo\n", args.argv0);
		XCloseDisplay(dpy);
		return 1;
	}

	// On the first iteration we do not know if it is currently idle or awake
	// so the commands are always run.
	enum {
		STATE_UNKNOWN,
		STATE_IDLE,
		STATE_ACTIVE,
	} state = STATE_UNKNOWN;
	while(1) {
		if(!XScreenSaverQueryInfo(dpy, DefaultRootWindow(dpy), info)) {
			fprintf(stderr, "%s: cannot query XScreenSaverInfo\n", args.argv0);
			goto error;
		}

		int gettime_error = 0;
		struct timespec before;
		if(clock_gettime(CLOCK_REALTIME, &before) < 0) {
			fprintf(stderr, "%s: clock_gettime: %s\n", args.argv0, strerror(errno));
			gettime_error = 1;
		}

		struct timespec idle = {
			.tv_sec = info->idle / 1000,
			.tv_nsec = (info->idle % 1000) * 1000000,
		};

		struct timespec wait = {0};
		if(idle.tv_sec >= args.timeout) {
			if(state != STATE_IDLE) {
				if(args.verbose >= 1) {
					fprintf(stderr, "%s: idle for %lu.%03us\n",
						args.argv0, idle.tv_sec, (unsigned int)idle.tv_nsec / 1000000);
				}
				if(runcmd(args.idle_cmd, &args) < 0) {
					goto error;
				}
				state = STATE_IDLE;
			} else {
				// Do not bother with subtracting passed time, since we did
				// not run `idle_cmd` we should have been fairly quick.
				gettime_error = 1;
			}

			// check once per second for activity
			wait.tv_sec = 1;
		} else {
			if(state != STATE_ACTIVE) {
				if(args.verbose >= 1) {
					fprintf(stderr, "%s: waking up\n", args.argv0);
				}
				if(args.wake_cmd && runcmd(args.wake_cmd, &args) < 0) {
					goto error;
				}
				state = STATE_ACTIVE;
			} else {
				// Do not bother with subtracting passed time, since we did
				// not run `wake_cmd` we should have been fairly quick.
				gettime_error = 1;
			}

			// wait for (timeout - info->idle)
			wait.tv_sec = args.timeout;
			timespec_sub_zero(&wait, &idle);
			if(args.verbose == 2) { // would be logged twice if args.verbose >= 3
				fprintf(stderr, "%s: waiting %lu.%09us...\n",
					args.argv0, (unsigned long)wait.tv_sec, (unsigned int)wait.tv_nsec);
			}
		}

		if(!gettime_error) {
			struct timespec after;
			if(clock_gettime(CLOCK_REALTIME, &after) < 0) {
				fprintf(stderr, "%s: clock_gettime: %s\n", args.argv0, strerror(errno));
				gettime_error = 1;
			}

			if(!gettime_error) {
				// calculate time passed since query
				timespec_sub_zero(&after, &before);
				if(args.verbose >= 3) {
					fprintf(stderr, "%s: subtracting %lu.%09us from sleep interval\n",
						args.argv0, after.tv_sec, (unsigned int)after.tv_nsec);
				}
				// subtract from sleep interval
				timespec_sub_zero(&wait, &after);
			}
		}

		// wait
		if(args.verbose >= 3) {
			fprintf(stderr, "%s: waiting %lu.%09us...\n",
				args.argv0, (unsigned long)wait.tv_sec, (unsigned int)wait.tv_nsec);
		}
		while(nanosleep(&wait, &wait) < 0) {
			if(errno != EINTR) {
				fprintf(stderr, "%s: nanosleep: %s\n", args.argv0, strerror(errno));
				goto error;
			}
		}
	}

error:
	XFree(info);
	XCloseDisplay(dpy);
	return 1;
}
