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
# stderr.

if [ -z "TESTER_SCRIPT" ]; then
	. "$(dirname "$0")/utils.sh" || exit 1

	orchestrator_pid=$(start_orchestrator 1 fcfs "$(readlink "/proc/self/1")") || exit 1
	./bin/client execute 100 -u 'sh -c "TESTER_SCRIPT=1 ./tests/fds.sh"' > /dev/null
	stop_orchestrator true "$orchestrator_pid"
else
	echo "Open file descriptors:"
	for fd in /proc/self/fd/*; do
		if [ "$fd" = "/proc/self/fd/255" ]; then # Bash specific file descriptor
			continue
		fi

		if ! name="$(readlink "$fd")"; then
			name="?"
		fi
		echo "$fd: $name"
	done
fi
