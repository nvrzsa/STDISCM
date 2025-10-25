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
    std::ifstream configFile("config2.ini");
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

// --- Variant 2 specific stuff ---

// this function runs in each thread - finds primes in a range and saves them (doesn't print yet)
void findPrimes_Range_Batched(long long start, long long end, std::vector<long long>& thread_results) {
    for (long long num = start; num <= end; ++num) {
        if (isPrime(num)) {
            // no mutex needed - each thread has its own vector
            thread_results.push_back(num);
        }
    }
}

int main() {
    std::cout << "--- Variant 2: Range Division / Batched Print ---" << std::endl;
    
    // Start timing
    auto start_time = std::chrono::high_resolution_clock::now();
    std::cout << "Run START: " << getCurrentTimestamp() << std::endl;

    std::map<std::string, long long> config;
    if (!loadConfig(config) || config.find("THREAD_COUNT") == config.end() || config.find("MAX_NUMBER") == config.end()) {
        std::cerr << "Config file missing or incomplete. Exiting." << std::endl;
        return 1;
    }

    long long thread_count = config["THREAD_COUNT"];
    long long max_number = config["MAX_NUMBER"];

    std::cout << "Config: Using " << thread_count << " threads to search up to " << max_number << "." << std::endl;

    std::vector<std::thread> threads;
    // Create a vector of vectors, one for each thread to store its results
    std::vector<std::vector<long long>> all_results(thread_count);

    long long range_size = max_number / thread_count;

    for (int i = 0; i < thread_count; ++i) {
        long long start = (i * range_size) + 1;
        long long end = (i == thread_count - 1) ? max_number : (i + 1) * range_size;
        
        if (start > max_number) break;
        if (i == 0 && start == 1) start = 2;

        // Pass a reference to the thread's specific results vector
        threads.emplace_back(findPrimes_Range_Batched, start, end, std::ref(all_results[i]));
    }

    // Wait for all threads to complete
    for (std::thread& t : threads) {
        t.join();
    }

    // End timing and calculate duration
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "All threads finished. Consolidating and printing results..." << std::endl;

    // Now, print all the results from the main thread
    long long total_primes = 0;
    for (int i = 0; i < all_results.size(); ++i) {
        // You could sort each vector here if you wanted them in order
        // std::sort(all_results[i].begin(), all_results[i].end());
        
        std::cout << "--- Results from Thread " << i << " (" << all_results[i].size() << " primes) ---" << std::endl;
        for (long long prime : all_results[i]) {
            std::cout << prime << " ";
            total_primes++;
        }
        std::cout << std::endl;
    }

    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Total primes found: " << total_primes << std::endl;
    std::cout << "Run END: " << getCurrentTimestamp() << std::endl;
    std::cout << "Total execution time: " << duration.count() << " ms" << std::endl;
    std::cout << "Performance: " << max_number << " numbers processed in " << duration.count() << " ms" << std::endl;
    return 0;
}
