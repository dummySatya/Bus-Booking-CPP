#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8081      
#define MAX_BUFFER 1024

std::string readFromSocket(int sock) {
    std::string result;
    char buffer[MAX_BUFFER] = {0};
    while (true) {
        int valread = read(sock, buffer, MAX_BUFFER - 1);
        if (valread < 0) {
            std::cerr << "Error reading from server." << std::endl;
            break;
        }
        buffer[valread] = '\0';  // Null-terminate for safety
        result += buffer;
        
        // Check if the message has a termination (like newline)
        if (result.find('\n') != std::string::npos) {
            break;
        }
    }
    return result;
}

int main()
{
    int sock = 0;
    struct sockaddr_in server_addr;
    char buffer[MAX_BUFFER] = {0};

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid address / Address not supported" << std::endl;
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Connection failed" << std::endl;
        return -1;
    }


    std::string mode;
    std::cout<< "Enter mode (admin or client): ";
    std:: cin>>mode;
    send(sock, (mode + "\n").c_str(), mode.size(), 0); // Send mode to server
    std::string response = readFromSocket(sock);
    std::cout << "Server response: " << response << std::endl;
    if(mode == "client"){
        std::string clientID;
        std::cout << "Enter client ID: ";
        std::cin >> clientID;
        clientID += "\n";     
        send(sock, clientID.c_str(), clientID.size(), 0); // Send client ID to server
        std::string response = readFromSocket(sock); 
        std::cout << "Server response: " << response << std::endl;

    }
    else if(mode != "admin"){
        std::cout<<"Invalid mode\n";
        close(sock);
    }
    
    int iters = 0;
    while (true)
    {
        memset(buffer, 0, MAX_BUFFER); // Clear buffer before each use

        std::string input;
        if(iters++ != 0) std::cout << "Enter your action: ";
        std::getline(std::cin, input);

        if(input == ""){
            continue;
        }

        if (input == "exit") 
        {
            break;
        }

        input += "\n";                              // Ensure newline termination
        send(sock, input.c_str(), input.size(), 0); // Send action to server
        std::cout << "Request sent to server." << std::endl;

        std::string response = readFromSocket(sock);
        std::cout << "Server response: " << response << std::endl;
    }

    close(sock);
    return 0;
}