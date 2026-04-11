#include "os_sim.h"

// --- GLOBAL HARDWARE SIMULATION ---
MemoryWord main_memory[40]; // Your strict 40-word memory limit
int current_time = 0;

// Initialize your three required Semaphores (Value 1 = Unlocked)
BinarySemaphore userInput = {1, -1, {{0}, 0, 0, 0}};
BinarySemaphore userOutput = {1, -1, {{0}, 0, 0, 0}};
BinarySemaphore fileAccess = {1, -1, {{0}, 0, 0, 0}};

// Initialize your system queues
Queue ready_queue = {{0}, 0, 0, 0};
Queue general_blocked_queue = {{0}, 0, 0, 0};



int main() {
    printf("Starting Operating System Simulation...\n");
    printf("Clock Cycle: %d\n", current_time);

    // TODO: Write a function to read Program 1 from the .txt file
    // TODO: Write a function to load Program 1 into main_memory at mem_start 0

    // The Main CPU Loop
    int active_processes = 1; // Temporarily set to 1 to test the loop
    
    while(active_processes > 0) {
        // Here is where you will check the scheduler, fetch the next instruction
        // using the Program Counter, and execute it.
        
        printf("Executing clock cycle...\n");
        active_processes = 0; // Break the loop for now so it doesn't run forever
    }

    printf("Simulation Complete.\n");
    return 0;
}