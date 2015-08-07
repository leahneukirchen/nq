/*
 * nq CMD... - run CMD... in background and in order, saving output
 * -w ...  wait for all jobs/listed jobs queued so far to finish
 * -t ...  exit 0 if no (listed) job needs waiting
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
 * To the extent possible under law,
 * Christian Neukirchen <chneukirchen@gmail.com>
 * has waived all copyright and related or neighboring rights to this work.
 * http://creativecommons.org/publicdomain/zero/1.0/
*/

/* for FreeBSD.  */
#define _WITH_DPRINTF

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

int
main(int argc, char *argv[])
{
	int64_t ms;
	int dirfd = 0, lockfd = 0, opt = 0, tflag = 0, wflag = 0;
	int pipefd[2];
	char lockfile[64];
	pid_t child;
	struct timeval started;
	struct dirent *ent;
	DIR *dir;

	/* timestamp is milliseconds since epoch.  */
	gettimeofday(&started, NULL);
	ms = started.tv_sec*1000 + started.tv_usec/1000;

	while ((opt = getopt(argc, argv, "+htw")) != -1) {
		switch (opt) {
		case 'w':
			wflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'h':
		default:
			goto usage;
		}
	}

	if (argc <= 1) {
usage:
		swrite(2, "usage: nq [-w ... | -t ... | CMD...]\n");
		exit(1);
	}

	char *path = getenv("NQDIR");
	if (!path)
		path = ".";

	dirfd = open(path, O_RDONLY);
	if (dirfd < 0) {
		perror("dir open");
		exit(111);
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

		fchmod(lockfd, 0600);
		if (WIFEXITED(status))
			dprintf(lockfd, "\n[exited with status %d.]\n",
			    WEXITSTATUS(status));
		else
			dprintf(lockfd, "\n[killed by signal %d.]\n",
			    WTERMSIG(status));

		exit(0);
	}

	close(pipefd[1]);

	/* create and lock lockfile.  since this cannot be done in one step,
	   use a different filename first.  */
	snprintf(lockfile, sizeof lockfile,
	    ".,%011" PRIx64 ".%d", ms, getpid());
	lockfd = openat(dirfd, lockfile,
	    O_CREAT | O_EXCL | O_RDWR | O_APPEND, 0600);
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

	write_execline(lockfd, argc, argv);

	dup2(lockfd, 2);
	dup2(lockfd, 1);

wait:
	if ((tflag || wflag) && argc - optind > 0) {
		/* wait for files passed as command line arguments.  */

		int i;
		for (i = optind; i < argc; i++) {
			int fd;

			if (strchr(argv[i], '/'))
				fd = open(argv[i], O_RDWR);
			else
				fd = openat(dirfd, argv[i], O_RDWR);
			if (fd < 0)
				continue;

			if (flock(fd, LOCK_EX | LOCK_NB) == -1 &&
			    errno == EWOULDBLOCK) {
				if (tflag)
					exit(1);
				flock(fd, LOCK_EX);   /* sit it out.  */
			}

			fchmod(fd, 0600);
			close(fd);
		}
	} else {
		dir = fdopendir(dirfd);
		if (!dir) {
			perror("fdopendir");
			exit(111);
		}

again:
		while ((ent = readdir(dir))) {
			/* wait for all ,* files.  */

			if (ent->d_name[0] == ',' &&
			    strcmp(ent->d_name, lockfile+1) < 0) {
				int fd;

				fd = openat(dirfd, ent->d_name, O_RDWR);
				if (fd < 0)
					continue;

				if (flock(fd, LOCK_EX | LOCK_NB) == -1 &&
				    errno == EWOULDBLOCK) {
					if (tflag)
						exit(1);
					flock(fd, LOCK_EX);   /* sit it out.  */

					close(fd);
					rewinddir(dir);
					goto again;
				}

				fchmod(fd, 0600);
				close(fd);
			}
		}

		closedir(dir);		/* closes dirfd too.  */
	}

	if (tflag || wflag)
		exit(0);

	/* ready to run.  */

	swrite(lockfd, "\n\n");
	fchmod(lockfd, 0700);

	close(lockfd);

	setenv("NQJOBID", lockfile+1, 1);
	setsid();
	execvp(argv[1], argv+1);

	perror("execvp");
	return 222;
}
