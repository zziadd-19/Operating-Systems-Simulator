#ifndef OS_SIM_H
#define OS_SIM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// 1. Define Process States
typedef enum {
    STATE_NEW,
    STATE_READY,
    STATE_RUNNING,
    STATE_BLOCKED,
    STATE_FINISHED
} ProcessState;

// 2. Define the Process Control Block (PCB)
typedef struct {
    int processID;
    ProcessState state;
    int program_counter;  // Tracks which line of code to execute next
    int mem_start;        // Index in main_memory where this process starts
    int mem_end;          // Index in main_memory where this process ends
} PCB;

// 3. Define the Memory Architecture
// The memory array will be an array of 40 of these
typedef struct {
    char name[20];
    char value[30]; 
} MemoryWord;

// 4. Define a basic Queue structure for the Semaphores and Ready Queue
#define MAX_PROCESSES 10
typedef struct {
    int process_ids[MAX_PROCESSES];
    int front;
    int rear;
    int count;
} Queue;

// 5. Define the Binary Semaphore
typedef struct {
    int value;               // 1 = Available, 0 = In Use
    int owner_process_id;    // ID of the process currently holding the lock
    Queue blocked_queue;     // Processes waiting on this specific resource
} BinarySemaphore;

#endif