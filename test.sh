#!/usr/bin/env bash

# Description
#
# This script generates a file containing random commands to be used by pmanager
# in test mode. Each command is chosen randomly from the "commands" array.
# If a command requires a process name as argument, it is provided by selecting
# a random one from the "proc_names" array, which is kept updated with each pnew
# command generation.
#
# Notes:
#  - for simplicity, "proc_names" is not updated on pspawn, prmall, and pclose
#  - pnew is assumed to be successful

if [ "$#" -ne 2 ]; then
  echo "Usage: $(basename "$0") <commands_count> <output_file>"
  exit 1
fi

command_count=$1
out=$2
commands=("phelp" "plist" "pnew" "pinfo" "pclose" "pspawn" "prmall" "ptree")

# Remove output file if it already exists
[ -e "$out" ] && rm "$out"

# Add a random command command_count times
proc_count=0
for ((i = 1; i <= command_count; i++))
do
  # Pick a random command
  rand=$((RANDOM % ${#commands[@]}))
  cmd="${commands[$rand]}"
  # Add argument to command, if needed
  case $cmd in
    pnew)
      # Use an incremental name for the new process
      proc_name="dummy$((++proc_count))"
      # Add new proceess to proc_names
      proc_names+=("$proc_name")
      # Write command with argument to file
      cmd="$cmd $proc_name"
      echo "$cmd" >> "$out"
      ;;
    pinfo|pclose|pspawn|prmall)
      # If proc_names is empty, it makes no sense to write these commands,
      # because pnew/pspawn was never written
      if [ "${#proc_names[@]}" -gt 0 ]; then
        # Pick a random process from proc_names
        rand=$((RANDOM % ${#proc_names[@]}))
        proc_name="${proc_names[$rand]}"
        # Write command with argument to file
        cmd="$cmd $proc_name"
        echo "$cmd" >> "$out"
      fi
      ;;
    *)
      echo "$cmd" >> "$out"
  esac
done
