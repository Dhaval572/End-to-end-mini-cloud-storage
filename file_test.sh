#!/bin/bash

echo "=== File Upload/Download Test ==="

# Create test files
echo "Creating test files..."
echo "This is test file 1 content" > test_file1.txt
echo "This is test file 2 content" > test_file2.txt

# Start server
echo "Starting server..."
./server &
SERVER_PID=$!
sleep 2

# Register and login user
echo "Setting up test user..."
curl -X POST "http://localhost:8080/register" \
     -d "username=testuser&password=testpass" \
     -w "\nRegister Response: %{http_code}\n"

# Test file upload
echo "Testing file upload..."

# Upload file 1
curl -X POST "http://localhost:8080/upload" \
     -F "file=@test_file1.txt" \
     -F "username=testuser" \
     -F "filename=test_file1.txt" \
     -w "Upload 1 Response: %{http_code}\n"

# Upload file 2
curl -X POST "http://localhost:8080/upload" \
     -F "file=@test_file2.txt" \
     -F "username=testuser" \
     -F "filename=test_file2.txt" \
     -w "Upload 2 Response: %{http_code}\n"

# List files
echo "Listing uploaded files..."
curl -X GET "http://localhost:8080/files?username=testuser" \
     -w "\nList Response: %{http_code}\n"

# Test file download
echo "Testing file download..."

# Download file 1
curl -X GET "http://localhost:8080/download?username=testuser&filename=test_file1.txt" \
     -o downloaded_file1.txt \
     -w "Download 1 Response: %{http_code}\n"

# Download file 2
curl -X GET "http://localhost:8080/download?username=testuser&filename=test_file2.txt" \
     -o downloaded_file2.txt \
     -w "Download 2 Response: %{http_code}\n"

# Verify downloaded content
echo "Verifying downloaded content..."
echo "Original file 1:"
cat test_file1.txt
echo "Downloaded file 1:"
cat downloaded_file1.txt

echo "Original file 2:"
cat test_file2.txt
echo "Downloaded file 2:"
cat downloaded_file2.txt

# Test storage usage
echo "Checking storage usage..."
curl -X GET "http://localhost:8080/storage?username=testuser" \
     -w "\nStorage Response: %{http_code}\n"

# Clean up
echo "Cleaning up..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true
rm -f test_file1.txt test_file2.txt downloaded_file1.txt downloaded_file2.txt

echo "=== File Test Complete ==="