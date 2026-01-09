#!/bin/bash

# Monitor both Arduino serial outputs simultaneously using tmux

echo "=== Starting Dual Serial Monitor ==="
echo ""

# Check if tmux is installed
if ! command -v tmux &> /dev/null; then
    echo "❌ tmux is not installed"
    echo "   Install with: sudo apt-get install tmux"
    echo ""
    echo "Alternative: Open two separate terminals and run:"
    echo "   Terminal 1: pio device monitor -e initiator"
    echo "   Terminal 2: pio device monitor -e responder"
    exit 1
fi

cd "$(dirname "$0")/.." || exit 1

# Kill existing session if it exists
tmux kill-session -t uwb_monitor 2>/dev/null

echo "Creating tmux session with split panes..."
echo ""
echo "Controls:"
echo "  - Ctrl+B then →  : Switch to right pane"
echo "  - Ctrl+B then ←  : Switch to left pane"
echo "  - Ctrl+B then D  : Detach (monitors keep running)"
echo "  - Ctrl+C then exit : Close monitor"
echo "  - Type 'exit' in both panes to close session"
echo ""
echo "Press Enter to continue..."
read

# Create new session with split panes
tmux new-session -d -s uwb_monitor

# Split window vertically
tmux split-window -h -t uwb_monitor

# Send commands to each pane
tmux send-keys -t uwb_monitor:0.0 'cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB && echo "=== INITIATOR (Left) ===" && pio device monitor -e initiator' C-m
tmux send-keys -t uwb_monitor:0.1 'cd /home/devel/Desktop/SwarmLoc/DWS1000_UWB && echo "=== RESPONDER (Right) ===" && pio device monitor -e responder' C-m

# Set pane titles
tmux select-pane -t uwb_monitor:0.0 -T "Initiator"
tmux select-pane -t uwb_monitor:0.1 -T "Responder"

# Attach to session
tmux attach -t uwb_monitor
