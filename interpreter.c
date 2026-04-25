#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "os_sim.h"

int get_program_memory_size(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Could not open %s\n", filename);
        return -1; // Return -1 to indicate the file doesn't exist
    }

    int line_count = 0;
    char buffer[100]; // A temporary bucket to hold each line as we read it

    // Loop through the file and count every line
    while (fgets(buffer, sizeof(buffer), file)) {
        line_count++;
    }

    fclose(file);

    // Apply the OS architecture formula: 4 (PCB) + 3 (Vars) + lines of code
    int total_memory_words = 7 + line_count;

    return total_memory_words;
}

PCB load_program_to_memory(MemoryWord memory[], int start_index, const char* filename, int process_id) {

    PCB new_pcb; // Create the struct here


    // 1. Get total required space using your helper function
    int required_words = get_program_memory_size(filename);
    if (required_words == -1) 
        {   PCB error_pcb;
            error_pcb.processID = -1; // Marks it as a failed load
            return error_pcb;
        }; // Stop if the file doesn't exist

    // Calculate the exact ending index
    int mem_end = start_index + required_words - 1;

    // Check if it fits in your 40-word limit
    if (mem_end >= 40) {
        printf("Error: Not enough space in main memory for Process %d!\n", process_id);
        PCB error_pcb;
error_pcb.processID = -1; // Marks it as a failed load
return error_pcb;;
    }

    // Open the file to actually extract the text
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: Could not open %s\n", filename);
        PCB error_pcb;
error_pcb.processID = -1; // Marks it as a failed load
return error_pcb;;
    }

    // --- 2. Allocate the PCB (4 Words) ---
    
    // Word 0: Process ID
    sprintf(memory[start_index].name, "P%d_ID", process_id);
    sprintf(memory[start_index].value, "%d", process_id);

    // Word 1: Process State
    sprintf(memory[start_index + 1].name, "P%d_State", process_id);
    strcpy(memory[start_index + 1].value, "READY");

    // Word 2: Program Counter (Instructions start exactly 7 slots down)
    sprintf(memory[start_index + 2].name, "P%d_PC", process_id);
    sprintf(memory[start_index + 2].value, "%d", start_index + 7);

    // Word 3: Memory Boundaries
    sprintf(memory[start_index + 3].name, "P%d_Bounds", process_id);
    sprintf(memory[start_index + 3].value, "%d-%d", start_index, mem_end);


    // --- 3. Allocate the Variables (3 Words) ---
    // This loop fills start_index + 4, 5, and 6
    for (int i = 0; i < 3; i++) {
        sprintf(memory[start_index + 4 + i].name, "P%d_Var%d", process_id, i + 1);
        strcpy(memory[start_index + 4 + i].value, "NULL");
    }


    // --- 4. Allocate Unparsed Instructions (N Words) ---
    char line[30];
    int inst_count = 1;
    int current_memory_slot = start_index + 7; // Start right after the variables

    while (fgets(line, sizeof(line), file)) {
        // Strip the hidden newline character
        line[strcspn(line, "\n")] = 0;

        sprintf(memory[current_memory_slot].name, "P%d_Inst%d", process_id, inst_count);
        strcpy(memory[current_memory_slot].value, line);

        current_memory_slot++;
        inst_count++;
    }

    fclose(file);
    printf("--> Successfully loaded Process %d into memory bounds [%d-%d]\n", process_id, start_index, mem_end);

    // 3. Logical Initialization: Fill the struct fields
    new_pcb.processID = process_id;
    new_pcb.state = STATE_READY;
    new_pcb.program_counter = start_index + 7; // Points to first instruction
    new_pcb.mem_start = start_index;
    new_pcb.mem_end = mem_end;

    printf("--> Process %d created and loaded into RAM.\n", process_id);

    return new_pcb; // Return the whole struct to main

}

void execute_instruction(char* instr_raw, int processID, MemoryWord memory[], PCB processes[]) {
    char instr[50];
    strcpy(instr, instr_raw); // Copy so we don't destroy the original string in memory
    
    char* command = strtok(instr, " ");
    char* arg1 = strtok(NULL, " ");
    char* arg2 = strtok(NULL, " ");
    char* arg3 = strtok(NULL, " "); 

    // --- CASE: print ---
    if (strcmp(command, "print") == 0) {
        char* val = get_variable_value(processID, arg1, memory, processes);
        printf("[Process %d Output]: %s\n", processID, val);
    }

    // --- CASE: assign ---
    else if (strcmp(command, "assign") == 0) {
        if (strcmp(arg2, "input") == 0) {
            char userInput[30];
            printf("Please enter a value: ");
            scanf("%s", userInput);
            set_variable_value(processID, arg1, userInput, memory, processes);
        } else if (strcmp(arg2, "readFile") == 0) {
            // Nested command: assign x readFile y
            char* filename = get_variable_value(processID, arg3, memory, processes);
            FILE* file = fopen(filename, "r");
            if (file) {
                char fileData[30];
                fgets(fileData, 30, file);
                fileData[strcspn(fileData, "\n")] = 0; // Clean the hidden newline character
                set_variable_value(processID, arg1, fileData, memory, processes);
                fclose(file);
            } else {
                printf("Error: Could not open file %s\n", filename);
            }
        } else {
            // Standard assignment: assign x y 
            char* val = get_variable_value(processID, arg2, memory, processes);
            set_variable_value(processID, arg1, val, memory, processes);
        }
    }

    // --- CASE: writeFile ---
    else if (strcmp(command, "writeFile") == 0) {
        char* filename = get_variable_value(processID, arg1, memory, processes);
        char* data = get_variable_value(processID, arg2, memory, processes);
        FILE* file = fopen(filename, "w");
        if (file) {
            fprintf(file, "%s", data);
            fclose(file);
        } else {
            printf("Error: Could not write to file %s\n", filename);
        }
    }

    // --- CASE: readFile ---
    else if (strcmp(command, "readFile") == 0) {
        char* filename = get_variable_value(processID, arg1, memory, processes);
        FILE* file = fopen(filename, "r");
        if (file) {
            char line[30];
            while (fgets(line, 30, file)) printf("%s", line);
            printf("\n");
            fclose(file);
        } else {
            printf("Error: Could not read file %s\n", filename);
        }
    }

    // --- CASE: printFromTo ---
    else if (strcmp(command, "printFromTo") == 0) {
        int startNum = atoi(get_variable_value(processID, arg1, memory, processes));
        int endNum = atoi(get_variable_value(processID, arg2, memory, processes));
        for (int i = startNum; i <= endNum; i++) {
            printf("%d ", i);
        }
        printf("\n");
    }

    // --- CASE: semWait ---
    else if (strcmp(command, "semWait") == 0) {
        // 1. Identify which semaphore we are targeting
        BinarySemaphore* target_sem = NULL;
        if (strcmp(arg1, "userInput") == 0) target_sem = &userInput;
        else if (strcmp(arg1, "userOutput") == 0) target_sem = &userOutput;
        else if (strcmp(arg1, "file") == 0) target_sem = &fileAccess;

        if (target_sem != NULL) {
            // 2. Check if the resource is free
            if (target_sem->value == 1) {
                // Free! Take the key and mark ownership
                target_sem->value = 0;
                target_sem->owner_process_id = processID;
            } else {
                // Taken! Block the current process
                processes[processID - 1].state = STATE_BLOCKED;
                enqueue(&(target_sem->blocked_queue), processID);
            }
        }
    }

    // --- CASE: semSignal ---
    else if (strcmp(command, "semSignal") == 0) {
        // 1. Identify which semaphore we are targeting
        BinarySemaphore* target_sem = NULL;
        if (strcmp(arg1, "userInput") == 0) target_sem = &userInput;
        else if (strcmp(arg1, "userOutput") == 0) target_sem = &userOutput;
        else if (strcmp(arg1, "file") == 0) target_sem = &fileAccess;

        if (target_sem != NULL) {
            // 2. Security Check: Only the owner can signal a release
            if (target_sem->owner_process_id == processID) {
                // 3. Check if anyone is waiting in line
                if (!isEmpty(&(target_sem->blocked_queue))) {
                    // Hand the key directly to the next process in line
                    int next_process_id = dequeue(&(target_sem->blocked_queue));
                    target_sem->owner_process_id = next_process_id;
                    
                    // Wake them up and put them back in the ready queue
                    processes[next_process_id - 1].state = STATE_READY;
                    enqueue(&ready_queue, next_process_id);
                } else {
                    // Nobody is waiting, put the key back on the hook
                    target_sem->value = 1;
                    target_sem->owner_process_id = -1;
                }
            } else {
                printf("OS Error: Process P%d tried to signal %s, but doesn't own it!\n", processID, arg1);
            }
        }
    }
}