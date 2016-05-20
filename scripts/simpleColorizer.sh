#!/bin/sh
filename="$1"

# string INFO_COLOR = "[1;32m"; // green
# string WARNING_COLOR = "[1;36m"; // cyan
# string DEBUG_COLOR = "[1;37m"; // white
# string ERROR_COLOR = "[1;31m"; // red
# string INTRO_COLOR = "[1;37m"; // white
# string OTHER_COLOR = "[1;33m"; // yellow
# string EXITING_COLOR = "[1;40;35m"; // purple
# string NUCLEUS_COLOR = "[1;40;35m"; // purple
# string ORIGINAL_COLOR = "[0m"; // purple

while read -r line
do
	if [[ $line == *'debug'* ]] || [[ $line == *'DEBUG'* ]]; then
		printf "\e[1;37m ${line}\n"
	else
	if [[ $line == *'info'* ]] || [[  $line == *'INFO'* ]]; then
		printf "\e[1;32m ${line}\n"
	else
	if [[ $line == *'Warning'* ]] || [[  $line == *'WARN'* ]]; then
		printf "\e[1;36m ${line}\n"
	else
	if [[ $line == *'Error'* ]] || [[  $line == *'ERROR'* ]]; then
		printf "\e[1;31m ${line}\n"
	else
		printf "\e[1;33m ${line}\n"
	fi
	fi
	fi
	fi
done

printf "\e[1;33m ${line}\n"