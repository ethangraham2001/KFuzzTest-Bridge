# KFuzzTest Bridge

This program serves as a bridge between a random blob of data, and KFuzzTest. In
short, it allows a user to describe the expected input format of a KFuzzTest
target in textual format, and patch all fields with data from some specified
source file.

## Usage

First, build the binary with `make`. You may have to do this on the system being
(e.g., QEMU VM) to avoid GLIBC incompatibilities.

Example usage:

```sh
./kfuzztest-bridge \
    "foo { u32 ptr[bar] }; bar { ptr[data] }; data { arr[u8, 42] };" \
    "my-fuzz-target" /dev/urandom
```

Where

1. `argv[1]` is a textual representation of the expected structure of the fuzz
   target.   
2. `argv[2]` is the name of the fuzz target (i.e., it's directory name under
   `/sys/kernel/debug/kfuzztest`).
3. `argv[3]` is the name of some file from which data will be read, preferably
   pseudo-random data as you may find in `/dev/urandom`.

In the usage example, the textual input format corresponds to the following
C struct representation.

```c
struct foo {
    u32 a;
    struct bar *b;
};

struct bar {
    struct data *d;
};

struct data {
    char arr[42];
};
```

## Textual Input Format

KFuzzTest uses a region-based encoding in its input format. This is described
in detail in the [initial RFC](https://lore.kernel.org/all/20250813133812.926145-1-ethan.w.s.graham@gmail.com/).

The textual input format is also region-based, like the binary format, and is
described by the following grammar:

```
schema      ::= region ( ";" region )* [";"]

region      ::= identifier "{" type+ "}"

type        ::= primitive | pointer | array

primitive   ::= "u8" | "u16" | "u32" | "u64"
pointer     ::= "ptr" "[" identifier "]"
array       ::= "arr" "[" primitive "," integer "]"

identifier  ::= [a-zA-Z_][a-zA-Z0-9_]*
integer     ::= [0-9]+
```

Note that raw arrays must also be defined inside of a region. For example:

```c
struct my_struct {
    char *buf;
    size_t buflen;
};

/* Defined as: "my_struct { ptr[buf] u64 }; buf { arr[u8, <size>] };'*/
```
