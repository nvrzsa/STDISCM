#include <iostream>
#include <thread>         // For std::thread
#include <mutex>          // For std::mutex, std::lock_guard
#include <condition_variable>
#include <vector>         // For storing instance status and stats
#include <string>         // For instance status strings
#include <chrono>         // For std::this_thread::sleep_for
#include <random>         // For dungeon time simulation
#include <algorithm>      // For std::min
#include <iomanip>        // For std::setw (output formatting)

// Globals
int g_max_instances;                       // max concurrent instances

// Simple counting semaphore (portable)
class CountingSemaphore {
public:
    explicit CountingSemaphore(int initial = 0) : count(initial) {}
    void acquire() {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, [&]() { return count > 0; });
        --count;
    }
    void release() {
        std::lock_guard<std::mutex> lock(m);
        ++count;
        cv.notify_one();
    }
private:
    std::mutex m;
    std::condition_variable cv;
    int count;
};

CountingSemaphore* g_instance_slots; // controls available slots
std::mutex g_data_mutex;                   // protects the data vectors
std::vector<std::string> g_instance_status; // "empty" or "active"
std::vector<int> g_parties_served;         // count per instance
std::vector<double> g_time_served;         // seconds per instance
std::mutex g_cout_mutex;                   // protect std::cout

// Player pool (shared)
int g_tanks = 0;
int g_healers = 0;
int g_dps = 0;
bool g_arrival_done = false;

// Helper Functions

// Print current status of instances.
// Call while holding g_cout_mutex.
void print_status() {
    std::cout << "Instance Status: |";
    std::lock_guard<std::mutex> data_lock(g_data_mutex);
    for (int i = 0; i < g_max_instances; ++i) {
        std::string status = g_instance_status[i];
        if (status == "empty") {
            std::cout << " " << std::setw(8) << "empty" << " |";
        } else {
            std::cout << " " << std::setw(8) << status << " |";
        }
    }
    std::cout << "\n--------------------------------------------------------\n";
}

// Each party runs this function.
// Steps: wait for a slot, claim an instance, run, update stats, release.
void run_dungeon(int party_id, int min_time, int max_time) {
    // RNG for this thread
    std::random_device rd;
    std::mt19937 generator(rd() + party_id);
    std::uniform_int_distribution<> dis(min_time, max_time);
    int duration = dis(generator);

    // wait for an available slot
    g_instance_slots->acquire();

    // find and mark a free instance
    int instance_id = -1;
    {
        std::lock_guard<std::mutex> data_lock(g_data_mutex);
        for (int i = 0; i < g_max_instances; ++i) {
            if (g_instance_status[i] == "empty") {
                instance_id = i;
                g_instance_status[i] = "active";
                break;
            }
        }
    }

    // print entry
    {
        std::lock_guard<std::mutex> cout_lock(g_cout_mutex);
        std::cout << "[Party " << party_id << "] entered Instance " << instance_id
                  << ". (Running for " << duration << "s)\n";
        print_status();
    }

    // simulate run
    std::this_thread::sleep_for(std::chrono::seconds(duration));

    // finish and update stats
    if (instance_id >= 0) {
        std::lock_guard<std::mutex> data_lock(g_data_mutex);
        g_instance_status[instance_id] = "empty";
        g_parties_served[instance_id]++;
        g_time_served[instance_id] += duration;
    } else {
        std::lock_guard<std::mutex> cout_lock(g_cout_mutex);
        std::cout << "[Party " << party_id << "] ERROR: no instance to release.\n";
    }

    // print finish
    {
        std::lock_guard<std::mutex> cout_lock(g_cout_mutex);
        std::cout << "[Party " << party_id << "] finished Instance " << instance_id << ".\n";
        print_status();
    }

    // release the slot
    g_instance_slots->release();
}

// Try to form a party from the shared player pool.
// Returns true and consumes players if a party can be formed.
bool try_form_party() {
    std::lock_guard<std::mutex> lock(g_data_mutex);
    if (g_tanks >= 1 && g_healers >= 1 && g_dps >= 3) {
        g_tanks -= 1;
        g_healers -= 1;
        g_dps -= 3;
        return true;
    }
    return false;
}

// Arrival thread: periodically add random new players to the pool.
void arrival_thread_func(int cycles, int min_sleep_s, int max_sleep_s) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sleep_dis(min_sleep_s, max_sleep_s);
    std::uniform_int_distribution<> add_tanks(0, 2);
    std::uniform_int_distribution<> add_heals(0, 2);
    std::uniform_int_distribution<> add_dps(0, 6);

    for (int i = 0; i < cycles; ++i) {
        int s = sleep_dis(gen);
        std::this_thread::sleep_for(std::chrono::seconds(s));

        int at = add_tanks(gen);
        int ah = add_heals(gen);
        int ad = add_dps(gen);

        {
            std::lock_guard<std::mutex> lock(g_data_mutex);
            g_tanks += at;
            g_healers += ah;
            g_dps += ad;
        }

        {
            std::lock_guard<std::mutex> cout_lock(g_cout_mutex);
            std::cout << "[Arrival] added " << at << "T " << ah << "H " << ad << "D\n";
        }
    }

    // signal arrival finished
    {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        g_arrival_done = true;
    }
}

// Main Program Execution

int main() {
    // 1. Get user input
    int n, tanks, healers, dps, t1, t2;

    std::cout << "Enter max concurrent instances (n): ";
    std::cin >> n;
    std::cout << "Enter number of Tanks in queue (t): ";
    std::cin >> tanks;
    std::cout << "Enter number of Healers in queue (h): ";
    std::cin >> healers;
    std::cout << "Enter number of DPS in queue (d): ";
    std::cin >> dps;
    std::cout << "Enter min dungeon time (t1): ";
    std::cin >> t1;
    std::cout << "Enter max dungeon time (t2) (<=15 recommended): ";
    std::cin >> t2;
    std::cout << "\n";

    // Input validation
    if (n <= 0) {
        std::cout << "Error: max concurrent instances 'n' must be >= 1. Exiting.\n";
        return 1;
    }
    if (t1 < 0) t1 = 0;
    if (t2 < 0) t2 = 0;
    if (t1 > t2) {
        std::cout << "Warning: t1 > t2. Swapping the values so t1 <= t2.\n";
        std::swap(t1, t2);
    }
    if (t2 > 15) {
        std::cout << "Warning: t2 > 15 (test limit). Clamping t2 to 15.\n";
        t2 = 15;
    }
    if (tanks < 0 || healers < 0 || dps < 0) {
        std::cout << "Error: player counts must be non-negative. Exiting.\n";
        return 1;
    }

    // 2. Prepare shared resources
    g_max_instances = n;

    // semaphore
    g_instance_slots = new CountingSemaphore(n);

    // instance arrays
    g_instance_status.resize(n, "empty");
    g_parties_served.resize(n, 0);
    g_time_served.resize(n, 0.0);

    // initialize player pool from initial input
    {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        g_tanks = tanks;
        g_healers = healers;
        g_dps = dps;
        g_arrival_done = false;
    }

    std::cout << "=== LFG Queue Starting ===\n";
    std::cout << "Max concurrent instances: " << n << "\n";
    std::cout << "Player pool initial: " << tanks << "T, " << healers << "H, " << dps << "D\n";
    std::cout << "(A background arrival thread will add players randomly.)\n";
    std::cout << "============================\n\n";

    // container for party threads
    std::vector<std::thread> party_threads;

    // start arrival thread (bonus): run a fixed number of cycles
    std::thread arrival_thread(arrival_thread_func, 10, 1, 3); // 10 cycles, 1-3s sleeps

    // continuously try to form parties while arrivals may still come
    int next_party_id = 0;
    while (true) {
        // form as many parties as possible right now
        bool formed_any = false;
        while (try_form_party()) {
            ++next_party_id;
            party_threads.emplace_back(run_dungeon, next_party_id, t1, t2);
            formed_any = true;
        }

        // if arrival finished and we couldn't form any new party, we're done
        {
            std::lock_guard<std::mutex> lock(g_data_mutex);
            if (g_arrival_done && !formed_any) break;
        }

        // wait a short while before retrying
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // wait for arrival thread and all party threads
    arrival_thread.join();
    for (auto& th : party_threads) th.join();

    // 6. Print Final Summary
    std::cout << "\n=== QUEUE FINISHED: FINAL SUMMARY ===\n";
    double total_time_all = 0;
    int total_parties_all = 0;

    for (int i = 0; i < g_max_instances; ++i) {
        std::cout << "Instance " << i << ":\n";
        std::cout << "  - Parties Served:   " << g_parties_served[i] << "\n";
        std::cout << "  - Total Time Served: " << g_time_served[i] << "s\n";
        total_parties_all += g_parties_served[i];
        total_time_all += g_time_served[i];
    }
    std::cout << "-------------------------------------\n";
    std::cout << "Overall:\n";
    std::cout << "  - Total Parties Served: " << total_parties_all << "\n";
    std::cout << "  - Combined Time Served: " << total_time_all << "s\n";
    std::cout << "=====================================\n";

    // 7. Cleanup
    delete g_instance_slots;

    return 0;
}