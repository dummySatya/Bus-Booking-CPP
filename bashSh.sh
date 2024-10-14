#!/bin/bash

# Run the server and capture the PID
./a.out &
echo $! > server_pid.txt
pid=$(cat server_pid.txt)

# Track CPU and memory usage with pidstat
pidstat -p $pid 1 > server_stats.log &

# Track I/O activity with iotop
sudo iotop -b -p $pid -n 10 > server_io.log &

# Capture memory mapping details with pmap
pmap $pid > memory_map.log

# Track disk activity with iostat
iostat -p $pid 1 > disk_activity.log &

# Optionally, wait for server process to finish
wait $pid

# Stop all monitoring tools after server finishes
killall pidstat iostat
sudo killall iotop
