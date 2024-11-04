# Write-to-file server

# Usage
## Compilation
```sh
make target/fileprotocol
```

## Running
```sh
target/fileprotocol [socket] [file]
```

## Client
```
# Example usage:
$ scripts/client [socket]
ok
write hello
clear
error "Error message"
ping
22 Some random content
```
Inputing the last line sends the content with header code 22.
There is no option (yet) to send a message with a wrong content length.

## Tests
```sh
make tests/{1,2}.success
```

# Design decisions
I did not put a lot of work into this so this is still pretty basic.

I chose C because a lot of things are more straightforward there (e.g. parsing binary messages into structs).
The lack of memory safety can be compensated by having a good suite of tests (admittedly, I don't have a good suite of tests.)

To make the program more flexible, I might switch to another language in the future.
(I tried a first draft in Rust - see branch `rust` - but I ended up needing too much boilerplate and assertions.
Maybe a higher-level language that has less safety - e.g. Kotlin or Python - might be a better choice.)
