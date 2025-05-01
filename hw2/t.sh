#!/bin/bash

# Function to check and kill process by port
kill_process_by_port() {
    PORT=$1
    PID=$(lsof -t -i:$PORT)  # Get the process ID using the port
    if [ ! -z "$PID" ]; then
        echo "Killing process on port $PORT (PID: $PID)..."
        kill -9 $PID  # Forcefully kill the process
    else
        echo "No process found on port $PORT."
    fi
}

# Close the specified ports if they are in use
kill_process_by_port 9000
kill_process_by_port 9002
kill_process_by_port 9003
kill_process_by_port 9004
kill_process_by_port 9010
kill_process_by_port 9011
kill_process_by_port 9012
kill_process_by_port 9013


# Kill any existing tmux session to avoid duplicates
tmux kill-session -t mysession 2>/dev/null

# Create a new tmux session
tmux new-session -d -s mysession

# Set tmux options (prefix, mouse)
tmux set-option -g prefix C-a
tmux bind-key C-a send-prefix
tmux set-option -g mouse on

# First horizontal split
tmux send-keys 'bash' C-m
tmux split-window -h  # Split into left-right panes

# Second horizontal split on the left pane
tmux select-pane -t 0
tmux split-window -h  # Further split the left pane

# Execute client, server, router in different panes
tmux send-keys -t 0 './server' C-m
#sleep 0.5
tmux send-keys -t 1 './router' C-m
#sleep 0.5
tmux send-keys -t 2 './client' C-m


# Wait for tmux to finish execution and attach to the session
tmux attach-session -t mysession

