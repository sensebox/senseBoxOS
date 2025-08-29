# senseBoxOS

Pseudocode Sender GUI
- Connect over USB Serial and send lines to your interpreter.
- Buttons for RUN, RUNLOOP, STOP.
- Multiline editor to paste your pseudocode; each line is sent with a trailing newline '\n'.
- Port dropdown + refresh + connect/disconnect.
- Shows device log output (from Serial) in the right pane.

Requirements:
    pip install pyserial

Tested with Python 3.8+ on Windows/macOS/Linux.