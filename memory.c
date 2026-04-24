#include "os_sim.h"
#include <stdbool.h>

/**
 * 1. Initializes the memory array by clearing all names and values.
 * This should be called once when the OS boots up.
 */
void init_memory(MemoryWord memory[]) {
    for (int i = 0; i < 40; i++) {
        // Setting the first character to null effectively creates an empty string
        memory[i].name[0] = '\0';
        memory[i].value[0] = '\0';
    }
    printf("--> Hardware System: Main Memory (40 words) initialized.\n");
}

/**
 * 2. Scans the memory for the first available (empty) word.
 * Returns the index if found, or -1 if the memory is completely full.
 */
int find_first_empty_index(MemoryWord memory[]) {
    for (int i = 0; i < 40; i++) {
        // If the name is an empty string, the word is available
        if (memory[i].name[0] == '\0') {
            return i;
        }
    }
    
    // If the loop finishes without returning, the memory is full
    return -1; 
}

// Helper to find the first empty index on the hard disk
int find_first_empty_disk_index(MemoryWord disk[], int disk_size) {
    for (int i = 0; i < disk_size; i++) {
        if (disk[i].name[0] == '\0') {
            return i;
        }
    }
    return -1; // Disk is full
}

// The core swap function
// Notice the return type is now 'int' instead of 'void'
int swap_to_disk(int victim_id, PCB processes[], MemoryWord main_memory[], MemoryWord disk[]) {
    int v_idx = victim_id - 1;
    
    int start = processes[v_idx].mem_start;
    int end = processes[v_idx].mem_end;
    int process_size = end - start + 1;

    // Find a place on the disk
    int disk_start = find_first_empty_disk_index(disk, 100);

    if (disk_start == -1 || (disk_start + process_size > 100)) {
        printf("CRITICAL ERROR: Hard Disk is completely full!\n");
        return -1; // Return -1 on failure
    }

    // --- STEP 1: Copy to Disk ---
    for (int i = 0; i < process_size; i++) {
        strcpy(disk[disk_start + i].name, main_memory[start + i].name);
        strcpy(disk[disk_start + i].value, main_memory[start + i].value);
    }

    // --- STEP 2: Wipe from Main Memory ---
    for (int i = start; i <= end; i++) {
        main_memory[i].name[0] = '\0';
        main_memory[i].value[0] = '\0';
    }

    // --- STEP 3: Update Logical State ---
    // Protect the PC by converting it to a relative offset before we wipe the start address!
    processes[v_idx].program_counter = processes[v_idx].program_counter - processes[v_idx].mem_start;
    
    processes[v_idx].mem_start = -1;
    processes[v_idx].mem_end = -1;

    printf("[Memory Manager]: Process P%d safely copied to Hard Disk [Index %d].\n", victim_id, disk_start);
    
    // --> THE FIX: Return the index so disk_locations array can save it! <--
    return disk_start; 
}

// Returns true if the process is in RAM, false if it is on the Hard Disk
bool is_process_in_memory(int processID, PCB processes[]) {
    int p_idx = processID - 1; // Convert ID (1, 2, 3) to array index (0, 1, 2)
    
    if (processes[p_idx].mem_start != -1) {
        return true;  // It is safely in the 40-word main_memory
    } else {
        return false; // It has been swapped to the HDD
    }
}

// ---> ADDED SHIFTING LOGIC HERE <---
// Shifts all processes in RAM to the top (index 0) to remove empty gaps
void compact_memory(PCB processes[], int total_processes, MemoryWord memory[]) {
    int next_free_index = 0; 

    for (int step = 0; step < total_processes; step++) {
        int closest_p_idx = -1;
        int min_start = 999; 

        // Find the process closest to the top
        for (int i = 0; i < total_processes; i++) {
            if (processes[i].mem_start != -1 && processes[i].mem_start >= next_free_index) {
                if (processes[i].mem_start < min_start) {
                    min_start = processes[i].mem_start;
                    closest_p_idx = i;
                }
            }
        }

        if (closest_p_idx == -1) break; 

        int start = processes[closest_p_idx].mem_start;
        int end = processes[closest_p_idx].mem_end;
        int size = end - start + 1;

        if (start == next_free_index) {
            next_free_index += size;
            continue;
        }

        printf("[Memory Manager]: Compacting RAM -> Shifting P%d up from index %d to %d.\n", 
               processes[closest_p_idx].processID, start, next_free_index);

        // Shift Data Up
        for (int i = 0; i < size; i++) {
            strcpy(memory[next_free_index + i].name, memory[start + i].name);
            strcpy(memory[next_free_index + i].value, memory[start + i].value);
        }

        // Wipe Tail
        for (int i = next_free_index + size; i <= end; i++) {
            memory[i].name[0] = '\0';
            memory[i].value[0] = '\0';
        }

        // Update PCB and PC
        int shift_distance = start - next_free_index;
        processes[closest_p_idx].mem_start = next_free_index;
        processes[closest_p_idx].mem_end = next_free_index + size - 1;
        processes[closest_p_idx].program_counter -= shift_distance;

        next_free_index += size;
    }
}

void ensure_process_in_memory(int processID, PCB processes[], MemoryWord main_memory[], MemoryWord hard_disk[], int disk_locations[], int service_times[], int current_time, int arrival_times[]) {
    int p_idx = processID - 1;

    // 1. FAST CHECK: Is it already in RAM?
    if (processes[p_idx].mem_start != -1) {
        return; // Yes! Do nothing, let the CPU run.
    }

    printf("[Memory Manager]: P%d is parked on the HDD. Initiating Swap-In...\n", processID);

    // 2. Calculate the exact size needed (7 words for PCB/Vars + Lines of Code)
    int space_needed = 7 + service_times[p_idx];

    // 3. Find a spot in RAM
    int start_idx = find_first_empty_index(main_memory);

    // 4. MEMORY FULL LOOP: If RAM is full, we must kick someone else out
    while (start_idx == -1 || (start_idx + space_needed) > 40) {
        printf("[Memory Manager]: RAM full during Swap-In. Evicting a victim...\n");
        
        // Pick a victim and swap them out
        int victim_id = get_victim_process(processes, 3, current_time, arrival_times, service_times);
        disk_locations[victim_id - 1] = swap_to_disk(victim_id, processes, main_memory, hard_disk);
        
        // Shift remaining RAM up to close the gap
        compact_memory(processes, 3, main_memory); 
        
        // Recalculate space after compaction
        start_idx = find_first_empty_index(main_memory);
    }

    // 5. RESTORE: Copy data from Hard Disk back to Main Memory
    int disk_start = disk_locations[p_idx];
    for (int i = 0; i < space_needed; i++) {
        strcpy(main_memory[start_idx + i].name, hard_disk[disk_start + i].name);
        strcpy(main_memory[start_idx + i].value, hard_disk[disk_start + i].value);

        // Clean up the hard disk block so it can be reused later
        hard_disk[disk_start + i].name[0] = '\0';
        hard_disk[disk_start + i].value[0] = '\0';
    }

    // 6. UPDATE PCB: Give the process its new physical addresses
    processes[p_idx].mem_start = start_idx;
    processes[p_idx].mem_end = start_idx + space_needed - 1;

    // Restore the PC from a Relative Offset back to an Absolute Index
    processes[p_idx].program_counter = start_idx + processes[p_idx].program_counter;

    // Erase its disk location tracker
    disk_locations[p_idx] = -1;

    printf("[Memory Manager]: P%d successfully swapped back into RAM [Indices %d to %d].\n", 
           processID, start_idx, processes[p_idx].mem_end);
}

// Helper function to display the current state of the Hard Disk
void print_disk_state(MemoryWord disk[]) {
    printf("\n--- Hard Disk Content ---\n");
    int free_space = 0;
    
    for (int i = 0; i < 100; i++) {
        // If the slot is occupied, print it cleanly
        if (disk[i].name[0] != '\0') {
            // %-15s adds spacing so the colons line up perfectly
            printf("[%02d] %-15s : %s\n", i, disk[i].name, disk[i].value);
        } else {
            free_space++;
        }
    }
    printf("-> [Free Space: %d/100 words]\n", free_space);
    printf("---------------------------\n");
}