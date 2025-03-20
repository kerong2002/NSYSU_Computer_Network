#!/bin/bash

tmux kill-session -t mysession 2>/dev/null
tmux new-session -d -s mysession \
	'set -g mouse on; bash' \; \
	split-window -h \; \
	split-window -v \; \
	send-keys -t 0 './client' C-m \; \
	send-keys -t 1 './server' C-m \; \
	send-keys -t 2 './router' C-m \; \
	attach-session -t mysession
