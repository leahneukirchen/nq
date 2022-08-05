/*
 * nq CMD... - run CMD... in background and in order, saving output
 * -w ...  wait for all jobs/listed jobs queued so far to finish
 * -t ...  exit 0 if no (listed) job needs waiting
 * -q      quiet, do not output job id
 * -c      clean, don't keep output if job exited with status 0
 *
 * - requires POSIX.1-2008 and having flock(2)
 * - enforcing order works like this:
 *   - every job has a flock(2)ed output file ala ",TIMESTAMP.PID"
 *   - every job starts only after all earlier flock(2)ed files finished
 *   - the lock is released when job terminates
 *   - no sub-second file system time stamps are required, jobs are started
 *     with millisecond precision
 * - we try hard to make the currently running ,* file have +x bit
 * - you can re-queue jobs using "sh ,jobid"
 *
 * To the extent possible under law, Leah Neukirchen <leah@vuxu.org>
 * has waived all copyright and related or neighboring rights to this work.
 * http://creativecommons.org/publicdomain/zero/1.0/
*/

/* for FreeBSD.  */
#define _WITH_DPRINTF

#if defined(__sun) && defined(__SVR4) && !defined(HAVE_DPRINTF)
#define NEED_DPRINTF
#endif

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

#ifdef NEED_DPRINTF
#include <stdarg.h>
static int
dprintf(int fd, const char *fmt, ...)
{
	char buf[128];  // good enough for usage in nq
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (r >= 0 && r < sizeof buf)
		return write(fd, buf, r);
	return -1;
}
#endif

static void
swrite(int fd, char *str)
{
	size_t l = strlen(str);

	if (write(fd, str, l) != l) {
		perror("write");
		exit(222);
	}
}

static void
write_execline(int fd, int argc, char *argv[])
{
	int i;
	char *s;

	swrite(fd, "exec");

	for (i = 0; i < argc; i++) {
		if (!strpbrk(argv[i],
				"\001\002\003\004\005\006\007\010"
				"\011\012\013\014\015\016\017\020"
				"\021\022\023\024\025\026\027\030"
				"\031\032\033\034\035\036\037\040"
				"`^#*[]=|\\?${}()'\"<>&;\177")) {
			swrite(fd, " ");
			swrite(fd, argv[i]);
		} else {
			swrite(fd, " '");
			for (s = argv[i]; *s; s++) {
				if (*s == '\'')
					swrite(fd, "'\\''");
				else
					write(fd, s, 1);
			}
			swrite(fd, "'");
		}
	}
}

static void
setx(int fd, int executable)
{
	struct stat st;
	fstat(fd, &st);
	if (executable)
		st.st_mode |= 0100;
	else
		st.st_mode &= ~0100;
	fchmod(fd, st.st_mode);
}

int
main(int argc, char *argv[])
{
	int64_t ms;
	int dirfd = -1, donedirfd = -1, faildirfd = -1, lockfd = -1;
	int opt = 0, cflag = 0, qflag = 0, tflag = 0, wflag = 0;
	int pipefd[2];
	char lockfile[64], newestlocked[64];
	pid_t child;
	struct timeval started;
	struct dirent *ent;
	DIR *dir;

	/* timestamp is milliseconds since epoch.  */
	gettimeofday(&started, NULL);
	ms = (int64_t)started.tv_sec*1000 + started.tv_usec/1000;

	while ((opt = getopt(argc, argv, "+chqtw")) != -1) {
		switch (opt) {
		case 'c':
			cflag = 1;
			break;
		case 'w':
			wflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'h':
		default:
			goto usage;
		}
	}

	if (!tflag && !wflag && argc <= optind) {
usage:
		swrite(2, "usage: nq [-c] [-q] [-w ... | -t ... | CMD...]\n");
		exit(1);
	}

	char *path = getenv("NQDIR");
	if (!path)
		path = ".";

	if (mkdir(path, 0777) < 0) {
		if (errno != EEXIST) {
			perror("mkdir $NQDIR");
			exit(111);
		}
	}

	dirfd = open(path, O_RDONLY | O_DIRECTORY);
	if (dirfd < 0) {
		perror("dir open");
		exit(111);
	}

	char *donepath = getenv("NQDONEDIR");
	if (donepath) {
		if (mkdir(donepath, 0777) < 0) {
			if (errno != EEXIST) {
				perror("mkdir $NQDONEDIR");
				exit(111);
			}
		}

		donedirfd = open(donepath, O_RDONLY | O_DIRECTORY);
		if (donedirfd < 0) {
			perror("dir open");
			exit(111);
		}
	}

	char *failpath = getenv("NQFAILDIR");
	if (failpath) {
		if (mkdir(failpath, 0777) < 0) {
			if (errno != EEXIST) {
				perror("mkdir $NQFAILDIR");
				exit(111);
			}
		}

		faildirfd = open(failpath, O_RDONLY | O_DIRECTORY);
		if (faildirfd < 0) {
			perror("dir open");
			exit(111);
		}
	}

	if (tflag || wflag) {
		snprintf(lockfile, sizeof lockfile,
		    ".,%011" PRIx64 ".%d", ms, getpid());
		goto wait;
	}

	if (pipe(pipefd) < 0) {
		perror("pipe");
		exit(111);
	};

	/* first fork, parent exits to run in background.  */
	child = fork();
	if (child == -1) {
		perror("fork");
		exit(111);
	}
	else if (child > 0) {
		char c;

		/* wait until child has backgrounded.  */
		close(pipefd[1]);
		read(pipefd[0], &c, 1);

		exit(0);
	}

	close(pipefd[0]);

	/* second fork, child later execs the job, parent collects status.  */
	child = fork();
	if (child == -1) {
		perror("fork");
		exit(111);
	}
	else if (child > 0) {
		int status;

		/* output expected lockfile name.  */
		snprintf(lockfile, sizeof lockfile,
		    ",%011" PRIx64 ".%d", ms, child);
		if (!qflag)
			dprintf(1, "%s\n", lockfile);
		close(0);
		close(1);
		close(2);

		/* signal parent to exit.  */
		close(pipefd[1]);

		wait(&status);

		lockfd = openat(dirfd, lockfile, O_RDWR | O_APPEND);
		if (lockfd < 0) {
			perror("open");
			exit(222);
		}

		setx(lockfd, 0);
		if (WIFEXITED(status)) {
			dprintf(lockfd, "\n[exited with status %d.]\n",
			    WEXITSTATUS(status));
			if (WEXITSTATUS(status) == 0) {
				if (cflag)
					unlinkat(dirfd, lockfile, 0);
				else if (donepath)
					renameat(dirfd, lockfile,
					    donedirfd, lockfile);
			}
			if (WEXITSTATUS(status) != 0) {
				if (failpath)
					renameat(dirfd, lockfile, faildirfd, lockfile);
				/* errors above are ignored */
			}
		} else {
			dprintf(lockfd, "\n[killed by signal %d.]\n",
			    WTERMSIG(status));
		}

		exit(0);
	}

	close(pipefd[1]);

	/* create and lock lockfile.  since this cannot be done in one step,
	   use a different filename first.  */
	snprintf(lockfile, sizeof lockfile,
	    ".,%011" PRIx64 ".%d", ms, getpid());
	lockfd = openat(dirfd, lockfile,
	    O_CREAT | O_EXCL | O_RDWR | O_APPEND, 0666);
	if (lockfd < 0) {
		perror("open");
		exit(222);
	}
	if (flock(lockfd, LOCK_EX) < 0) {
		perror("flock");
		exit(222);
	}

	/* drop leading '.' */
	renameat(dirfd, lockfile, dirfd, lockfile+1);

	/* block until rename is committed */
	fsync(dirfd);

	write_execline(lockfd, argc, argv);

	if (dup2(lockfd, 2) < 0 ||
	    dup2(lockfd, 1) < 0) {
		perror("dup2");
		exit(222);
	}

wait:
	if ((tflag || wflag) && argc - optind > 0) {
		/* wait for files passed as command line arguments.  */

		int i;
		for (i = optind; i < argc; i++) {
			int fd;

			if (strchr(argv[i], '/'))
				fd = open(argv[i], O_RDONLY);
			else
				fd = openat(dirfd, argv[i], O_RDONLY);
			if (fd < 0)
				continue;

			if (flock(fd, LOCK_SH | LOCK_NB) == -1 &&
			    errno == EWOULDBLOCK) {
				if (tflag)
					exit(1);
				flock(fd, LOCK_SH);   /* sit it out.  */
			}

			setx(fd, 0);
			close(fd);
		}
	} else {
		dir = fdopendir(dirfd);
		if (!dir) {
			perror("fdopendir");
			exit(111);
		}

again:
		*newestlocked = 0;

		while ((ent = readdir(dir))) {
			/* wait for all older ,* files than ours.  */

			if (!(ent->d_name[0] == ',' &&
			    strcmp(ent->d_name, lockfile+1) < 0 &&
			    strlen(ent->d_name) < sizeof(newestlocked)))
				continue;

			int fd = openat(dirfd, ent->d_name, O_RDONLY);
			if (fd < 0)
				continue;

			if (flock(fd, LOCK_SH | LOCK_NB) == -1 &&
			    errno == EWOULDBLOCK) {
				if (tflag)
					exit(1);
				if (strcmp(ent->d_name, newestlocked) > 0)
					strcpy(newestlocked, ent->d_name);
			} else {
				setx(fd, 0);
			}

			close(fd);
		}

		if (*newestlocked) {
			int fd = openat(dirfd, newestlocked, O_RDONLY);
			if (fd >= 0) {
				flock(fd, LOCK_SH);   /* sit it out.  */
				close(fd);
			}
			rewinddir(dir);
			goto again;
		}

		closedir(dir);          /* closes dirfd too.  */
	}

	if (tflag || wflag)
		exit(0);

	/* ready to run.  */

	swrite(lockfd, "\n\n");
	setx(lockfd, 1);

	close(lockfd);

	setenv("NQJOBID", lockfile+1, 1);
	setsid();
	execvp(argv[optind], argv+optind);

	perror("execvp");
	return 222;
}
