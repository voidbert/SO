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

# This test attempts to send very long messages to make sure the server handles them correctly.

MAX_LENGTH=4063 # To know this value, run ./bin/client execute 100 -u ""

. "$(dirname "$0")/utils.sh" || exit 1

orchestrator_pid=$(start_orchestrator 1 fcfs "/dev/null") || exit 1

message="$(yes | tr -d '\n' | head -c"$MAX_LENGTH")"
./bin/client execute 100 -u "$(echo "$message" | head -c$((MAX_LENGTH - 20)))" > /dev/null 2>&1 || \
	echo "MAX_LENGTH too long" 1>&2
./bin/client execute 100 -u "$message" > /dev/null 2>&1 || \
	echo "MAX_LENGTH too long" 1>&2
./bin/client execute 100 -u "y$message" > /dev/null 2>&1 && \
	echo "MAX_LENGTH shorter than real maximum" 1>&2

./bin/client status
stop_orchestrator true "$orchestrator_pid"
