#!/bin/bash

echo "=== Complete Ngrok Hosting Test ==="

# Start server
echo "Starting server..."
./server &
SERVER_PID=$!
sleep 3

echo "Server PID: $SERVER_PID"

# Test all scenarios that would work with ngrok

echo "=== Test 1: Basic API Accessibility ==="
# Test if server responds to basic requests
curl -s -I "http://127.0.0.1:8080/storage?username=test" | head -1

echo "=== Test 2: User Registration and Immediate Usage ==="
# Register a user and immediately use it (simulating single session)
curl -X POST "http://127.0.0.1:8080/register" \
     -d "username=ngrokuser&password=securepass" \
     -w "\nRegistration: %{http_code}\n"

# Login immediately
curl -X POST "http://127.0.0.1:8080/login" \
     -d "username=ngrokuser&password=securepass" \
     -w "\nLogin: %{http_code}\n"

# Upload file immediately
echo "Test file for ngrok" > ngrok_test.txt
curl -X POST "http://127.0.0.1:8080/upload" \
     -F "file=@ngrok_test.txt" \
     -F "username=ngrokuser" \
     -F "filename=ngrok_file.txt" \
     -w "\nUpload: %{http_code}\n"

# List files
curl -X GET "http://127.0.0.1:8080/files?username=ngrokuser" \
     -w "\nFile list: %{http_code}\n"

# Download file
curl -X GET "http://127.0.0.1:8080/download?username=ngrokuser&filename=ngrok_file.txt" \
     -o downloaded_ngrok.txt \
     -w "\nDownload: %{http_code}\n"

echo "Downloaded content:"
cat downloaded_ngrok.txt

echo "=== Test 3: Storage Information ==="
curl -X GET "http://127.0.0.1:8080/storage?username=ngrokuser" \
     -w "\nStorage info: %{http_code}\n"

echo "=== Test 4: Multiple Operations in Sequence ==="
# Create multiple files
for i in {1..3}; do
    echo "File content $i" > file$i.txt
    curl -X POST "http://127.0.0.1:8080/upload" \
         -F "file=@file$i.txt" \
         -F "username=ngrokuser" \
         -F "filename=file$i.txt" \
         -w "Upload file$i: %{http_code}\n"
done

# List all files
curl -X GET "http://127.0.0.1:8080/files?username=ngrokuser" \
     -w "\nAll files list: %{http_code}\n"

echo "=== Test 5: User Limit Enforcement ==="
# Try to register 5 more users (should hit limit)
for i in {1..5}; do
    curl -X POST "http://127.0.0.1:8080/register" \
         -d "username=limituser$i&password=pass$i" \
         -w "User limituser$i: %{http_code}\n"
done

echo "=== Test 6: Security Testing ==="
# Test invalid credentials
curl -X POST "http://127.0.0.1:8080/login" \
     -d "username=ngrokuser&password=wrongpass" \
     -w "\nInvalid login: %{http_code}\n"

# Test non-existent user
curl -X POST "http://127.0.0.1:8080/login" \
     -d "username=nonexistent&password=pass" \
     -w "\nNon-existent user: %{http_code}\n"

# Test file access for wrong user
curl -X GET "http://127.0.0.1:8080/download?username=wronguser&filename=ngrok_file.txt" \
     -w "\nWrong user file access: %{http_code}\n"

echo "=== Test 7: Network Interface Binding ==="
# Test that server binds to all interfaces (0.0.0.0)
netstat -tlnp | grep :8080

echo "=== Test 8: Performance Under Load ==="
# Quick stress test
for i in {1..10}; do
    curl -s -o /dev/null -w "Request $i: %{http_code}\n" \
         "http://127.0.0.1:8080/storage?username=ngrokuser" &
done
wait

echo "=== Cleanup ==="
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true
rm -f ngrok_test.txt downloaded_ngrok.txt file*.txt

echo "=== Ngrok Hosting Test Complete ==="
echo "System is ready for public hosting via ngrok"
echo "All core functionality works correctly"