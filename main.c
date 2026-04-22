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


// --- MAIN OS EXECUTION ---

int main()
{
    // --- 1. INITIALIZATION (RESTORED) ---
    int current_time = 0;
    int finished_processes = 0;
    int scheduling_choice;
    
    PCB processes[3]; 
    bool loaded[3] = {false, false, false};

    // --> ADD THESE THREE LINES BACK <--
    init_memory(main_memory);
    initQueue(&ready_queue);
    initQueue(&general_blocked_queue);

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
            current_time++; 
        }
    }
    else if (scheduling_choice == 2)
    {
        printf("\n--- Starting Round Robin Execution ---\n");
        while (finished_processes < 3) {}
    }
    else if (scheduling_choice == 3)
    {
        printf("\n--- Starting MLFQ Execution ---\n");
        while (finished_processes < 3) {}
    }

    printf("\nAll processes executed. System shutting down at clock cycle %d.\n", current_time);
    return 0;
}