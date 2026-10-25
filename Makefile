PYTHON ?= python3
CC ?= cc

.PHONY: test native

test:
	$(PYTHON) test_pmcfg.py
	$(PYTHON) test_editor.py
	$(PYTHON) test_tools.py
	mkdir -p build
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 test_transaction.c -o build/test-transaction
	./build/test-transaction
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 test_snapshot.c -o build/test-snapshot
	./build/test-snapshot
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 test_query.c -o build/test-query
	./build/test-query
	$(CC) -std=c99 -Wall -Wextra -Werror -O2 test_status.c -o build/test-status
	./build/test-status

native:
	sh build.sh
