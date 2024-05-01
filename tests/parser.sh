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

# Test for the server's command line parser (command_parser.c). Launches the server and sends it
# command lines. Then, each parsing result is checked against expected results (parsing success or
# failure).

. "$(dirname "$0")/utils.sh" || exit 1

orchestrator_pid=$(start_orchestrator 1 fcfs "/dev/null") || exit 1

found_error=false
while IFS= read -r file_line; do
	exit_code="$(echo "$file_line" | head -c1)"
	command_line="$(echo "$file_line" | cut -c2-)"

	./bin/client execute 100 -p "$command_line" > /dev/null 2>&1
	if [ "$?" -ne "$exit_code" ]; then
		echo "Test failure: $command_line" 1>&2
		found_error=true
	fi
done < tests/parser.txt

$found_error || echo "All parsing tests passed!"
stop_orchestrator true "$orchestrator_pid"
