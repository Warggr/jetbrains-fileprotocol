target:
	mkdir $@

target/fileprotocol: src/main.c | target
	${CC} -Wall -Werror $< -o $@

tests/%.success: tests/%.in tests/%.out.server tests/%.out.file target/fileprotocol
	rm $@ /tmp/file.$* # in case it already exists; do not propagate the error if it doesn't
	set -e
	target/fileprotocol /tmp/socket /tmp/file.$* | diff - tests/$*.out.server &
	cat tests/$*.in | scripts/client /tmp/socket
	diff /tmp/file.$* tests/$*.out.file
	touch $@
