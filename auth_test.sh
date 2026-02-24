#!/bin/bash

echo "=== Authentication System Test ==="

# Start server in background
echo "Starting server..."
./server &
SERVER_PID=$!
sleep 2

# Test registration
echo "Testing user registration..."

# Register user1
curl -X POST "http://localhost:8080/register" \
     -d "username=user1&password=password123" \
     -w "\nResponse: %{http_code}\n"

# Register user2
curl -X POST "http://localhost:8080/register" \
     -d "username=user2&password=password456" \
     -w "\nResponse: %{http_code}\n"

# Try to register 6th user (should fail)
curl -X POST "http://localhost:8080/register" \
     -d "username=user6&password=password789" \
     -w "\nResponse: %{http_code}\n"

# Test login
echo "Testing user login..."

# Valid login
curl -X POST "http://localhost:8080/login" \
     -d "username=user1&password=password123" \
     -w "\nResponse: %{http_code}\n"

# Invalid login
curl -X POST "http://localhost:8080/login" \
     -d "username=user1&password=wrongpassword" \
     -w "\nResponse: %{http_code}\n"

# Non-existent user
curl -X POST "http://localhost:8080/login" \
     -d "username=nonexistent&password=password" \
     -w "\nResponse: %{http_code}\n"

# Test storage info
echo "Testing storage info..."

curl -X GET "http://localhost:8080/storage?username=user1" \
     -w "\nResponse: %{http_code}\n"

# Clean up
echo "Stopping server..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

echo "=== Test Complete ==="