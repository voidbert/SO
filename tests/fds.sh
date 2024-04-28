#!/bin/bash
# |
# \_ bash is used so that the server can be spawned as a daemon.

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

# Test whether the programs executed by the server have only 3 file descriptors, stdout, stdin and
# stderr. THIS TEST IS ONLY COMPATIBLE WITH LINUX.

if [ -z "$1" ]; then
	. "$(dirname "$0")/utils.sh" || exit 1

	orchestrator_pid=$(start_orchestrator 1 fcfs "test.txt") || exit 1
	./bin/client execute 100 -u "./tests/fds.sh 0" > /dev/null
	./bin/client execute 100 -p "./tests/fds.sh 1 | ./tests/fds.sh 2 | ./tests/fds.sh 3" > /dev/null
	stop_orchestrator true "$orchestrator_pid"

	cat "/tmp/orchestrator/1.out" ; echo "" ; cat "/tmp/orchestrator/2.out"
else
	[ "$1" -gt 1 ] && while IFS= read -r output_line; do echo "$output_line"; done && echo ""

	if [ "$1" -eq 0 ]; then
		echo "Open file descriptors (unique program):"
	else
		echo "Open file descriptors (pipeline step $1):"
	fi

	for fd in /proc/self/fd/*; do
		# Remove bash specific file descriptor
		if [ "$fd" = "/proc/self/fd/255" ]; then continue; fi

		if ! name="$(readlink "$fd")"; then
			name="?"
		fi
		echo "$fd: $name"
	done
fi
