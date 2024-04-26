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

# Test of a pipeline's capacity, and whether program outputs larger than the size of the pipe
# buffer are allowed (processes aren't prematrurely killed in task_runner.c). This test tries to
# download Rick Astley's "Never Gonna Give You Up", bass boost it using ffmpeg, and show it to user
# using mpv without ever writing to the file system.

. "$(dirname "$0")/utils.sh" || exit 1

orchestrator_pid=$(start_orchestrator 1 fcfs "/dev/null") || exit 1

assert_installed_command "yt-dlp"
assert_installed_command "ffmpeg"
assert_installed_command "mpv"

pipeline=(
	"yt-dlp https://youtu.be/dQw4w9WgXcQ -o - |"
	"ffmpeg -i - -af bass=g=20:f=110:w=0.6 -vcodec copy -acodec aac -f matroska - |"
	"mpv -"
)
./bin/client execute 100 -p "${pipeline[*]}" >/dev/null || exit 1

echo "Please wait a moment ..."

while pgrep mpv > /dev/null; do sleep 1; done # Assume no other MPV is running
stop_orchestrator false "$orchestrator_pid"
