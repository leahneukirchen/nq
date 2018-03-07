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
