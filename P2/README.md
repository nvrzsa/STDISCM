compile: g++ "main.cpp" -std=c++20 -O2 -o "main.exe"

Test #1 - Even-steven case:
(echo 4&echo 10&echo 10&echo 30&echo 1&echo 4) | main.exe > test1_output.txt

Test #2 - No heals:
(echo 4&echo 20&echo 5&echo 50&echo 1&echo 4) | main.exe > test2_output.txt

Test #3 - No tanks:
(echo 4&echo 10&echo 50&echo 100&echo 1&echo 4) | main.exe > test3_output.txt

Test #4 - Many players:
(echo 8&echo 200&echo 200&echo 1000&echo 1&echo 4) | main.exe > test4_output.txt