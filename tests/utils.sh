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

# Utilities for the test scripts.

if [ "$(basename "$0")" = "utils.sh" ]; then
	echo "This script isn't meant to be executed, but sourced." >&2
	exit 1
fi

REPO_DIR="$(realpath "$(dirname -- "$0")/..")"
cd "$REPO_DIR" || exit 1

# Checks if a command is installed, and exits the program with an error message if it is not.
#
# $1 - name of the command.
assert_installed_command() {
	if ! command -v "$1" > /dev/null; then
		printf "%s is not installed! Please install it and try again. " "$1" >&2
		echo   "Leaving ..." >&2
		exit 1
	fi
}

# Runs the orchestrator as a daemon.
#
# $1 - Number of concurrent tasks.
# $2 - Scheduling policy (fcfs / sjf).
# $3 - Output redirection.
#
# stdout - PID of the orchestrator, nothing on failure.
# return - 0 on success, 1 on failure.
start_orchestrator() {
	if pgrep "orchestrator" > /dev/null; then
		echo "Orchestrator already running!" 1>&2
		return 1
	fi

	rm /tmp/*.fifo > /dev/null 2>&1
	nohup "./bin/orchestrator" "/tmp/orchestrator" "$1" "$2" 0<&- &> "$3" &
	pgrep "orchestrator"
	return 0
}

# Waits for the orchestrator daemon to have no children left and kills it.
#
# $1 - Whether to wait for all the child processes to have terminated.
# $2 - Orchestrator's PID.
stop_orchestrator() {
	$1 && while pgrep -P "$2" > /dev/null; do sleep 1; done
	kill "$2"
	rm "/tmp/orchestrator.fifo" > "/dev/null" 2>&1
}
