#!/bin/bash

# Simple build script for E2Eye

BUILD_DIR="build"
SERVER_URL="http://localhost:8080"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

setup_intellisense()
{
    echo -e "${BLUE}Setting up IntelliSense...${NC}"
    mkdir -p .vscode
    
    cat > .vscode/c_cpp_properties.json << EOF
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "\${workspaceFolder}/**",
                "\${workspaceFolder}/common",
                "\${workspaceFolder}/${BUILD_DIR}/_deps/httplib-src",
                "\${workspaceFolder}/${BUILD_DIR}/_deps/ftxui-src/include"
            ],
            "compileCommands": "\${workspaceFolder}/${BUILD_DIR}/compile_commands.json",
            "cppStandard": "c++20"
        }
    ]
}
EOF
    echo -e "${GREEN}IntelliSense configured${NC}"
}

case $1 in
    debug)
        echo -e "${BLUE}Building debug...${NC}"
        mkdir -p $BUILD_DIR
        cd $BUILD_DIR
        cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
        ninja
        cd ..
        setup_intellisense
        ;;
        
    release)
        echo -e "${BLUE}Building release...${NC}"
        mkdir -p $BUILD_DIR
        cd $BUILD_DIR
        cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
        ninja
        cd ..
        echo -e "${GREEN}Release build complete${NC}"
        ;;
        
    clean)
        echo -e "${YELLOW}Cleaning...${NC}"
        rm -rf $BUILD_DIR
        echo -e "${GREEN}Clean complete${NC}"
        ;;
        
    test)
        echo -e "${BLUE}Testing authentication...${NC}"
        
        if ! curl -s $SERVER_URL > /dev/null; then
            echo -e "${RED}Server not running. Start with: cd $BUILD_DIR && ./server${NC}"
            exit 1
        fi
        
        echo -e "${YELLOW}Testing registration...${NC}"
        RESPONSE=$(curl -s -X POST $SERVER_URL/register -d "username=test&password=test")
        if [[ $RESPONSE == *"successful"* ]]; then
            echo -e "${GREEN}Registration successful${NC}"
        else
            echo -e "${RED}Registration failed (user may already exist)${NC}"
        fi
        
        echo -e "${YELLOW}Testing login...${NC}"
        RESPONSE=$(curl -s -X POST $SERVER_URL/login -d "username=test&password=test")
        if [[ $RESPONSE == *"successful"* ]]; then
            echo -e "${GREEN}Login successful${NC}"
        else
            echo -e "${RED}Login failed${NC}"
        fi
        ;;
        
    run-server)
        cd $BUILD_DIR && ./server
        ;;
        
    run-client)
        cd $BUILD_DIR && ./client
        ;;
        
    *)
        echo -e "${YELLOW}Usage: ./build.sh [debug|release|clean|test|run-server|run-client]${NC}"
        ;;
esac