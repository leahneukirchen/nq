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

static void
quit(int sig)
{
	exit(0);
}

/*
  char *
  timestamp() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  static char buf[10];

  int64_t ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

  buf[9] = 0;
  int i;
  for (i = 8; i >= 0; i--) {
  buf[i] = 'a' + (ms % 26);
  ms /= 26;
  }
  
  return buf;
  }
*/

int
main(int argc, char *argv[])
{
	pid_t child;
	int lockfd;
	char lockfile[128];
	int dirfd;

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

	child = fork();
	if (child == -1) {
		perror("fork");
		exit(111);
	}

	if (child > 0) {
		// background
		signal(SIGINT, quit);
		pause();
		exit(0);
	}

	child = fork();
	if (child == -1) {
		perror("fork");
		exit(111);
	}
	if (child > 0) {
		int status;
		sprintf(lockfile, ",%lx.%d", ms, child);

		puts(lockfile);

		// signal parent to exit
		kill(getppid(), SIGINT);

		wait(&status);
		int fd;
		fd = openat(dirfd, lockfile, O_RDWR | O_APPEND);
		fchmod(fd, 0600);
		if (WIFEXITED(status))
			dprintf(fd, "\n[exited with status %d.]\n", WEXITSTATUS(status));
		else
			dprintf(fd, "\n[killed by signal %d.]\n", WTERMSIG(status));

		exit(0);
	}

	sprintf(lockfile, ".,%lx.%d", ms, getpid());
	lockfd = openat(dirfd, lockfile, O_CREAT | O_EXCL | O_RDWR | O_APPEND, 0600);
  
	if (lockfd < 0) {
		perror("open");
		exit(222);
	}

	flock(lockfd, LOCK_EX);
  
	// drop leading .
	renameat(dirfd, lockfile, dirfd, lockfile+1);

	write(lockfd, "exec", 4);
	int i;
	for (i = 0; i < argc; i++) {
		int j, l = strlen(argv[i]);
		write(lockfd, " '", 2);
		for (j = 0; j < l; j++) {
			if (argv[i][j] == '\'')
				write(lockfd, "'\\''", 4);
			else
				write(lockfd, argv[i]+j, 1);
		}
		write(lockfd, "'", 1);
	}

	DIR *dir = fdopendir(dirfd);
	if (!dir) {
		perror("fdopendir");
		exit(111);
	}

	struct dirent *ent;

again:
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == ',' && strcmp(ent->d_name, lockfile+1) < 0) {
			int f = openat(dirfd, ent->d_name, O_RDWR);
    
			if (flock(f, LOCK_EX | LOCK_NB) == -1 && errno == EWOULDBLOCK) {
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
