#include "os_sim.h"

// Initialize the queue to an empty state
void initQueue(Queue *q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

// Check if the queue has reached MAX_PROCESSES
bool isFull(Queue *q) {
    return q->count == MAX_PROCESSES;
}

// Check if there are no processes in the queue
bool isEmpty(Queue *q) {
    return q->count == 0;
}

// Add a process ID to the back of the queue (FIFO)
void enqueue(Queue *q, int processID) {
    if (isFull(q)) {
        printf("Error: Queue Overflow! Cannot add P%d\n", processID);
        return;
    }
    
    // Circular increment: moves rear to the next index, or back to 0 if at the end
    q->rear = (q->rear + 1) % MAX_PROCESSES;
    q->process_ids[q->rear] = processID;
    q->count++;
}

// Remove and return the process ID from the front of the queue
int dequeue(Queue *q) {
    if (isEmpty(q)) {
        return -1; // Indicates queue was empty
    }
    
    int id = q->process_ids[q->front];
    
    // Circular increment: moves front to the next index, or back to 0 if at the end
    q->front = (q->front + 1) % MAX_PROCESSES;
    q->count--;
    
    return id;
}

// Helper function to display queue contents for your system trace
void printQueue(Queue *q, char *queue_name) {
    printf("%s: [", queue_name);
    for (int i = 0; i < q->count; i++) {
        // Calculate the correct index in the circular array
        int index = (q->front + i) % MAX_PROCESSES;
        printf("P%d%s", q->process_ids[index], (i == q->count - 1) ? "" : ", ");
    }
    printf("]\n");
}

// Removes a specific process ID from anywhere in the queue
void remove_from_queue(Queue *q, int processID) {
    // Safety check
    if (isEmpty(q)) {
        return; 
    }

    int target_index = -1;
    
    // 1. Find the exact index of the process ID in the circular array
    for (int i = 0; i < q->count; i++) {
        int idx = (q->front + i) % MAX_PROCESSES;
        if (q->process_ids[idx] == processID) {
            target_index = idx;
            break; // Found it, stop searching
        }
    }

    // If the ID somehow wasn't found, abort
    if (target_index == -1) {
        printf("Error: Process P%d not found in queue!\n", processID);
        return;
    }

    // 2. Shift all elements that come *after* the target forward by one space
    int curr = target_index;
    while (curr != q->rear) {
        int next = (curr + 1) % MAX_PROCESSES;
        q->process_ids[curr] = q->process_ids[next];
        curr = next;
    }

    // 3. Update the rear pointer (step it back by one safely)
    // We add MAX_PROCESSES before modulo to prevent negative numbers in C
    q->rear = (q->rear - 1 + MAX_PROCESSES) % MAX_PROCESSES;
    
    // 4. Decrement the total count
    q->count--;

    // 5. Clean up if the queue is now completely empty
    if (q->count == 0) {
        q->front = 0;
        q->rear = -1;
    }
}