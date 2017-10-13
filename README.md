## nq: queue utilities

These small utilities allow creating very lightweight job queue
systems which require no setup, maintenance, supervision, or any
long-running processes.

`nq` should run on any POSIX.1-2008 compliant system which also
provides a working flock(2).  Tested on Linux 2.6.37, Linux 4.1,
OpenBSD 5.7, FreeBSD 10.1, NetBSD 7.0.2, Mac OS X 10.3 and
SmartOS joyent_20160304T005100Z.

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
- Why flock(2)? Because it locks the file handle, which is shared
  across exec(2) with the child process (the actual job), and it will
  unlock when the file is closed (usually when the job terminates).

You enqueue (get it?) new jobs using `nq CMDLINE...`.  The job id is
output (unless suppressed using `-q`) and `nq` detaches immediately,
running the job in the background.  STDOUT and STDERR are redirected
into the log file.

`nq` tries hard (but does not guarantee) to ensure the log file of the
currently running job has +x bit set.  Thus you can use `ls -F` to get
a quick overview of the state of your queue.

The "file extension" of the log file is actually the PID, so you can
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

## Examples

Build targets `clean`, `depends`, `all`, without occupying the terminal:

	% nq make clean
	% nq make depends
	% nq make all
	% fq
	... look at output, can interrupt with C-c any time
	without stopping the build ...

Simple download queue, accessible from multiple terminals:

	% mkdir -p /tmp/downloads
	% alias qget='NQDIR=/tmp/downloads nq wget'
	% alias qwait='NQDIR=/tmp/downloads fq -q'
	window1% qget http://mymirror/big1.iso
	window2% qget http://mymirror/big2.iso
	window3% qget http://mymirror/big3.iso
	% qwait
	... wait for all downloads to finish ...

As nohup replacement (The benchmark will run in background, every run
gets a different output file, and the command line you ran is logged
too.):

	% ssh remote
	remote% nq ./run-benchmark
	,14f6f3034f8.17035
	remote% ^D
	% ssh remote
	remote% fq
	... see output, fq exits when job finished ...

## Assumptions

`nq` will only work correctly when:
- `$NQDIR` (respectively `.`) is writable.
- `flock(2)` works in `$NQDIR` (respectively `.`).
- `gettimeofday` behaves monotonic (using `CLOCK_MONOTONIC` would
  create confusing file names).  Else job order can be confused and
  multiple tasks can run at once due to race conditions.
- No other programs put files matching `,*` into `$NQDIR` (respectively `.`).

## nq helpers

Two helper programs are provided:

`fq` outputs the log of the currently running jobs, exiting when the
jobs are done.  If no job is running, the output of the last job is
shown.  `fq -a` shows the output of all jobs, `fq -q` only shows one
line per job.  `fq` uses `inotify` on Linux and falls back to polling
for size change else.  (`fq.sh` is a similar tool, not quite as robust,
implemented as shell-script calling `tail`.)

`tq` wraps `nq` and displays the `fq` output in a new tmux or screen window.

(A pure shell implementation of `nq` is provided as `nq.sh`.  It needs
`flock` from util-linux, and only has a timer resolution of 1s.
Lock files from `nq` and `nq.sh` should not be mixed.)

## Installation

Use `make all` to build, `make install` to install relative to `PREFIX`
(`/usr/local` by default).  The `DESTDIR` convention is respected.
You can also just copy the binaries into your `PATH`.

You can use `make check` to run a simple test suite, if you have
Perl's `prove` installed.

## Copyright

nq is in the public domain.

To the extent possible under law,
Leah Neukirchen <leah@vuxu.org>
has waived all copyright and related or
neighboring rights to this work.

http://creativecommons.org/publicdomain/zero/1.0/
