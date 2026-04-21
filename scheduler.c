#include "os_sim.h"

// Returns the Process ID of the best candidate to swap to disk
int get_victim_process(PCB processes[], int total_processes, int current_time, int arrival_times[], int service_times[]) {
    int best_victim_id = -1;

    // --- PRIORITY 1: The "Dead" (Finished Processes) ---
    for (int i = 0; i < total_processes; i++) {
        if (processes[i].state == STATE_FINISHED && processes[i].mem_start != -1) {
            return processes[i].processID; 
        }
    }

    // --- PRIORITY 2: The "Sleeping" (Blocked Processes) ---
    for (int i = 0; i < total_processes; i++) {
        if (processes[i].state == STATE_BLOCKED && processes[i].mem_start != -1) {
            return processes[i].processID; 
        }
    }

    // --- PRIORITY 3: The "Low Priority" (Lowest HRRN Ready Process) ---
    double lowest_ratio = 999999.0; 
    
    for (int i = 0; i < total_processes; i++) {
        if (processes[i].state == STATE_READY && processes[i].mem_start != -1) {
            
            double w = (double)(current_time - arrival_times[i]);
            double s = (double)service_times[i];
            double ratio = (w + s) / s;

            if (ratio < lowest_ratio) {
                lowest_ratio = ratio;
                best_victim_id = processes[i].processID;
            }
        }
    }

    return best_victim_id; 
} // <-- This is the brace that was missing!