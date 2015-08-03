## nq: queue utilities

These small utilities allow creating very lightweight job queue
systems which require no setup, maintenance, supervision, or any
long-running processes.

`nq` should run on any POSIX.1-2008 compliant system which also
provides a working flock(2).  Tested on Linux 4.1, OpenBSD 5.7 and
FreeBSD 10.1.

The intended purpose is ad-hoc queuing of command lines (e.g. for
building several targets of a Makefile, downloading multiple files one
at a time, running benchmarks in several configurations, or simply as
a glorified `nohup`), but as any good Unix tool, it can be abused for
whatever you like.

Job order is enforced by a timestamp `nq` gets immediately when
started.  Synchronization happens on file-system level.  Timer
resolution is milliseconds.  No sub-second file system time stamps are
required.  Polling is not used.  Exclusive execution is maintained
strictly.

Enforcing job order works like this:
- every job has a flock(2)ed output file ala `,TIMESTAMP.PID`
- every job starts only after all earlier flock(2)ed files are unlocked
- the lock is released by the kernel after the job terminates

You enqueue (get it?) new jobs using `nq CMDLINE...`.  The job id is
output and `nq` detaches immediately, running the job in the background.
STDOUT and STDERR are redirected into the log file.

`nq` tries hard (but does not guarantee) to ensure the log file of the
currently running job has +x bit set.  Thus you can use `ls -F` to get
a quick overview of the state of your queue.

The "file extension" of the log file is the actually PID, so you can
kill jobs easily.  Before the job is started, it is the PID of `nq`,
so you can cancel a queued job by killing it as well.

Due to the initial `exec` line in the log files, you can resubmit a
job by executing it as a shell command file, i.e. running `sh $jobid`.

You can wait for jobs to finish using `nq -w`, possibly listing job
ids you want to wait for; the default is all of them.  Likewise, you
can test if there are jobs which need to be waited upon using `-t`.

By default, job ids are per-directory, but you can set `$NQDIR` to put
them elsewhere.  Creating `nq` wrappers setting `$NQDIR` to provide
different queues for different purposes is encouraged.

All these operations take worst-case quadratic time in the amount of
lock files produced, so you should clean them regularly.

## nq helpers

Two helper scripts are provided:

`tq` wraps `nq` and displays the output in a new tmux window (it needs
`tmux` and GNU `tail`).

`fq` outputs the log of the currently running jobs, exiting when the
jobs are done.  If no job is running, the output of the last job is
shown.

(A pure shell implementation of `nq` is provided as `nq.sh`.  It needs
`flock` from util-linux, and only has a timer resolution of 1s.
Lock files from `nq` and `nq.sh` should not be mixed.)

## Copyright

nq is in the public domain.

To the extent possible under law,
Christian Neukirchen <chneukirchen@gmail.com>
has waived all copyright and related or
neighboring rights to this work.

http://creativecommons.org/publicdomain/zero/1.0/
