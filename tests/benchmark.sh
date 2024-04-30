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

# This test benchmarks different scheduling policies.

. "$(dirname "$0")/utils.sh" || exit 1

remove_units() {
	if [ "${1: -1}" = "u" ]; then
		# shellcheck disable=SC2001
		echo "$1" | sed 's/u$//'
	elif [ "${1: -1}" = "m" ]; then
		# shellcheck disable=SC2001
		awk 'BEGIN { printf "%.3f\n", '"$(echo "$1" | sed 's/m$//')"' * 1e3 }'
	else
		awk 'BEGIN { printf "%.3f\n", '"$1"' * 1e6 }'
	fi
}

for sched in "sjf" "fcfs"; do
	echo "$sched:"
	orchestrator_pid=$(start_orchestrator 1 "$sched" "/dev/null") || exit 1

	for _ in $(seq 1 100); do
		time_ms=$((RANDOM % 1700 + 300))
		time_s="$(awk 'BEGIN { printf "%.3f", '"$time_ms"' / 1000 }')"
		./bin/client execute "$time_ms" -u "sleep $time_s" > /dev/null || echo "Client died" 1>&2
	done

	echo "Waiting for the tasks to terminate ..."
	while pgrep -P "$orchestrator_pid" > /dev/null; do sleep 1; done

	./bin/client status | \
		tail +2 | \
		awk '{ print substr($4, 1, 5) "s\n" $5 "\n" $6 "\n" $7 "\n" $8 }' |
		sed 's/.$//' | \
		while read -r line; do remove_units "$line"; done | \
		xargs -n 5 | \
		tr ' ' ','

	stop_orchestrator true "$orchestrator_pid"
done
