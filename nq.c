/*
##% gcc -Wall -g -o $STEM $FILE
*/

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void
swrite(int fd, char *str)
{
	if (write(fd, str, strlen(str)) < 0) {
		perror("write");
		exit(222);
	}
}

int
main(int argc, char *argv[])
{
	pid_t child;
	int lockfd, dirfd;
	char lockfile[32];
	int pipefd[2];

	struct timeval tv;
	gettimeofday(&tv, NULL);
	int64_t ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	char *path = getenv("NQDIR");
	if (!path)
		path = ".";

	dirfd = open(path, O_RDONLY);
	if (dirfd < 0) {
		perror("dir open");
		exit(111);
	}

	pipe(pipefd);

	child = fork();
	if (child == -1) {
		perror("fork");
		exit(111);
	}
	else if (child > 0) {
		// background
		close(pipefd[1]);
		char c;
		read(pipefd[0], &c, 1);
		exit(0);
	}

	close(pipefd[0]);

	child = fork();
	if (child == -1) {
		perror("fork");
		exit(111);
	}
	else if (child > 0) {
		int status;
		sprintf(lockfile, ",%lx.%d", ms, child);

		dprintf(1, "%s\n", lockfile);

		// signal parent to exit
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

	sprintf(lockfile, ".,%lx.%d", ms, getpid());
	lockfd = openat(dirfd, lockfile, O_CREAT | O_EXCL | O_RDWR | O_APPEND, 0600);
  
	if (lockfd < 0) {
		perror("open");
		exit(222);
	}

	flock(lockfd, LOCK_EX);
  
	// drop leading '.'
	renameat(dirfd, lockfile, dirfd, lockfile+1);

	swrite(lockfd, "exec");
	int i;
	for (i = 0; i < argc; i++) {
		int j, l = strlen(argv[i]);
		swrite(lockfd, " '");
		for (j = 0; j < l; j++) {
			if (argv[i][j] == '\'')
				swrite(lockfd, "'\\''");
			else
				write(lockfd, argv[i]+j, 1);
		}
		swrite(lockfd, "'");
	}

	DIR *dir = fdopendir(dirfd);
	if (!dir) {
		perror("fdopendir");
		exit(111);
	}

	struct dirent *ent;

again:
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == ',' &&
		    strcmp(ent->d_name, lockfile+1) < 0) {
			int f = openat(dirfd, ent->d_name, O_RDWR);
    
			if (flock(f, LOCK_EX | LOCK_NB) == -1 &&
			    errno == EWOULDBLOCK) {
				flock(f, LOCK_EX);   // sit it out

				rewinddir(dir);
				goto again;
			}
    
			fchmod(f, 0600);
			close(f);
		}
	}

	closedir(dir);

	// ready to run

	write(lockfd, "\n", 1);

	fchmod(lockfd, 0700);
  
	dup2(lockfd, 2);
	dup2(lockfd, 1);
	close(lockfd);
	close(dirfd);

	execvp(argv[1], argv+1);

	return 111;
}
