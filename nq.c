/*
##% gcc -std=c89 -Wall -g -o $STEM $FILE
*/

#define _POSIX_C_SOURCE 200809L

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void
swrite(int fd, char *str)
{
	size_t l = strlen(str);

	if (write(fd, str, l) != l) {
		perror("write");
		exit(222);
	}
}

void
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
	int dirfd, lockfd;
	int pipefd[2];
	char lockfile[32];
	pid_t child;
	struct timeval started;
	struct dirent *ent;

	/* timestamp is milliseconds since epoch.  */
	gettimeofday(&started, NULL);
	int64_t ms = started.tv_sec*1000 + started.tv_usec/1000;

	if (argc <= 1) {
		swrite(2, "usage: nq CMD...\n");
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

	pipe(pipefd);

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
		sprintf(lockfile, ",%011lx.%d", ms, child);
		dprintf(1, "%s\n", lockfile);
		close(0);
		close(1);

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
	sprintf(lockfile, ".,%011lx.%d", ms, getpid());
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

	DIR *dir = fdopendir(dirfd);
	if (!dir) {
		perror("fdopendir");
		exit(111);
	}

again:
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == ',' &&
		    strcmp(ent->d_name, lockfile+1) < 0) {
			int f = openat(dirfd, ent->d_name, O_RDWR);
    
			if (flock(f, LOCK_EX | LOCK_NB) == -1 &&
			    errno == EWOULDBLOCK) {
				flock(f, LOCK_EX);   /* sit it out.  */

				close(f);
				rewinddir(dir);
				goto again;
			}
    
			fchmod(f, 0600);
			close(f);
		}
	}

	closedir(dir);		/* closes dirfd too.  */

	/* ready to run.  */

	swrite(lockfd, "\n\n");
	fchmod(lockfd, 0700);
  
	dup2(lockfd, 2);
	dup2(lockfd, 1);
	close(lockfd);

	execvp(argv[1], argv+1);

	perror("execvp");
	return 222;
}
