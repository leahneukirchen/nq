/*
 * fq [FILES...] - follow output of nq jobs, quitting when they are done
 *
 * To the extent possible under law,
 * Christian Neukirchen <chneukirchen@gmail.com>
 * has waived all copyright and related or neighboring rights to this work.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __linux__
#define USE_INOTIFY
#endif

#ifdef USE_INOTIFY
#include <sys/inotify.h>
char ibuf[8192];
#endif

char buf[8192];

int main(int argc, char *argv[])
{
	int i, fd;
	off_t off, loff;
	ssize_t rd;

#ifdef USE_INOTIFY
	int ifd, wd;
#endif

	if (argc < 2) {
		/* little better than glob(3)... */
                /* TODO: replace and enable rescanning of the dir... */
		execl("/bin/sh", "sh", "-c", "fq ${NQDIR:+$NQDIR/},*", (char *) 0);
		exit(111);
	}

#ifdef USE_INOTIFY
	ifd = inotify_init();
	if (ifd < 0)
		exit(111);
#endif

	for (i = 1; i < argc; i++) {
		fd = open(argv[i], O_RDONLY);
		loff = 0;

		write(1, "==> ", 4);
		write(1, argv[i], strlen(argv[i]));
		write(1, "\n", 1);

#ifdef USE_INOTIFY
		wd = inotify_add_watch(ifd, argv[i], IN_MODIFY|IN_CLOSE_WRITE);
#endif

		while (1) {
			off = lseek(fd, 0, SEEK_END);

			if (off < loff)
				loff = off;               /* file truncated */

			if (off == loff) {
				if (flock(fd, LOCK_EX|LOCK_NB) == -1 &&
				    errno == EWOULDBLOCK) {
#ifdef USE_INOTIFY
					/* any inotify event is good */
					read(ifd, ibuf, sizeof ibuf);
#else
					/* poll for size change */
					while (off == lseek(fd, 0, SEEK_END))
						usleep(250000);
#endif
					continue;
				} else {
					flock(fd, LOCK_UN);
					break;
				}
			}

			if (off - loff > sizeof buf)
				off = loff + sizeof buf;

			rd = pread(fd, &buf, off - loff, loff);
			write(1, buf, rd);

			loff += rd;
		}

#ifdef USE_INOTIFY
		inotify_rm_watch(ifd, wd);
#endif
		close(fd);
	}

#ifdef USE_INOTIFY
	close(ifd);
#endif
	return 0;
}
