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

# This test attempts to overload the input FIFO of the server. If a single child process of the
# server fails to be waited for due to a communication failure, this test will block. This test also
# shows the errors that are recovered from.

. "$(dirname "$0")/utils.sh" || exit 1

echo "Starting competition for the FIFO. You should see errors that are recovered from."

server_err=$(mktemp) || exit 1
for i in $(seq 1 10); do
	orchestrator_pid=$(start_orchestrator 1 fcfs "$server_err") || exit 1
	for j in $(seq 1 10000); do
		./bin/client execute 100 -u "echo $j" > /dev/null || echo "Client died" 1>&2
	done
	stop_orchestrator true "$orchestrator_pid"
	echo "Progress: $((i * 10))%"
done

echo "Server's complaints:"
cat "$server_err"
rm "$server_err"
