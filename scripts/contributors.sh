#!/bin/sh

# This script counts lines (includes comments and empty lines) committed by
# each contributor.

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
assert_installed_command git

find Makefile src include scripts theme -type f \
	-exec grep -I -q . {} \; -print | while read -r file; do

	# Only report files tracked by git
	if git ls-files --error-unmatch "$file" > /dev/null 2>&1; then
		git blame --line-porcelain "$file" \
			| grep '^author ' | sed 's/^author //g'
	fi
done | sort -f | uniq -ic
