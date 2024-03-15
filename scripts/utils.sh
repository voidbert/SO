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
