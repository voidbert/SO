#!/bin/bash
# |
# \_ bash is used so that the server can be spawned as a daemon.

# Copyright 2024 Humberto Gomes, José Lopes, José Matos
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#	 http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This test checks if tasks are being scheduled in the correct order according the chosen policy.

. "$(dirname "$0")/utils.sh" || exit 1

failed=false

for sched in "sjf" "fcfs"; do
	orchestrator_pid=$(start_orchestrator 1 "$sched" "/dev/null") || exit 1

	./bin/client execute 1 -u "sleep 1" > /dev/null || echo "Client died" 1>&2
	for i in $(seq 1 10); do
		./bin/client execute $((100 - i)) -u "echo Hello" > /dev/null || echo "Client died" 1>&2
	done

	while pgrep -P "$orchestrator_pid" > /dev/null; do sleep 1; done # Wait for all processes

	order=$(./bin/client status | tail +2 | awk '{print $2}' | sed -z 's/:\n/ /g' | head -c-1)
	if [ "$sched" = "fcfs" ] && [ "$order" != "$(seq -s ' ' 1 11)" ]; then
		echo "FCFS test failed: $order" | sed -z 's/\n/ /g; s/$/\n/g'
		failed=true
	elif [ "$sched" = "sjf" ] && [ "$order" != "1 $(seq -s ' ' 11 -1 2)" ]; then
		echo "SJF test failed: '$order'" | sed -z 's/\n/ /g; s/$/\n/g'
		failed=true
	fi

	stop_orchestrator true "$orchestrator_pid"
done

$failed || echo "No tests failed :-)"
