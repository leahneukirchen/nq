#!/bin/sh
# fq - tail -F the queue outputs, quitting when the job finishes

tailed=false
for f in ${NQDIR:-.}/,*; do
	if ! nq -t $f; then
		tailed=true
		printf '==> %s\n' "$f"
		tail -F $f & p=$!
		nq -w $f
		kill $p
	fi
done

if ! $tailed; then
	cat $f
fi
