# shellfish

a small terminal shell written in c

## features

- run external commands
- pipe commands
- redirect input and output
- run commands in the background
- expand environment variables
- keep command history

## builtins

| command | action |
|---|---|
| `cd [dir]` | change directory |
| `pwd` | print the current directory |
| `export name=value` | set an environment variable |
| `unset name` | remove an environment variable |
| `history` | show command history |
| `help` | show available commands |
| `exit [code]` | leave the shell |

## structure

```text
shellfish/
├── src/
│   ├── main.c
│   ├── shell.c
│   ├── parser.c
│   ├── exec.c
│   ├── builtins.c
│   └── history.c
├── include/
│   ├── shell.h
│   ├── parser.h
│   ├── exec.h
│   ├── builtins.h
│   └── history.h
├── makefile
├── readme.md
├── .gitignore
└── license
```

## build

```bash
make
```

## run

```bash
./shellfish
```

## license

mit license
