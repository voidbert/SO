#!/bin/sh

# This script formats the source code, and asks the user if they wish to keep
# the changes.

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

assert_installed_command clang-format
assert_installed_command git

out_dir="$(mktemp -d)"
diff_path="$(mktemp)"
mkdir "$out_dir/src" "$out_dir/include"

find src include -type f | while read -r file; do
	mkdir -p "$(dirname "$out_dir/$file")"
	clang-format "$file" | sed "\$a\\" | sed 's/\s*$//' > "$out_dir/$file"
done

git --no-pager -c color.ui=always diff --no-index "src" "$out_dir/src" \
	> "$diff_path"
git --no-pager -c color.ui=always diff --no-index "include" "$out_dir/include" \
	>> "$diff_path"

if ! [ -s "$diff_path" ]; then
	echo "Already formatted! Leaving ..."
	rm -r "$diff_path" "$out_dir"
	exit 0
elif [ "$1" = "--check" ]; then
	echo "Formatting errors!"
	cat "$diff_path"
	rm -r "$diff_path" "$out_dir"
	exit 1
else
	less -R "$diff_path"
fi

if yesno "Agree with these changes? [Y/n]: " true; then
	perform_changes=true
else
	echo "Source code left unformatted. Leaving ..."
	perform_changes=false
fi

if $perform_changes; then
	cp -r "$out_dir/src"     .
	cp -r "$out_dir/include" .
fi

# Clean up and exit with correct exit code
rm -r "$diff_path" "$out_dir"
$perform_changes
exit $?
