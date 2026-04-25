#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "os_sim.h"

// INITILIZATION

MemoryWord hard_disk[100];
MemoryWord main_memory[40]; 


BinarySemaphore userInput = {1, -1, {{0}, 0, 0, 0}};
BinarySemaphore userOutput = {1, -1, {{0}, 0, 0, 0}};
BinarySemaphore fileAccess = {1, -1, {{0}, 0, 0, 0}};


Queue ready_queue = {{0}, 0, 0, 0};
Queue general_blocked_queue = {{0}, 0, 0, 0};


//HELPER FUNCTIONS

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
    int p_idx = processID - 1; 
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
    int p_idx = processID - 1; 
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
        if (memory[i].name[0] != '\0') {
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
    // ---> Disable buffering so Node.js gets live text! <---
    setbuf(stdout, NULL);
    
    // --- 1. INITIALIZATION ---
    int current_time = 0;
    int finished_processes = 0;
    int scheduling_choice;
    
    PCB processes[3]; 
    bool loaded[3] = {false, false, false};

    for (int i = 0; i < 3; i++) {
        processes[i].mem_start = -1;
        processes[i].mem_end = -1;
    }

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


    // --- 2. OS SETUP & SYSTEM CONFIGURATION ---
    int arrival_times[3];
    const char *programs[] = {"Program_1.txt", "Program_2.txt", "Program_3.txt"};
    
    printf("\n--- System Configuration ---\n");
    printf("Enter Arrival Time for P1 (%s): ", programs[0]);
    scanf("%d", &arrival_times[0]);
    printf("Enter Arrival Time for P2 (%s): ", programs[1]);
    scanf("%d", &arrival_times[1]);
    printf("Enter Arrival Time for P3 (%s): ", programs[2]);
    scanf("%d", &arrival_times[2]);

    // Calculate service times globally
    int service_times[3];
    for (int i = 0; i < 3; i++) {
        service_times[i] = get_program_memory_size(programs[i]) - 7;
    }
    
    int disk_locations[3] = {-1, -1, -1}; 

    // --> SCHEDULING PROMPT <--
    printf("\nSelect Scheduling Technique:\n");
    printf("1. Highest Response Ratio Next (HRRN)\n");
    printf("2. Round Robin (RR)\n");
    printf("3. Multilevel Feedback Queue (MLFQ)\n");
    printf("Choice: ");
    scanf("%d", &scheduling_choice);


    // --- 3. EXECUTION LOOPS ---
    if (scheduling_choice == 1)
    {
        printf("\n--- Starting HRRN  ---\n");
        int runningProcessID = -1;
        
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
            for (int p = 0; p < 3; p++) {
                if (processes[p].mem_start != -1) {
                    int state_idx = processes[p].mem_start + 1; 
                    int pc_idx = processes[p].mem_start + 2;    
                    
                    if (processes[p].state == STATE_READY) strcpy(main_memory[state_idx].value, "READY");
                    else if (processes[p].state == STATE_RUNNING) strcpy(main_memory[state_idx].value, "RUNNING");
                    else if (processes[p].state == STATE_BLOCKED) strcpy(main_memory[state_idx].value, "BLOCKED");

                    sprintf(main_memory[pc_idx].value, "%d", processes[p].program_counter);
                }
            }

            printQueue(&ready_queue, "Ready Queue");
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
        initQueue(&ready_queue); 
        
        int time_quantum;
        printf("Enter the Time Quantum (Q): ");
        scanf("%d", &time_quantum);
        printf("-> Time Quantum set to %d ticks.\n", time_quantum);

        int process_ticks = 0; 
        int runningProcessID = -1;

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
                    
                    enqueue(&ready_queue, processes[i].processID);
                    loaded[i] = true;
                }
            }

            // --- 3. PREEMPTION LOGIC (TIME QUANTUM EXPIRED) ---
            if (runningProcessID != -1) {
                if (process_ticks >= time_quantum) {
                    printf("[Round Robin]: P%d exceeded Time Quantum (%d ticks). Moving to back of line...\n", runningProcessID, time_quantum);
                    processes[runningProcessID - 1].state = STATE_READY;
                    
                    enqueue(&ready_queue, runningProcessID);
                    runningProcessID = -1;
                }
            }

            // --- 4. THE SCHEDULER ---
            if (runningProcessID == -1 && !isEmpty(&ready_queue)) {
                runningProcessID = dequeue(&ready_queue);
                process_ticks = 0; 
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
                for (int i = 0; i < ready_queue.count; i++) {
                    int id = ready_queue.process_ids[(ready_queue.front + i) % MAX_PROCESSES];
                    if (processes[id - 1].state == STATE_BLOCKED) {
                        processes[id - 1].state = STATE_READY;
                        printf("[Round Robin]: Process P%d woke up and is Ready!\n", id);
                    }
                }

                processes[p_idx].program_counter++;
                process_ticks++;

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

            // --- 7. SYSTEM TRACE ---
            for (int p = 0; p < 3; p++) {
                if (processes[p].mem_start != -1) {
                    int state_idx = processes[p].mem_start + 1; 
                    int pc_idx = processes[p].mem_start + 2;    
                    
                    if (processes[p].state == STATE_READY) strcpy(main_memory[state_idx].value, "READY");
                    else if (processes[p].state == STATE_RUNNING) strcpy(main_memory[state_idx].value, "RUNNING");
                    else if (processes[p].state == STATE_BLOCKED) strcpy(main_memory[state_idx].value, "BLOCKED");

                    sprintf(main_memory[pc_idx].value, "%d", processes[p].program_counter);
                }
            }

            printQueue(&ready_queue, "Ready Queue");
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
        initQueue(&q0); initQueue(&q1); initQueue(&q2); initQueue(&q3);
        initQueue(&ready_queue); 

        // --> TA EVALUATION VARIABLES: Change these if asked! <--
        int q0_quantum = 1;
        int q1_quantum = 2;
        int q2_quantum = 4;
        int q3_quantum = 8;
        // -------------------------------------------------------

        int current_queue_level = -1; 
        int process_ticks = 0;        
        int runningProcessID = -1;
        int process_queue_history[3] = {0, 0, 0};

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
                    
                    enqueue(&q0, processes[i].processID);
                    loaded[i] = true;
                }
            }

            // --- 3. THE BOUNCER (PREEMPTION LOGIC) ---
            if (runningProcessID != -1) {
                bool kicked = false;

                // ---> CHECK TIME QUANTUM FIRST <---
                if (current_queue_level == 0 && process_ticks >= q0_quantum) {
                    printf("[MLFQ]: P%d exceeded Q0 Quantum. Demoting to Q1...\n", runningProcessID);
                    enqueue(&q1, runningProcessID);
                    kicked = true;
                } 
                else if (current_queue_level == 1 && process_ticks >= q1_quantum) {
                    printf("[MLFQ]: P%d exceeded Q1 Quantum. Demoting to Q2...\n", runningProcessID);
                    enqueue(&q2, runningProcessID);
                    kicked = true;
                } 
                else if (current_queue_level == 2 && process_ticks >= q2_quantum) {
                    printf("[MLFQ]: P%d exceeded Q2 Quantum. Demoting to Q3...\n", runningProcessID);
                    enqueue(&q3, runningProcessID);
                    kicked = true;
                }
                else if (current_queue_level == 3 && process_ticks >= q3_quantum) {
                    printf("[MLFQ]: P%d exceeded Q3 Quantum. Cycling back to end of Q3...\n", runningProcessID);
                    enqueue(&q3, runningProcessID);
                    kicked = true;
                }

                // ---> THEN CHECK FOR HIJACKS <---
                if (!kicked) {
                    bool higher_waiting = false;
                    if (current_queue_level > 0 && !isEmpty(&q0)) higher_waiting = true;
                    if (current_queue_level > 1 && !isEmpty(&q1)) higher_waiting = true;
                    if (current_queue_level > 2 && !isEmpty(&q2)) higher_waiting = true;

                    if (higher_waiting) {
                        printf("[MLFQ]: Priority Hijack! A higher priority process stole the CPU from P%d.\n", runningProcessID);
                        
                        if (current_queue_level == 1) enqueue(&q1, runningProcessID);
                        else if (current_queue_level == 2) enqueue(&q2, runningProcessID);
                        else if (current_queue_level == 3) enqueue(&q3, runningProcessID);
                        
                        kicked = true;
                    }
                }

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
                        int target_q = process_queue_history[woken_id - 1];
                        printf("[MLFQ]: Process P%d woke up! Returning it to its bookmarked Q%d.\n", woken_id, target_q);
                        
                        if (target_q == 0) enqueue(&q0, woken_id);
                        else if (target_q == 1) enqueue(&q1, woken_id);
                        else if (target_q == 2) enqueue(&q2, woken_id);
                        else if (target_q == 3) enqueue(&q3, woken_id);
                    }
                }

                processes[p_idx].program_counter++;
                process_ticks++; 

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

            // --- 7. SYSTEM TRACE ---
            for (int p = 0; p < 3; p++) {
                if (processes[p].mem_start != -1) {
                    int state_idx = processes[p].mem_start + 1; 
                    int pc_idx = processes[p].mem_start + 2;    
                    
                    if (processes[p].state == STATE_READY) {
                        strcpy(main_memory[state_idx].value, "READY");
                    } else if (processes[p].state == STATE_RUNNING) {
                        strcpy(main_memory[state_idx].value, "RUNNING");
                    } else if (processes[p].state == STATE_BLOCKED) {
                        strcpy(main_memory[state_idx].value, "BLOCKED");
                    }
                    sprintf(main_memory[pc_idx].value, "%d", processes[p].program_counter);
                }
            }

            printQueue(&q0, "Queue 0 (Q=1)");
            printQueue(&q1, "Queue 1 (Q=2)");
            printQueue(&q2, "Queue 2 (Q=4)");
            printQueue(&q3, "Queue 3 (Q=8)");

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