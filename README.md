# shellfish

a terminal shell written from scratch in c, no external libs, just the
native ones (unistd, sys/wait, fcntl etc). made it to actually understand
how a shell works under the hood (fork, pipe, dup2, all that stuff) and
also to have something to actually use day to day.

## what it does

- runs any external system command (ls, grep, cat, whatever)
- pipes: `ls | grep c | wc -l`
- redirection: `>` `>>` and `<`
- runs stuff in the background with `&` (and doesnt leave zombie
  processes behind, already took care of that)
- expands environment variables, like `$HOME` `${HOME}` and even `$$`
  for the pid
- single quotes dont expand anything, double quotes do, same as bash
- command history that saves to a hidden file in your home
  (`~/.shellfish_history`) and loads it back up next time you open the shell

### builtin commands

| command | what it does |
|---|---|
| `cd [dir]` | change directory, no argument goes home, `cd -` goes back to the previous one |
| `pwd` | show the current directory |
| `export VAR=value` | set an environment variable |
| `unset VAR` | remove an environment variable |
| `history` | show the commands typed so far |
| `help` | show a quick rundown of all this |
| `exit [code]` | quit the shell |

anything thats not on that list it tries to find in the PATH and run as
a regular program.

## how to build

just need gcc and make, nothing else.

```bash
make
./shellfish
```

if you want to install it to run from anywhere:

```bash
sudo make install
```

that copies the binary to `/usr/local/bin`, then you just type
`shellfish` in any terminal.

## code structure

```
src/
  main.c         entry point, calls the loop and returns the exit code
  shell.c/.h     main loop, prompt, line reading
  parser.c/.h    turns the typed text into commands/pipes/redirections
  exec.c/.h      fork, pipe, dup2, actually runs the commands
  builtins.c/.h  the internal commands (cd, exit, export, etc)
  history.c/.h   command history, saves and loads from the file
```

## quick example

```
your_user:~$ echo hello world
hello world
your_user:~$ ls | grep .c | wc -l
6
your_user:~$ echo test > output.txt
your_user:~$ cat output.txt
test
your_user:~$ sleep 5 &
[bg] 12345
your_user:~$ cd /tmp
your_user:/tmp$ cd -
/tmp
your_user:~$
```

## stuff it doesnt have yet

- no tab autocomplete
- no arrow-up history navigation (just the `history` command for now)
- no alias support
- variables only work for reading ($VAR), cant do `VAR=value` directly
  without export

if you want to send a pull request for any of this id be happy, just
open an issue first so we can talk it through.

## license

mit, feel free to use it, copy it, modify it, whatever.
