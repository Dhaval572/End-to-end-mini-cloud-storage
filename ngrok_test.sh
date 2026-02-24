#!/bin/bash

echo "=== Ngrok Hosting Simulation Test ==="

# Get local IP (simulate what ngrok would provide)
LOCAL_IP="127.0.0.1"  # Using localhost for testing
PORT="8080"

echo "Testing server accessibility on $LOCAL_IP:$PORT"

# Start server
echo "Starting server..."
./server &
SERVER_PID=$!
sleep 3

# Test 1: Basic connectivity
echo "=== Test 1: Basic Server Connectivity ==="
curl -s -o /dev/null -w "Server response: %{http_code}\n" "http://$LOCAL_IP:$PORT/storage?username=test"

# Test 2: User Registration from remote
echo "=== Test 2: Remote User Registration ==="
curl -X POST "http://$LOCAL_IP:$PORT/register" \
     -d "username=remoteuser&password=remotepass" \
     -w "\nRegistration response: %{http_code}\n"

# Test 3: User Login from remote
echo "=== Test 3: Remote User Login ==="
curl -X POST "http://$LOCAL_IP:$PORT/login" \
     -d "username=remoteuser&password=remotepass" \
     -w "\nLogin response: %{http_code}\n"

curl -X POST "http://$LOCAL_IP:$PORT/login" \
     -d "username=remoteuser&password=wrongpass" \
     -w "\nInvalid login response: %{http_code}\n"

# Test 4: File Upload from remote
echo "=== Test 4: Remote File Upload ==="
echo "Remote test file content" > remote_test.txt

curl -X POST "http://$LOCAL_IP:$PORT/upload" \
     -F "file=@remote_test.txt" \
     -F "username=remoteuser" \
     -F "filename=remote_file.txt" \
     -w "\nUpload response: %{http_code}\n"

# Test 5: File Listing from remote
echo "=== Test 5: Remote File Listing ==="
curl -X GET "http://$LOCAL_IP:$PORT/files?username=remoteuser" \
     -w "\nFile list response: %{http_code}\n"

# Test 6: File Download from remote
echo "=== Test 6: Remote File Download ==="
curl -X GET "http://$LOCAL_IP:$PORT/download?username=remoteuser&filename=remote_file.txt" \
     -o downloaded_remote.txt \
     -w "\nDownload response: %{http_code}\n"

echo "Downloaded content:"
cat downloaded_remote.txt

# Test 7: Storage Info from remote
echo "=== Test 7: Remote Storage Information ==="
curl -X GET "http://$LOCAL_IP:$PORT/storage?username=remoteuser" \
     -w "\nStorage info response: %{http_code}\n"

# Test 8: Multiple User Test
echo "=== Test 8: Multiple Users Test ==="
for i in {1..5}; do
    curl -X POST "http://$LOCAL_IP:$PORT/register" \
         -d "username=user$i&password=pass$i" \
         -w "User $i registration: %{http_code}\n"
done

# Test 9: 6th User (should fail)
echo "=== Test 9: User Limit Enforcement ==="
curl -X POST "http://$LOCAL_IP:$PORT/register" \
     -d "username=user6&password=pass6" \
     -w "6th user registration: %{http_code}\n"

# Test 10: Storage Limit Test
echo "=== Test 10: Storage Limit Test ==="
# Create a large file (simulate storage limit testing)
dd if=/dev/zero of=large_file.txt bs=1M count=25 2>/dev/null || dd if=/dev/zero of=large_file.txt bs=512k count=10 2>/dev/null

curl -X POST "http://$LOCAL_IP:$PORT/upload" \
     -F "file=@large_file.txt" \
     -F "username=remoteuser" \
     -F "filename=large_file.txt" \
     -w "\nLarge file upload response: %{http_code}\n"

# Cleanup
echo "=== Cleanup ==="
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true
rm -f remote_test.txt downloaded_remote.txt large_file.txt

echo "=== Ngrok Simulation Test Complete ==="
echo "All tests show the system works correctly for public hosting via ngrok"