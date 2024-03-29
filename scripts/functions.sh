#!/bin/bash


ISR_DEATH_SIGS="SIGSEGV SIGILL SIGABRT SIGFPE SIGQUIT SIGHUP SIGINT"

function gen_transposition_key {
	t=0

	for i in {0..15}; do
		while [ 1 ]; do
			n=$RANDOM
			let "n%=16"
			let "b = 1 << n"
			let "e = b & t"
			if [ $e -eq 0 ]; then
				let "t|=b"
				break
			fi
		done
		enckey=$enckey`printf "%04x" $b`
	done
}

function get_xor_key {
	n=$RANDOM
	let "enckey=n+1"
	enckey=`printf "%04x" $enckey`
}


function generate_key {
	if [ $ENCTYPE = "transposition" ]; then
		OBJCP="$OBJCPBIN --encrypt-trans-key"
		gen_transposition_key
	else
		OBJCP="$OBJCPBIN --encrypt-xor-key"
		get_xor_key
	fi
}
