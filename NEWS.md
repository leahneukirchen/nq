## 1.0 (2024-07-03)

* **Incompatible change:** The fq utility has been renamed to nqtail.
* **Incompatible change:** The tq utility has been renamed to nqterm.
* nq: add support for a $NQFAILDIR

## 0.5 (2022-03-26)

* **Notable change:** nq now creates files with permissions 0666 and
  subject to your umask (like most programs that create new files).
  If your queue needs to remain secret, prohibit access to the whole
  directory.
* Support for nq in a multi-user environment: having read permission
  for queued tasks in the directory is enough to wait for them.
* Makefile: support INSTALL variable.
* Bugfix: create $NQDONEDIR properly

## 0.4 (2021-03-15)

* nq: now scales a lot better
* nq: set $NQDONEDIR to move finished jobs there
* fq: add kevent/kqueue support
* Bugfixes

## 0.3.1 (2018-03-07)

* Fix build on FreeBSD, OpenBSD and macOS.

## 0.3 (2018-03-06)

* nq: add `-c` to clean job file when the process succeeded.
* nq: avoid unnecessary quoting for the exec line.
* Bugfix when `-q` was used with empty command lines.

## 0.2.2 (2017-12-21)

* fq: fix when `$NQDIR` is set and inotify is used.  (Thanks to Sebastian Reuße)
* Support for NetBSD 7.

## 0.2.1 (2017-04-27)

* fq: `-q` erroneously was on by default.

## 0.2 (2017-04-26)

* fq: add `-n` to not wait
* Support for platforms without O_DIRECTORY.
* Support for SmartOS.

## 0.1 (2015-08-28)

* Initial release
