#!/bin/bash
# arg1:  user@host 
# arg2:  gdbserver port 
# arg3:  source path of local program to copy to target
# arg4:  target path of program on target to debug


echo "Copy from $3 to $1:$4"
# TODO: Check if source file existing and target file unlocked
echo "scp $3 $1:$4"

scp $3 $1:$4
# Command to execute on remote target
echo "Setting capability of target program $4"
COMMAND="sudo setcap cap_sys_nice+ep $4; gdbserver --once :$2 $4; sleep 10"

# Start xterm and bash inside
xterm -e ssh $1 $COMMAND

