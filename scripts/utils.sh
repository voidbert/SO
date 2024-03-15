#!/bin/sh

# This script contains utilities for use in other scripts. Also, when sourced,
# it sets $PWD to the repository's root.

# Copyright 2024 Humberto Gomes, José Lopes, José Matos
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if [ "$(basename "$0")" = "utils.sh" ]; then
	echo "This script isn't meant to be executed, but sourced." >&2
	exit 1
fi

REPO_DIR="$(realpath "$(dirname -- "$0")/..")"
cd "$REPO_DIR" || exit 1

# Gets an environment variable from the Makefile. It must a constant, without
# quotes and independent from other variables, as its value is gotten by
# searching for that variable's assignment in the Makefile. Also, the variable
# must be assigned with :=
#
# $1 - variable name
# stdout - variable's value
get_makefile_const() {
	if ! [ -f "$REPO_DIR/Makefile" ]; then
		echo "Makefile has been deleted! Leaving ..." >&2
		exit 1
	fi

	< "$REPO_DIR/Makefile" grep "^$1\\s*:=" | sed "s/^$1\s*:=\s*//g"
}

# Checks if a command is installed, and exits the program with an error message
# if it is not.
#
# $1 - name of the command
assert_installed_command() {
	if ! command -v "$1" > /dev/null; then
		printf "%s is not installed! Please install it and try again. " "$1" >&2
		echo   "Leaving ..." >&2
		exit 1
	fi
}

# Asks a yes / no question to the user.
# $1 - Prompt to be shown to the user
# $2 - Whether an invalid input should considered to be a no, instead of an
#      invalid input that causes the program to exit, the default. Must be
#      true or false.
# Return value - 0 (success) for yes, 1 (error) for no
yesno() {
	stdbuf -o 0 printf "$1"
	read -r yn

	if [ -z "$2" ]; then
		consider_invalid_no=true
	fi

	if echo "$yn" | grep -Eq '^[Yy]([Ee][Ss])?$'; then
		return 0
	elif echo "$yn" | grep -Eq '^[Nn][Oo]?$'; then
		return 1
	else
		echo "Invalid input. Leaving ..." >&2
		if $consider_invalid_no; then
			return 1
		else
			exit 1
		fi
	fi
}

# Ask the user to choose an element from a list of elements. The program will be left on invalid
# user input.
# $1 - Prompt
# $2 - Variable name of where to place the chosen element (like read)
# $3 - Elements, newline separated
choose() {
	line_count="$(echo "$3" | wc -l)"

	i=0
	echo "$3" | while read -r element; do
		i=$((i + 1))
		spaces=$(printf "%$(( ${#line_count} - ${#i} ))s")
		printf "  (%s)%s - %s\n" "$i" "$spaces" "$element"
	done

	stdbuf -o 0 printf "$1"
	read -r user_input
	if ! (echo "$user_input" | grep -qE '^[0-9\-]+$') || \
		[ "$user_input" -lt 1 ] || [ "$user_input" -gt "$line_count" ]; then

		echo "Invalid input. Leaving ..." >&2
		exit 1
	fi

	# shellcheck disable=SC2034
	result="$(echo "$3" | sed "${user_input}q;d")"
	eval "$2=\"\$result\""
}
