#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "os_sim.h"

// --- GLOBAL HARDWARE SIMULATION ---
MemoryWord hard_disk[100];
MemoryWord main_memory[40]; 

// Initialize your three required Semaphores 
BinarySemaphore userInput = {1, -1, {{0}, 0, 0, 0}};
BinarySemaphore userOutput = {1, -1, {{0}, 0, 0, 0}};
BinarySemaphore fileAccess = {1, -1, {{0}, 0, 0, 0}};

// Initialize your system queues
Queue ready_queue = {{0}, 0, 0, 0};
Queue general_blocked_queue = {{0}, 0, 0, 0};


// --- HELPER FUNCTIONS ---

int get_running_process(PCB processes[], int total) {
    for (int i = 0; i < total; i++) {
        if (processes[i].state == STATE_RUNNING) {
            return processes[i].processID;
        }
    }
    return -1; // No one is running
}

// Helper to find the value of a variable in a process's memory block
char* get_variable_value(int processID, char* varName, MemoryWord memory[], PCB processes[]) {
    int p_idx = processID - 1; // --> FIX: Convert ID to 0-based array index
    int start = processes[p_idx].mem_start;
    
    // Variables are always at offsets +4, +5, and +6
    for (int i = 4; i <= 6; i++) {
        if (strcmp(memory[start + i].name, varName) == 0) {
            return memory[start + i].value;
        }
    }
    return varName; // If not found, assume it's a literal value (like "10")
}

// Helper to update a variable's value or assign a new one to an empty slot
void set_variable_value(int processID, char* varName, char* newValue, MemoryWord memory[], PCB processes[]) {
    int p_idx = processID - 1; // --> FIX: Convert ID to 0-based array index
    int start = processes[p_idx].mem_start;
    
    // First, try to find if the variable already exists to update it
    for (int i = 4; i <= 6; i++) {
        if (strcmp(memory[start + i].name, varName) == 0) {
            strcpy(memory[start + i].value, newValue);
            return;
        }
    }
    
    // If not found, find the first "NULL" slot to assign it
    for (int i = 4; i <= 6; i++) {
        if (strcmp(memory[start + i].value, "NULL") == 0) {
            strcpy(memory[start + i].name, varName);
            strcpy(memory[start + i].value, newValue);
            return;
        }
    }
}

// Helper function to display the current state of RAM
void print_memory_state(MemoryWord memory[]) {
    printf("\n--- Main Memory Content ---\n");
    int free_space = 0;
    
    for (int i = 0; i < 40; i++) {
        // If the slot is occupied, print it cleanly
        if (memory[i].name[0] != '\0') {
            // %-15s adds spacing so the colons line up perfectly in the terminal
            printf("[%02d] %-15s : %s\n", i, memory[i].name, memory[i].value);
        } else {
            free_space++;
        }
    }
    printf("-> [Free Space: %d/40 words]\n", free_space);
    printf("---------------------------\n");
}


// --- MAIN OS EXECUTION ---

int main()
{
    // --- 1. INITIALIZATION (RESTORED) ---
    int current_time = 0;
    int finished_processes = 0;
    int scheduling_choice;
    
    PCB processes[3]; 
    bool loaded[3] = {false, false, false};

    for (int i = 0; i < 3; i++) {
        processes[i].mem_start = -1;
        processes[i].mem_end = -1;
    }

    // --> ADD THESE THREE LINES BACK <--
    init_memory(main_memory);
    initQueue(&ready_queue);
    initQueue(&general_blocked_queue);

    initQueue(&userInput.blocked_queue);
    initQueue(&userOutput.blocked_queue);
    initQueue(&fileAccess.blocked_queue);

    // Wipe Hard Disk clean
    for (int i = 0; i < 100; i++) {
        hard_disk[i].name[0] = '\0';
        hard_disk[i].value[0] = '\0';
    }


    // --- 2. SCHEDULING PROMPT ---
    printf("Select Scheduling Technique:\n");
    printf("1. Highest Response Ratio Next (HRRN)\n");
    printf("2. Round Robin (RR)\n");
    printf("3. Multilevel Feedback Queue (MLFQ)\n");
    printf("Choice: ");
    scanf("%d", &scheduling_choice);


    // --- 3. EXECUTION LOOPS ---
    if (scheduling_choice == 1)
    {
        printf("\n--- Starting HRRN  ---\n");
        
        // --- PRE-LOOP SETUP ---
        int runningProcessID = -1;
        int arrival_times[3] = {0, 1, 4};
        int service_times[3];
        int disk_locations[3] = {-1, -1, -1}; 
        const char *programs[] = {"Program_1.txt", "Program_2.txt", "Program_3.txt"};

        // Calculate service times dynamically
        for (int i = 0; i < 3; i++) {
            service_times[i] = get_program_memory_size(programs[i]) - 7;
        }
        
        while (finished_processes < 3)
        {
            printf("\n=== Clock Cycle: %d ===\n", current_time);

            // 1. ARRIVAL & LOADER CHECK
            for (int i = 0; i < 3; i++)
            {
                if (current_time == arrival_times[i] && !loaded[i])
                {
                    int space_needed = service_times[i] + 7;
                    int start_idx = find_first_empty_index(main_memory);

                    // Memory Check
                    if (start_idx == -1 || (start_idx + space_needed) > 40)
                    {
                        printf("[Memory Manager]: Memory Full! Need to swap a process to Disk...\n");
                        
                        int victim_id = get_victim_process(processes, 3, current_time, arrival_times, service_times);
                        disk_locations[victim_id - 1] = swap_to_disk(victim_id, processes, main_memory, hard_disk);
                        
                        // Shift remaining RAM up to close the gap
                        compact_memory(processes, 3, main_memory);
                        
                        start_idx = find_first_empty_index(main_memory);
                    }

                    // Load the process and save its PCB
                    processes[i] = load_program_to_memory(main_memory, start_idx, programs[i], i + 1);

                    enqueue(&ready_queue, processes[i].processID);
                    loaded[i] = true;
                }
            }

            // 2. SCHEDULER: HRRN Selection 
            if (runningProcessID == -1 && !isEmpty(&ready_queue))
            {
                double max_ratio = -1.0;
                int best_id = -1;

                for (int i = 0; i < ready_queue.count; i++)
                {
                    int id = ready_queue.process_ids[(ready_queue.front + i) % MAX_PROCESSES];
                    int p_idx = id - 1; 

                    double w = (double)(current_time - arrival_times[p_idx]);
                    double s = (double)service_times[p_idx];
                    double ratio = (w + s) / s;

                    if (ratio > max_ratio)
                    {
                        max_ratio = ratio;
                        best_id = id;
                    }
                }

                remove_from_queue(&ready_queue, best_id);
                runningProcessID = best_id;
                processes[runningProcessID - 1].state = STATE_RUNNING;

                printf("[Scheduler]: Selected P%d (HRRN Ratio: %.2f)\n", runningProcessID, max_ratio);
            }

            // 3. EXECUTION: Run the current process
            if (runningProcessID != -1)
            {
                if (!is_process_in_memory(runningProcessID, processes))
                {
                    printf("[OS Alert]: P%d is on the Disk! Need to swap it back in.\n", runningProcessID);
                    
                    ensure_process_in_memory(runningProcessID, processes, main_memory, hard_disk, 
                                             disk_locations, service_times, current_time, arrival_times);
                }
                
                int p_idx = runningProcessID - 1;
                int pc = processes[p_idx].program_counter;

                // Fetch and Execute
                printf("[CPU]: P%d executing -> '%s'\n", runningProcessID, main_memory[pc].value);
                execute_instruction(main_memory[pc].value, runningProcessID, main_memory, processes);

                // Increment the Program Counter
                processes[p_idx].program_counter++;

                // 4. STATE CHECKS
                if (processes[p_idx].program_counter > processes[p_idx].mem_end)
                {
                    printf("[OS]: Process P%d has Finished Execution.\n", runningProcessID);
                    
                    // Wipe the finished process from RAM
                    for (int j = processes[p_idx].mem_start; j <= processes[p_idx].mem_end; j++) {
                        main_memory[j].name[0] = '\0';
                        main_memory[j].value[0] = '\0';
                    }

                    processes[p_idx].mem_start = -1;
                    processes[p_idx].mem_end = -1;
                    processes[p_idx].state = STATE_FINISHED;

                    // Shift remaining processes up to close the gap
                    compact_memory(processes, 3, main_memory);
                    
                    runningProcessID = -1; 
                    finished_processes++;
                }
                else if (processes[p_idx].state == STATE_BLOCKED)
                {
                    printf("[OS]: Process P%d is BLOCKED. Context switching...\n", runningProcessID);
                    runningProcessID = -1; 
                }
            }
            else
            {
                printf("[CPU]: IDLE\n");
            }

            // 5. SYSTEM TRACE
            printQueue(&ready_queue, "Ready Queue");

            // Print all Blocked and Semaphore Queues
            printQueue(&general_blocked_queue, "General Blocked Queue");
            printQueue(&userInput.blocked_queue, "userInput Semaphore Queue");
            printQueue(&userOutput.blocked_queue, "userOutput Semaphore Queue");
            printQueue(&fileAccess.blocked_queue, "fileAccess Semaphore Queue");

            print_memory_state(main_memory);
            print_disk_state(hard_disk);

            current_time++; 
        }
    }
    else if (scheduling_choice == 2)
    {
        printf("\n--- Starting Round Robin Execution ---\n");

        // --- 1. SETUP RR VARIABLES ---
        initQueue(&ready_queue); // Ensure the line is completely clean
        
        int time_quantum;
        printf("Enter the Time Quantum (Q): ");
        scanf("%d", &time_quantum);
        printf("-> Time Quantum set to %d ticks.\n", time_quantum);

        int process_ticks = 0; // Stopwatch for the current process
        int runningProcessID = -1;

        int arrival_times[3] = {0, 1, 4};
        int service_times[3];
        int disk_locations[3] = {-1, -1, -1};
        const char *programs[] = {"Program_1.txt", "Program_2.txt", "Program_3.txt"};

        // Calculate service times dynamically
        for (int i = 0; i < 3; i++) {
            service_times[i] = get_program_memory_size(programs[i]) - 7;
        }

        while (finished_processes < 3)
        {
            printf("\n=== Clock Cycle: %d ===\n", current_time);

            // --- 2. ARRIVALS & MEMORY SWAPPING ---
            for (int i = 0; i < 3; i++)
            {
                if (current_time == arrival_times[i] && !loaded[i])
                {
                    int space_needed = service_times[i] + 7;
                    int start_idx = find_first_empty_index(main_memory);

                    if (start_idx == -1 || (start_idx + space_needed) > 40) {
                        printf("[Memory Manager]: Memory Full! Need to swap a process to Disk...\n");
                        int victim_id = get_victim_process(processes, 3, current_time, arrival_times, service_times);
                        disk_locations[victim_id - 1] = swap_to_disk(victim_id, processes, main_memory, hard_disk);
                        compact_memory(processes, 3, main_memory);
                        start_idx = find_first_empty_index(main_memory);
                    }

                    processes[i] = load_program_to_memory(main_memory, start_idx, programs[i], i + 1);
                    
                    // Put brand new processes at the back of the line
                    enqueue(&ready_queue, processes[i].processID);
                    loaded[i] = true;
                }
            }

            // --- 3. PREEMPTION LOGIC (TIME QUANTUM EXPIRED) ---
            if (runningProcessID != -1) {
                if (process_ticks >= time_quantum) {
                    printf("[Round Robin]: P%d exceeded Time Quantum (%d ticks). Moving to back of line...\n", runningProcessID, time_quantum);
                    processes[runningProcessID - 1].state = STATE_READY;
                    
                    // Throw them to the back of the ready_queue
                    enqueue(&ready_queue, runningProcessID);
                    runningProcessID = -1;
                }
            }

            // --- 4. THE SCHEDULER ---
            // If the CPU is empty, just grab whoever is at the very front of the line
            if (runningProcessID == -1 && !isEmpty(&ready_queue)) {
                runningProcessID = dequeue(&ready_queue);
                process_ticks = 0; // Reset stopwatch for the new process
                processes[runningProcessID - 1].state = STATE_RUNNING;
                printf("[Scheduler]: Selected P%d from Ready Queue\n", runningProcessID);
            }

            // --- 5. EXECUTION ---
            if (runningProcessID != -1)
            {
                if (!is_process_in_memory(runningProcessID, processes)) {
                    printf("[OS Alert]: P%d is on the Disk! Need to swap it back in.\n", runningProcessID);
                    ensure_process_in_memory(runningProcessID, processes, main_memory, hard_disk, 
                                             disk_locations, service_times, current_time, arrival_times);
                }
                
                int p_idx = runningProcessID - 1;
                int pc = processes[p_idx].program_counter;

                printf("[CPU]: P%d executing -> '%s'\n", runningProcessID, main_memory[pc].value);
                execute_instruction(main_memory[pc].value, runningProcessID, main_memory, processes);
                
                // ---> THE STATE FIXER <---
                // Because interpreter.c directly enqueues awoken processes into ready_queue
                // but leaves their state as BLOCKED, we do a quick scan to force them to READY.
                for (int i = 0; i < ready_queue.count; i++) {
                    int id = ready_queue.process_ids[(ready_queue.front + i) % MAX_PROCESSES];
                    if (processes[id - 1].state == STATE_BLOCKED) {
                        processes[id - 1].state = STATE_READY;
                        printf("[Round Robin]: Process P%d woke up and is Ready!\n", id);
                    }
                }
                // --------------------------

                processes[p_idx].program_counter++;
                process_ticks++; // Tick the quantum stopwatch!

                // --- 6. STATE CHECKS ---
                if (processes[p_idx].program_counter > processes[p_idx].mem_end)
                {
                    printf("[OS]: Process P%d has Finished Execution.\n", runningProcessID);
                    for (int j = processes[p_idx].mem_start; j <= processes[p_idx].mem_end; j++) {
                        main_memory[j].name[0] = '\0';
                        main_memory[j].value[0] = '\0';
                    }
                    processes[p_idx].mem_start = -1;
                    processes[p_idx].mem_end = -1;
                    processes[p_idx].state = STATE_FINISHED;

                    compact_memory(processes, 3, main_memory);
                    runningProcessID = -1; 
                    finished_processes++;
                }
                else if (processes[p_idx].state == STATE_BLOCKED)
                {
                    printf("[OS]: Process P%d is BLOCKED. Context switching...\n", runningProcessID);
                    runningProcessID = -1; 
                }
            }
            else
            {
                printf("[CPU]: IDLE\n");
            }

            // --- 7. SYSTEM TRACE (With the Memory Synchronizer!) ---
            for (int p = 0; p < 3; p++) {
                if (processes[p].mem_start != -1) {
                    int state_idx = processes[p].mem_start + 1; 
                    int pc_idx = processes[p].mem_start + 2;    
                    
                    // Sync the State String
                    if (processes[p].state == STATE_READY) strcpy(main_memory[state_idx].value, "READY");
                    else if (processes[p].state == STATE_RUNNING) strcpy(main_memory[state_idx].value, "RUNNING");
                    else if (processes[p].state == STATE_BLOCKED) strcpy(main_memory[state_idx].value, "BLOCKED");

                    // Sync the PC Number
                    sprintf(main_memory[pc_idx].value, "%d", processes[p].program_counter);
                }
            }

            printQueue(&ready_queue, "Ready Queue");

            // Print all Blocked and Semaphore Queues
            printQueue(&general_blocked_queue, "General Blocked Queue");
            printQueue(&userInput.blocked_queue, "userInput Semaphore Queue");
            printQueue(&userOutput.blocked_queue, "userOutput Semaphore Queue");
            printQueue(&fileAccess.blocked_queue, "fileAccess Semaphore Queue");

            print_memory_state(main_memory);
            print_disk_state(hard_disk);

            current_time++; 
        }
    }
    else if (scheduling_choice == 3)
    {
        printf("\n--- Starting MLFQ Execution ---\n");

        // --- 1. SETUP MLFQ VARIABLES ---
        Queue q0, q1, q2, q3;
        initQueue(&q0);
        initQueue(&q1);
        initQueue(&q2);
        initQueue(&q3);
        
        initQueue(&ready_queue); // ---> THIS LINE FIXES THE P0 BUG <---

        int current_queue_level = -1; // Tracks which queue the active process came from
        int process_ticks = 0;        // Stopwatch for the Time Quantum
        int runningProcessID = -1;

        int process_queue_history[3] = {0, 0, 0};

        int arrival_times[3] = {0, 1, 4};
        int service_times[3];
        int disk_locations[3] = {-1, -1, -1};
        const char *programs[] = {"Program_1.txt", "Program_2.txt", "Program_3.txt"};

        // Calculate service times
        for (int i = 0; i < 3; i++) {
            service_times[i] = get_program_memory_size(programs[i]) - 7;
        }

        while (finished_processes < 3)
        {
            printf("\n=== Clock Cycle: %d ===\n", current_time);

            // --- 2. ARRIVALS & MEMORY SWAPPING ---
            for (int i = 0; i < 3; i++)
            {
                if (current_time == arrival_times[i] && !loaded[i])
                {
                    int space_needed = service_times[i] + 7;
                    int start_idx = find_first_empty_index(main_memory);

                    if (start_idx == -1 || (start_idx + space_needed) > 40) {
                        printf("[Memory Manager]: Memory Full! Need to swap a process to Disk...\n");
                        int victim_id = get_victim_process(processes, 3, current_time, arrival_times, service_times);
                        disk_locations[victim_id - 1] = swap_to_disk(victim_id, processes, main_memory, hard_disk);
                        compact_memory(processes, 3, main_memory);
                        start_idx = find_first_empty_index(main_memory);
                    }

                    processes[i] = load_program_to_memory(main_memory, start_idx, programs[i], i + 1);
                    
                    // MLFQ RULE: All new processes start in the highest priority queue (Q0)
                    enqueue(&q0, processes[i].processID);
                    loaded[i] = true;
                }
            }

            // --- 3. THE BOUNCER (PREEMPTION LOGIC) ---
            if (runningProcessID != -1) {
                bool kicked = false;

                // ---> FIX: CHECK TIME QUANTUM FIRST <---
                if (current_queue_level == 0 && process_ticks >= 1) {
                    printf("[MLFQ]: P%d exceeded Q0 Quantum. Demoting to Q1...\n", runningProcessID);
                    enqueue(&q1, runningProcessID);
                    kicked = true;
                } 
                else if (current_queue_level == 1 && process_ticks >= 2) {
                    printf("[MLFQ]: P%d exceeded Q1 Quantum. Demoting to Q2...\n", runningProcessID);
                    enqueue(&q2, runningProcessID);
                    kicked = true;
                } 
                else if (current_queue_level == 2 && process_ticks >= 4) {
                    printf("[MLFQ]: P%d exceeded Q2 Quantum. Demoting to Q3...\n", runningProcessID);
                    enqueue(&q3, runningProcessID);
                    kicked = true;
                }
                else if (current_queue_level == 3 && process_ticks >= 8) {
                    printf("[MLFQ]: P%d exceeded Q3 Quantum. Cycling back to end of Q3...\n", runningProcessID);
                    enqueue(&q3, runningProcessID);
                    kicked = true;
                }

                // ---> THEN CHECK FOR HIJACKS (If they haven't already been demoted) <---
                if (!kicked) {
                    bool higher_waiting = false;
                    if (current_queue_level > 0 && !isEmpty(&q0)) higher_waiting = true;
                    if (current_queue_level > 1 && !isEmpty(&q1)) higher_waiting = true;
                    if (current_queue_level > 2 && !isEmpty(&q2)) higher_waiting = true;

                    if (higher_waiting) {
                        printf("[MLFQ]: Priority Hijack! A higher priority process stole the CPU from P%d.\n", runningProcessID);
                        
                        // Put the interrupted process back into its CURRENT queue
                        if (current_queue_level == 1) enqueue(&q1, runningProcessID);
                        else if (current_queue_level == 2) enqueue(&q2, runningProcessID);
                        else if (current_queue_level == 3) enqueue(&q3, runningProcessID);
                        
                        kicked = true;
                    }
                }

                // Free the CPU
                if (kicked) {
                    processes[runningProcessID - 1].state = STATE_READY;
                    runningProcessID = -1; 
                }
            }

            // --- 4. THE SCHEDULER (STRICT PRIORITY SELECTION) ---
            if (runningProcessID == -1) {
                if (!isEmpty(&q0)) {
                    runningProcessID = dequeue(&q0);
                    current_queue_level = 0;
                } else if (!isEmpty(&q1)) {
                    runningProcessID = dequeue(&q1);
                    current_queue_level = 1;
                } else if (!isEmpty(&q2)) {
                    runningProcessID = dequeue(&q2);
                    current_queue_level = 2;
                } else if (!isEmpty(&q3)) {
                    runningProcessID = dequeue(&q3);
                    current_queue_level = 3;
                }

                // If we successfully picked someone, reset their stopwatch and mark them running
                if (runningProcessID != -1) {
                    process_ticks = 0; 
                    processes[runningProcessID - 1].state = STATE_RUNNING;

                    process_queue_history[runningProcessID - 1] = current_queue_level;

                    printf("[Scheduler]: Selected P%d from Q%d\n", runningProcessID, current_queue_level);
                }
            }

            // --- 5. EXECUTION ---
            if (runningProcessID != -1)
            {
                if (!is_process_in_memory(runningProcessID, processes)) {
                    printf("[OS Alert]: P%d is on the Disk! Need to swap it back in.\n", runningProcessID); 
                    ensure_process_in_memory(runningProcessID, processes, main_memory, hard_disk, 
                                             disk_locations, service_times, current_time, arrival_times);
                }
                
                int p_idx = runningProcessID - 1;
                int pc = processes[p_idx].program_counter;

                printf("[CPU]: P%d executing -> '%s'\n", runningProcessID, main_memory[pc].value);
                execute_instruction(main_memory[pc].value, runningProcessID, main_memory, processes);
                
                // ---> THE MISSING CATCHER BLOCK <---
                while (!isEmpty(&ready_queue)) {
                    int woken_id = ready_queue.process_ids[ready_queue.front];
                    remove_from_queue(&ready_queue, woken_id);
                    
                    if (woken_id >= 1 && woken_id <= 3) {
                        processes[woken_id - 1].state = STATE_READY;
                        
                        // Look up where this process came from
                        int target_q = process_queue_history[woken_id - 1];
                        printf("[MLFQ]: Process P%d woke up! Returning it to its bookmarked Q%d.\n", woken_id, target_q);
                        
                        // Put it back in its historical queue
                        if (target_q == 0) enqueue(&q0, woken_id);
                        else if (target_q == 1) enqueue(&q1, woken_id);
                        else if (target_q == 2) enqueue(&q2, woken_id);
                        else if (target_q == 3) enqueue(&q3, woken_id);
                    }
                }
                // -----------------------------------

                processes[p_idx].program_counter++;
                process_ticks++; // TICK THE STOPWATCH for the active process!

                // --- 6. STATE CHECKS ---
                // ... (the rest of your code is perfect)

                if (processes[p_idx].program_counter > processes[p_idx].mem_end)
                {
                    printf("[OS]: Process P%d has Finished Execution.\n", runningProcessID);
                    for (int j = processes[p_idx].mem_start; j <= processes[p_idx].mem_end; j++) {
                        main_memory[j].name[0] = '\0';
                        main_memory[j].value[0] = '\0';
                    }
                    processes[p_idx].mem_start = -1;
                    processes[p_idx].mem_end = -1;
                    processes[p_idx].state = STATE_FINISHED;

                    compact_memory(processes, 3, main_memory);
                    runningProcessID = -1; 
                    finished_processes++;
                }
                else if (processes[p_idx].state == STATE_BLOCKED)
                {
                    printf("[OS]: Process P%d is BLOCKED. Context switching...\n", runningProcessID);
                    runningProcessID = -1; 
                    // Note: We do NOT queue them here. The semSignal handles waking them up.
                }
            }
            else
            {
                printf("[CPU]: IDLE\n");
            }

            // --- 7. SYSTEM TRACE ---

            // ---> THE FIX: SYNC PCB STATE TO MAIN MEMORY STRINGS <---
            for (int p = 0; p < 3; p++) {
                // If the process is currently loaded in memory
                if (processes[p].mem_start != -1) {
                    int state_idx = processes[p].mem_start + 1; // State is always at offset +1
                    int pc_idx = processes[p].mem_start + 2;    // PC is at offset +2
                    
                    if (processes[p].state == STATE_READY) {
                        strcpy(main_memory[state_idx].value, "READY");
                    } else if (processes[p].state == STATE_RUNNING) {
                        strcpy(main_memory[state_idx].value, "RUNNING");
                    } else if (processes[p].state == STATE_BLOCKED) {
                        strcpy(main_memory[state_idx].value, "BLOCKED");
                    }
                    // 2. Sync the PC Number (Convert int to string)
                    sprintf(main_memory[pc_idx].value, "%d", processes[p].program_counter);
                }
            }

            printQueue(&q0, "Queue 0 (Q=1)");
            printQueue(&q1, "Queue 1 (Q=2)");
            printQueue(&q2, "Queue 2 (Q=4)");
            printQueue(&q3, "Queue 3 (Q=8)");

            // Print all Blocked and Semaphore Queues
            printQueue(&general_blocked_queue, "General Blocked Queue");
            printQueue(&userInput.blocked_queue, "userInput Semaphore Queue");
            printQueue(&userOutput.blocked_queue, "userOutput Semaphore Queue");
            printQueue(&fileAccess.blocked_queue, "fileAccess Semaphore Queue");

            print_memory_state(main_memory);
            print_disk_state(hard_disk);

            current_time++; 
        }
    }

    printf("\nAll processes executed. System shutting down at clock cycle %d.\n", current_time);
    return 0;
}