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


// 2. THE EXTERN PROMISES (Tells all files these exist somewhere else)
extern BinarySemaphore userInput;
extern BinarySemaphore userOutput;
extern BinarySemaphore fileAccess;
extern Queue ready_queue;
extern Queue general_blocked_queue;
extern MemoryWord main_memory[40];

// --- FUNCTION PROTOTYPES ---
int get_program_memory_size(const char* filename);
PCB load_program_to_memory(MemoryWord memory[], int start_index, const char* filename, int process_id);
char* get_variable_value(int processID, char* varName, MemoryWord memory[], PCB processes[]);
void set_variable_value(int processID, char* varName, char* newValue, MemoryWord memory[], PCB processes[]);

void initQueue(Queue *q);
bool isFull(Queue *q);
bool isEmpty(Queue *q);
void enqueue(Queue *q, int processID);
int dequeue(Queue *q);
void printQueue(Queue *q, char *queue_name);
void init_memory(MemoryWord memory[]);
int find_first_empty_index(MemoryWord memory[]);
void remove_from_queue(Queue *q, int processID);
// Inside os_sim.h, under your function prototypes section:
int get_victim_process(PCB processes[], int total_processes, int current_time, int arrival_times[], int service_times[]);
int find_first_empty_disk_index(MemoryWord disk[], int disk_size);
int swap_to_disk(int victim_id, PCB processes[], MemoryWord main_memory[], MemoryWord disk[]);
bool is_process_in_memory(int processID, PCB processes[]);
void compact_memory(PCB processes[], int total_processes, MemoryWord main_memory[]);
void ensure_process_in_memory(int processID, PCB processes[], MemoryWord main_memory[], MemoryWord hard_disk[], int disk_locations[], int service_times[], int current_time, int arrival_times[]);
void execute_instruction(char* instr_raw, int processID, MemoryWord memory[], PCB processes[]);
#endif