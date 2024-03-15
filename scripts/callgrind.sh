#!/bin/bash
# |
# \_ bash is needed for process substitution, for fully correct filtering of
#    callgrind's stderr (POSIX shell solutions assume fd 3 isn't in use).

# This script runs and profiles the built executable.

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

. "$(dirname "$0")/utils.sh"

assert_installed_command valgrind
assert_installed_command kcachegrind

MAKEFILE_BUILDDIR="$(get_makefile_const BUILDDIR)"
if ! [ -d "$MAKEFILE_BUILDDIR" ]; then
	echo "Executables not yet built! Build them and try again. Leaving ..." >&2
	exit 1
fi

echo "Choose a program:"
choose "Executable: " EXE_PATH \
	"$(find "$MAKEFILE_BUILDDIR" -type f -executable)"

if ! [ -f "${EXE_PATH}_type" ]; then
	echo "Executable build type (${EXE_PATH}_test) not found! Leaving ..." >&2
	exit 1
elif [ "$(cat "${EXE_PATH}_type")" != "PROFILE" ]; then
	printf "Executable not built in PROFILE mode %s" "($(cat "${EXE_PATH}_type") "
	printf "used instead). Callgrind's results won't be the best possible.\n"

	if ! yesno "Proceed? [Y/n]: "; then
		echo "User cancelled action. Leaving ..."
		exit 1
	fi
fi

OUTPUT="$(mktemp)"
valgrind --tool=callgrind --collect-jumps=yes --callgrind-out-file="$OUTPUT" \
	"$EXE_PATH" "$@" 2> >(sed -zE \
		's/^(==[0-9]*==[^\n]*\n)*//g ; s/(==[0-9]*==[^\n]*\n)*$//g' >&2)
		# |
		# \_ remove ==[PID]== messages from callgrind

sh -c "kcachegrind \"$OUTPUT\" ; rm \"$OUTPUT\"" &> /dev/null &
disown
