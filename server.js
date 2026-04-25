const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const { spawn } = require('child_process');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

app.use(express.static('public')); // Serve the GUI

io.on('connection', (socket) => {
    console.log('Frontend connected to OS Simulator');
    let osProcess = null;

    // Listen for the "start" command from the GUI
    socket.on('start_simulation', (data) => {
        // Spawn the C executable (Use './os_sim' for Mac/Linux)
        osProcess = spawn('./os_sim.exe'); 

        // ---> THE UPDATE: Feed the data exactly as the C program asks for it <---
        // 1. Send Arrival Times first
        osProcess.stdin.write(`${data.arr1}\n`);
        osProcess.stdin.write(`${data.arr2}\n`);
        osProcess.stdin.write(`${data.arr3}\n`);

        // 2. Send the scheduling choice
        osProcess.stdin.write(`${data.algorithm}\n`);
        
        // 3. If Round Robin, send the Time Quantum
        if (data.algorithm === '2') {
            osProcess.stdin.write(`${data.quantum}\n`);
        }
        // --------------------------------------------------------------------------

        // ---> THE FIX: Sliding window buffer to catch split chunks <---
        let outputBuffer = ""; 

        // Capture the C program's output and send it to the GUI
        osProcess.stdout.on('data', (output) => {
            const text = output.toString();
            
            // 1. Emit instantly to the terminal so your prompts don't get delayed
            socket.emit('os_output', text);
            
            // 2. Add the text to our sliding window memory
            outputBuffer += text;
            
            // 3. Search the stitched buffer for the cycle
            const cycleMatches = [...outputBuffer.matchAll(/=== Clock Cycle: (\d+) ===/g)];
            if (cycleMatches.length > 0) {
                // Grab the very last match found in this stitched text
                const latestCycle = cycleMatches[cycleMatches.length - 1][1];
                socket.emit('update_cycle', latestCycle);
            }

            // 4. Shrink the buffer! Keep only the last 100 characters.
            // This prevents memory leaks while ensuring we don't accidentally cut 
            // a '=== Clock Cycle ===' string in half for the next chunk.
            if (outputBuffer.length > 100) {
                outputBuffer = outputBuffer.slice(-100);
            }
        });

        osProcess.on('close', (code) => {
            socket.emit('os_output', `\n[System] Simulation finished with code ${code}`);
        });
    });

    // Listen for manual terminal inputs from the GUI text box
    socket.on('terminal_input', (text) => {
        if (osProcess) {
            osProcess.stdin.write(`${text}\n`);
        }
    });
});

server.listen(3000, () => {
    console.log('GUI Server running at http://localhost:3000');
});