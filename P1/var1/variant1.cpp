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

// --- Helper functions we'll use ---

// gets current time in a nice format
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto t_c = std::chrono::system_clock::to_time_t(now);
    
    std::tm tm_buf;
    #ifdef _WIN32
        localtime_s(&tm_buf, &t_c);
    #else
        localtime_r(&t_c, &tm_buf); // POSIX
    #endif

    std::stringstream ss;
    ss << std::put_time(&tm_buf, "%H:%M:%S") 
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// gets the thread ID as a string (for printing)
std::string getThreadId() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

// checks if a number is prime (basic algorithm)
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

// reads the config file and puts values in a map
bool loadConfig(std::map<std::string, long long>& config) {
    std::ifstream configFile("config1.ini");
    if (!configFile.is_open()) {
        std::cerr << "Error: Could not open config.ini" << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(configFile, line)) {
        // Ignore comments and empty lines
        if (line.empty() || line[0] == ';') {
            continue;
        }
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

// --- Variant 1 specific stuff ---

// mutex to make sure threads don't mess up the output
std::mutex cout_mutex;

// this function runs in each thread - finds primes in a range and prints them right away
void findPrimes_Range_Immediate(long long start, long long end) {
    for (long long num = start; num <= end; ++num) {
        if (isPrime(num)) {
            // lock so threads don't print at the same time
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[Time: " << getCurrentTimestamp() 
                      << "] [Thread: " << getThreadId() 
                      << "] Found prime: " << num << std::endl;
        }
    }
}

int main() {
    std::cout << "--- Variant 1: Range Division / Immediate Print ---" << std::endl;
    
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

    std::cout << "Config: Using " << thread_count << " threads to search up to " << max_number << "." << std::endl;

    // create threads and divide the work
    std::vector<std::thread> threads;
    long long range_size = max_number / thread_count;

    for (int i = 0; i < thread_count; ++i) {
        long long start = (i * range_size) + 1;
        // last thread gets any leftover numbers
        long long end = (i == thread_count - 1) ? max_number : (i + 1) * range_size;
        
        // skip if we don't have enough numbers
        if (start > max_number) break; 
        if (i == 0 && start == 1) start = 2; // 1 isn't prime, so skip it

        threads.emplace_back(findPrimes_Range_Immediate, start, end);
    }

    // wait for all threads to finish
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
