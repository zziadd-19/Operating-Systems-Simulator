const socket = io();
const terminal = document.getElementById('terminal');
const cycleDisplay = document.getElementById('cycle-display');
const algoSelect = document.getElementById('algorithm');
const quantumDiv = document.getElementById('quantum-div');

// Show/Hide Quantum input based on RR selection
algoSelect.addEventListener('change', (e) => {
    if (e.target.value === '2') quantumDiv.style.display = 'block';
    else quantumDiv.style.display = 'none';
});

document.getElementById('start-btn').addEventListener('click', () => {
    // Clear the terminal for a new run
    terminal.innerText = 'Booting System...\n';
    cycleDisplay.innerText = '0';
    
    // ---> UPDATE THIS: Send the Arrival Times to Node <---
    socket.emit('start_simulation', {
        arr1: document.getElementById('arr1').value,
        arr2: document.getElementById('arr2').value,
        arr3: document.getElementById('arr3').value,
        algorithm: algoSelect.value,
        quantum: document.getElementById('quantum').value
    });
    // -----------------------------------------------------
});

// Receive live trace data from the C program
socket.on('os_output', (data) => {
    terminal.innerText += data;
    terminal.scrollTop = terminal.scrollHeight; // Auto-scroll to bottom
});

// Update the big Clock Widget dynamically
socket.on('update_cycle', (cycleNum) => {
    cycleDisplay.innerText = cycleNum;
});

// ---> ADD THIS: Handle User Typing into the Terminal <---
const termInput = document.getElementById('terminal-input');
const sendBtn = document.getElementById('send-btn');

sendBtn.addEventListener('click', () => {
    const text = termInput.value;
    socket.emit('terminal_input', text); // Send it to Node!
    termInput.value = ''; // Clear the box
});

// Let the user press "Enter" on their keyboard to send
termInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') sendBtn.click();
});
// --------------------------------------------------------