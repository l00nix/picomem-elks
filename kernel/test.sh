#!/bin/sh
set -eu
kit=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
workspace=$(mktemp -d "${TMPDIR:-/tmp}/picomem-tests.XXXXXX")
trap 'rm -f "$workspace/picofs" "$workspace/mouse" "$workspace/rw" "$workspace/lfn" "$workspace/lfnrw" "$workspace/create" "$workspace/rename" "$workspace/namei"; rmdir "$workspace"' EXIT HUP INT TERM
host_cc=${CC:-cc}
"$host_cc" -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Werror \
  -fsanitize=address,undefined "$kit/tests/test-picofs.c" -o "$workspace/picofs"
"$workspace/picofs"
"$host_cc" -std=c99 -Wall -Wextra -Werror -Wno-sign-compare \
  -fsanitize=address,undefined "$kit/../mouse/tests/test-driver.c" -o "$workspace/mouse"
"$workspace/mouse"

"$host_cc" -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Werror \
  -fsanitize=address,undefined "$kit/tests/test-picofs-write.c" -o "$workspace/rw"
"$workspace/rw"

"$host_cc" -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Werror \
  -fsanitize=address,undefined "$kit/tests/test-picofs-lfn.c" -o "$workspace/lfn"
"$workspace/lfn"

"$host_cc" -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Werror \
  -fsanitize=address,undefined "$kit/tests/test-picofs-lfn-write.c" -o "$workspace/lfnrw"
"$workspace/lfnrw"

"$host_cc" -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Werror \
  -fsanitize=address,undefined "$kit/tests/test-picofs-create.c" -o "$workspace/create"
"$workspace/create"

"$host_cc" -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -fsanitize=address,undefined "$kit/tests/test-picofs-rename.c" -o "$workspace/rename"
"$workspace/rename"

"$host_cc" -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined "$kit/tests/test-namei-rename.c" -o "$workspace/namei"
"$workspace/namei"
