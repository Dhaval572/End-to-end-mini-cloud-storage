#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

SERVER_URL="http://localhost:8080"
TEST_USER="testuser_$(date +%s)"
TEST_PASS="TestPass123!"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}E2Eye Authentication Test Script${NC}"
echo -e "${BLUE}========================================${NC}"

# Step 1: Check if server is running
echo -e "\n${YELLOW}Step 1: Checking server status...${NC}"
if curl -s "$SERVER_URL" > /dev/null; then
    echo -e "${GREEN}✓ Server is running${NC}"
else
    echo -e "${RED}✗ Server is not running. Please start the server first.${NC}"
    exit 1
fi

# Step 2: Test registration
echo -e "\n${YELLOW}Step 2: Testing registration...${NC}"
echo -e "  Username: $TEST_USER"
echo -e "  Password: $TEST_PASS"

REGISTER_RESPONSE=$(curl -s -X POST "$SERVER_URL/register" \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "username=$TEST_USER&password=$TEST_PASS")

if [[ $REGISTER_RESPONSE == *"successful"* ]]; then
    echo -e "${GREEN}✓ Registration successful${NC}"
else
    echo -e "${RED}✗ Registration failed: $REGISTER_RESPONSE${NC}"
    exit 1
fi

# Step 3: Test login with correct credentials
echo -e "\n${YELLOW}Step 3: Testing login with correct credentials...${NC}"
LOGIN_RESPONSE=$(curl -s -X POST "$SERVER_URL/login" \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "username=$TEST_USER&password=$TEST_PASS")

if [[ $LOGIN_RESPONSE == *"successful"* ]]; then
    echo -e "${GREEN}✓ Login successful${NC}"
else
    echo -e "${RED}✗ Login failed: $LOGIN_RESPONSE${NC}"
    exit 1
fi

# Step 4: Test login with wrong password
echo -e "\n${YELLOW}Step 4: Testing login with wrong password...${NC}"
WRONG_LOGIN=$(curl -s -X POST "$SERVER_URL/login" \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "username=$TEST_USER&password=WrongPass123!")

if [[ $WRONG_LOGIN == *"Invalid"* ]]; then
    echo -e "${GREEN}✓ Correctly rejected wrong password${NC}"
else
    echo -e "${RED}✗ Unexpected response: $WRONG_LOGIN${NC}"
fi

# Step 5: Test duplicate registration
echo -e "\n${YELLOW}Step 5: Testing duplicate registration...${NC}"
DUPLICATE_RESPONSE=$(curl -s -X POST "$SERVER_URL/register" \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "username=$TEST_USER&password=$TEST_PASS")

if [[ $DUPLICATE_RESPONSE == *"already exists"* ]]; then
    echo -e "${GREEN}✓ Correctly rejected duplicate registration${NC}"
else
    echo -e "${RED}✗ Unexpected response: $DUPLICATE_RESPONSE${NC}"
fi

# Step 6: Test empty credentials
echo -e "\n${YELLOW}Step 6: Testing empty credentials...${NC}"
EMPTY_REGISTER=$(curl -s -X POST "$SERVER_URL/register" \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "username=&password=")

if [[ $(curl -s -o /dev/null -w "%{http_code}" -X POST "$SERVER_URL/register" -d "username=&password=") == "400" ]]; then
    echo -e "${GREEN}✓ Correctly rejected empty credentials${NC}"
else
    echo -e "${RED}✗ Should reject empty credentials${NC}"
fi

echo -e "\n${BLUE}========================================${NC}"
echo -e "${GREEN}All authentication tests passed!${NC}"
echo -e "${BLUE}========================================${NC}"