#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8081      // Port number to connect to
#define MAX_BUFFER 1024 // Maximum buffer size

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

    // Interact with the server
    std::string clientID;
    std::cout << "Enter client ID: ";
    std::cin >> clientID;
    clientID += "\n";     
    std::cout<<"Sock: "<<sock<<"\n";                            // Ensure newline termination for the server
    send(sock, clientID.c_str(), clientID.size(), 0); // Send client ID to server

    // Read the server's response after sending client ID
    int valread = read(sock, buffer, MAX_BUFFER - 1); // Read server response immediately after sending client ID
    if (valread < 0)
    {
        std::cerr << "Error reading from server." << std::endl;
    }
    else
    {
        buffer[valread] = '\0';                                  // Null terminate the buffer
        std::cout << "Server response: " << buffer << std::endl; // Print response (e.g., "Login Success")
    }

    std::cin.ignore(); // Clear any remaining input after client ID

    while (true)
    {
        memset(buffer, 0, MAX_BUFFER); // Clear buffer before each use

        std::string input;
        std::cout << "Enter your action (or type 'exit' to quit): ";
        std::getline(std::cin, input);

        if (input == "exit") // Handle exit command
        {
            break;
        }

        input += "\n";                              // Ensure newline termination
        send(sock, input.c_str(), input.size(), 0); // Send action to server
        std::cout << "Request sent to server." << std::endl;

        // Read response from server until newline or end of message
        std::string response = readFromSocket(sock); // Custom function to handle multiple reads
        std::cout << "Server response: " << response << std::endl;
    }

    close(sock);
    return 0;
}