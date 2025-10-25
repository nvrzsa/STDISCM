#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <queue>
#include <condition_variable>
#include <atomic>

// --- Helper functions (same as variant 1) ---

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto t_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    #ifdef _WIN32
        localtime_s(&tm_buf, &t_c);
    #else
        localtime_r(&t_c, &tm_buf);
    #endif
    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%H:%M:%S") 
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string getThreadId() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

bool isPrime(long long n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (long long i = 5; i * i <= n; i = i + 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return false;
    }
    return true;
}

bool loadConfig(std::map<std::string, long long>& config) {
    std::ifstream configFile("config3.ini");
    if (!configFile.is_open()) {
        std::cerr << "Error: Could not open config.ini" << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(configFile, line)) {
        if (line.empty() || line[0] == ';') continue;
        std::stringstream ss(line);
        std::string key, valueStr;
        if (std::getline(ss, key, '=') && std::getline(ss, valueStr)) {
            try {
                config[key] = std::stoll(valueStr);
            } catch (const std::exception& e) {
                std::cerr << "Error parsing config line: " << line << " - " << e.what() << std::endl;
            }
        }
    }
    configFile.close();
    return true;
}

// --- Variant 3 specific stuff ---

// shared stuff for the producer-consumer pattern
std::queue<long long> task_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;
std::atomic<bool> all_tasks_added(false);

// mutex to keep output from getting messed up
std::mutex cout_mutex;

// this function runs in each thread - grabs numbers from the queue and prints primes right away
void findPrimes_Number_Immediate() {
    while (true) {
        long long num_to_check;

        // grab a number from the queue (thread-safe)
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [] {
                return !task_queue.empty() || all_tasks_added;
            });

            if (task_queue.empty() && all_tasks_added) {
                return; // no more work to do
            }

            num_to_check = task_queue.front();
            task_queue.pop();
        } // unlock the queue

        // do the actual work (check if prime)
        if (isPrime(num_to_check)) {
            // lock so threads don't print at the same time
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[Time: " << getCurrentTimestamp() 
                      << "] [Thread: " << getThreadId() 
                      << "] Found prime: " << num_to_check << std::endl;
        }
    }
}

int main() {
    std::cout << "--- Variant 3: Number Division / Immediate Print ---" << std::endl;
    
    // start the timer
    auto start_time = std::chrono::high_resolution_clock::now();
    std::cout << "Run START: " << getCurrentTimestamp() << std::endl;

    // load settings from config file
    std::map<std::string, long long> config;
    if (!loadConfig(config) || config.find("THREAD_COUNT") == config.end() || config.find("MAX_NUMBER") == config.end()) {
        std::cerr << "Config file missing or incomplete. Exiting." << std::endl;
        return 1;
    }

    long long thread_count = config["THREAD_COUNT"];
    long long max_number = config["MAX_NUMBER"];
    all_tasks_added = false;

    std::cout << "Config: Using " << thread_count << " threads to search up to " << max_number << "." << std::endl;

    // create worker threads
    std::vector<std::thread> threads;

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(findPrimes_Number_Immediate);
    }

    // main thread puts numbers in the queue
    std::cout << "Main thread starting to produce tasks..." << std::endl;
    for (long long num = 2; num <= max_number; ++num) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            task_queue.push(num);
        }
        queue_cv.notify_one();
    }

    // tell threads we're done adding numbers
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        all_tasks_added = true;
    }
    std::cout << "Main thread finished producing tasks." << std::endl;
    queue_cv.notify_all();

    // wait for all worker threads to finish
    for (std::thread& t : threads) {
        t.join();
    }

    // stop timer and see how long it took
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "All threads finished." << std::endl;
    std::cout << "Run END: " << getCurrentTimestamp() << std::endl;
    std::cout << "Total execution time: " << duration.count() << " ms" << std::endl;
    std::cout << "Performance: " << max_number << " numbers processed in " << duration.count() << " ms" << std::endl;
    return 0;
}
