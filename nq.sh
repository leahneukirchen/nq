#!/bin/sh
# nq CMD... - run CMD... in background and in order, saving output to ,* files
#
# - needs POSIX sh + util-linux flock(1) (see nq.c for portable version)
# - when run from tmux, display output in a new window (needs
#   GNU tail, C-c to abort the job.)
# - we try hard to make the currently running ,* file have +x bit
# - enforcing order works like this:
#   - every job has a flock(2)ed file
#   - every job starts only after all earlier flock(2)ed files finished
#   - the lock is released when job terminates
#
# To the extent possible under law, Leah Neukirchen <leah@vuxu.org>
# has waived all copyright and related or neighboring rights to this work.
# http://creativecommons.org/publicdomain/zero/1.0/

if [ -z "$NQ" ]; then
	export NQ=$(date +%s)
	"$0" "$@" & c=$!
	(
		# wait for job to finish
		flock -x .,$NQ.$c -c true
		flock -x ,$NQ.$c -c true
		chmod -x ,$NQ.$c
	) &
	exit
fi

us=",$NQ.$$"

exec 9>>.$us
# first flock(2) the file, then make it known under the real name
flock -x 9
mv .$us $us

printf "## nq $*" 1>&9

if [ -n "$TMUX" ]; then
	tmux new-window -a -d -n '<' -c '#{pane_current_path}' \
		"trap true INT QUIT TERM EXIT;
                 tail -F --pid=$$ $us || kill $$;
		 printf '\n[%d exited, ^D to exit.]\n' $$;
		 cat >/dev/null"
fi
    
waiting=true
while $waiting; do
	waiting=false
	# this must traverse in lexical (= numerical) order:
        # check all older locks are released
	for f in ,*; do
		# reached the current lock, good to go
		[ $f = $us ] && break

		if ! flock -x -n $f -c "chmod -x $f"; then
			# force retrying all locks again;
			# an earlier lock could just now have really appeared
			waiting=true
			flock -x $f -c true
		fi
	done
done

printf '\n' 1>&9

chmod +x $us
exec "$@" 2>&1 1>&9
