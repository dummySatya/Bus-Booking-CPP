#include <bits/stdc++.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define INITIAL_BUSES 2
#define MAX_CLIENTS 10
#define MAX_BUSES 4
#define MAX_SEATS 5
#define TIMER 5
#define BUSES_TO_BE_ADDED 2
#define LOAD_EXCEED_FACTOR 0.8
#define LOAD_LESS_FACTOR 0.2

#define PORT 8081
#define MAX_BUFFER 1024

// Custom return codes
#define CLIENT_INFO_INVALID_CLIENT 900                  // Enter a valid client ID
#define CLIENT_INFO_INVALID_SEAT_BUS 901                // Invalid Bus or Seat
#define CLIENT_INFO_SEAT_BOOKED_ALREADY 902             // Seat already booked
#define CLIENT_INFO_SEAT_OCCUPIED 903                   // Seat selected by someone else
#define CLIENT_INFO_SEAT_NOT_BOOKED 904                 // Seat not booked
#define CLIENT_INFO_SEAT_NOT_BOOKED_BY_CLIENT 905       // Seat booked by someone else
#define CLIENT_INFO_SEAT_NOT_SELECTED_BY_CLIENT 906     // Seat not selected (for canceling it must be selected first)
#define CLIENT_INFO_SEAT_ALREADY_SELECTED_BY_CLIENT 907 // Seat already selected

#define CLIENT_ERROR 0;
#define CLIENT_SUCCESS 1;

#define CLIENT_SUCCESS_SEAT_BOOKING 800 // Successfully Booked Seat
#define CLIENT_SUCCESS_SEAT_CANCEL 801  // Successfully Cancelled Seat
#define CLIENT_SUCCESS_SEAT_SELECT 802  // Successfully Selected Seat

using namespace std;

class BusBooking
{
protected:
    struct Client
    {
        map<int, set<int>> seatsBooked; // bus:seats mapping
        // chose set over vector bcs deletion is much more costlier in vector
        map<int, set<int>> seatsSelected;
    };
    struct Bus
    {
        int seats[MAX_SEATS]; // 0 booked, 1 freee, 2 locked
        chrono::time_point<chrono::steady_clock> seatLockDurations[MAX_SEATS];
        int bookedSeats;
        bool active;        // is false only when some merger has happened
        bool loadExceeding; // to set the flag to true only when first time the bus exceedingm so that even if seat is booked on an overloaded bus, the busesExceeding wont again trigger and increment, also becomes false if load less than threshhold
    };

    Bus buses[MAX_BUSES];
    Client clients[MAX_CLIENTS];
    unordered_map<int, unordered_map<int, int>> seatToClientMap;
    int emptyBuses[MAX_BUSES];    // array of empty buses
    int *numEmptyBuses = new int; // empty buses are only formed after a merger
    int *totalBuses = new int;
    int *busesExceedingLoad = new int;
    // initializing it bcs it not would lead to segerr
public:
    BusBooking() // initial no. of buses
    {
        for (int c = 0; c < INITIAL_BUSES; c++)
        {
            auto &bus = buses[c];
            for (int i = 0; i < MAX_SEATS; i++)
            {
                bus.seats[i] = 1;
            }
            bus.bookedSeats = 0;
            bus.active = true;
            bus.loadExceeding = false;
        }
        *totalBuses = INITIAL_BUSES;
        *busesExceedingLoad = 0;
        *numEmptyBuses = 0;
    }
    // virtual ~BusBooking() {}

    void printer()
    {
        for (int i = 0; i < MAX_BUSES; i++)
        {
            auto bus = buses[i];
            if (bus.active != true)
                continue;
            cout << "Seat Status, Bus ID: " << i << endl;
            for (auto y : bus.seats)
                cout << y << " ";
            cout << endl;
            cout << "Booked Seats: " << bus.bookedSeats << endl;
        }
        cout << "Total Buses: " << (*totalBuses) << endl;
        cout << "No. of empty buses(post merger formed): " << (*numEmptyBuses) << endl;
        cout << "Empty Buses (post merger): " << endl;
        for (int i = 0; i < (*numEmptyBuses); i++)
            cout << emptyBuses[i] << " ";
        cout << endl;
        cout << "Buses exceeding load: " << (*busesExceedingLoad) << endl;
    }

    int clientDetails(int clientID)
    {
        cout << "Details of Client " << clientID << endl;
        auto client = clients[clientID];
        cout << "Seats Booked: " << endl;
        for (auto seatDetails : client.seatsBooked)
        {
            cout << "BusID " << seatDetails.first << " : ";
            for (auto seat : seatDetails.second)
            {
                cout << seat << " ";
            }
            cout << endl;
        }
        cout << endl;
        cout << "Seats Selected: " << endl;
        for (auto seatDetails : client.seatsSelected)
        {
            cout << "BusID " << seatDetails.first << " : ";
            for (auto seat : seatDetails.second)
            {
                cout << seat << " ";
            }
            cout << endl;
        }
        cout << endl;
        return CLIENT_SUCCESS;
    }

    double loadFactor(int busID)
    {
        double load = (double)buses[busID].bookedSeats / MAX_SEATS;
        return load;
    }

    bool checkLoadExceeds(double load)
    {
        if (load >= LOAD_EXCEED_FACTOR)
        {
            return true;
        }
        return false;
    }

    bool checkLoadLessThan(double load)
    {
        if (load <= LOAD_LESS_FACTOR)
        {
            return true;
        }
        return false;
    }

    void busAdder()
    {
        int busesExpansion = BUSES_TO_BE_ADDED - *numEmptyBuses;
        if (busesExpansion > 0)
        {
            //
            for (int bus = 0; bus < *numEmptyBuses; bus++)
            {
                int busid = emptyBuses[bus];
                for (int i = 0; i < MAX_SEATS; i++)
                {
                    buses[busid].seats[i] = 1;
                }
                buses[busid].active = true;
                buses[busid].bookedSeats = 0;
                buses[busid].loadExceeding = false;
            }
            (*totalBuses) += (*numEmptyBuses);
            *numEmptyBuses = 0;
            for (int busesToBeAdded = 0; busesToBeAdded < busesExpansion; busesToBeAdded++)
            {
                for (int i = 0; i < MAX_SEATS; i++)
                {
                    buses[(*totalBuses)].seats[i] = 1;
                }
                buses[(*totalBuses)].active = true;
                buses[(*totalBuses)].bookedSeats = 0;
                buses[(*totalBuses)].loadExceeding = false;
                (*totalBuses)++;
            }
        }
        else
        {
            for (int bus = 0; bus < BUSES_TO_BE_ADDED; bus++)
            {
                int busid = emptyBuses[bus];
                for (int i = 0; i < MAX_SEATS; i++)
                {
                    buses[busid].seats[i] = 1;
                }
                buses[busid].active = true;
                buses[busid].bookedSeats = 0;
                buses[busid].loadExceeding = false;
                (*numEmptyBuses)--;
            }
            *totalBuses += BUSES_TO_BE_ADDED;
        }
    }

    void loadChanger(int busID)
    {
        double load = loadFactor(busID);
        cout << "Load: " << load << endl;
        if ((!buses[busID].loadExceeding) && checkLoadExceeds(load)) // during book
        {
            buses[busID].loadExceeding = true;
            ++(*busesExceedingLoad);
        }
        else if (buses[busID].loadExceeding && (!checkLoadExceeds(load))) // during cancel
        {
            buses[busID].loadExceeding = false;
            --(*busesExceedingLoad);
        }
        if ((*busesExceedingLoad) == (*totalBuses))
        {
            busAdder();
        }
    }

    vector<pair<int, int>> mergerPossiblities()
    {
        // returns bus ids of possible buses which can be merged
        vector<pair<int, int>> possibleBuses;
        for (int bus1 = 0; bus1 < MAX_BUSES - 1; bus1++)
        {
            for (int bus2 = bus1 + 1; buses[bus1].active && bus2 < MAX_BUSES; bus2++)
            {
                if (!buses[bus2].active)
                    break;
                bool a = checkLoadLessThan(loadFactor(bus1));
                bool b = checkLoadLessThan(loadFactor(bus2));
                cout << "Bus" << bus1 << " bool load: " << a << endl;
                cout << "Bus" << bus2 << " bool load: " << b << endl;
                bool loadLessCondition = a && b;
                if (!loadLessCondition)
                {
                    continue;
                }
                bool possible = true;
                for (int seat = 0; seat < MAX_SEATS; seat++)
                {
                    // not possible only if both seats are booked
                    if ((buses[bus1].seats[seat] == 0 && buses[bus2].seats[seat] == 0))
                    {
                        possible = false;
                        break;
                    }
                }
                if (possible)
                {
                    possibleBuses.push_back({bus1, bus2});
                }
            }
        }
        return possibleBuses;
    }

    bool merge(int bus1, int bus2, vector<pair<int,int>>&possibilities)
    {
        if (bus2 < bus1)
        {
            swap(bus1, bus2); // merging into a smaller bus id
        }
        bool pairFound = false;
        for(auto &pairs: possibilities){
            if(pairs.first == bus1 && pairs.second == bus2){
                pairFound = true;
                break;
            }
        }
        if(!pairFound){
            return false;
        }
        for (int seat = 0; seat < MAX_SEATS; seat++)
        {
            if (buses[bus1].seats[seat] != 0 && buses[bus2].seats[seat] == 0)
            {
                buses[bus1].seats[seat] = 0;
                int clientID = seatToClientMap[bus2][seat];

                seatToClientMap[bus1][seat] = clientID;
                clients[clientID].seatsBooked[bus1].insert(seat);

                seatToClientMap[bus2].erase(seat);
                clients[clientID].seatsBooked[bus2].erase(seat);
            }
        }
        emptyBuses[(*numEmptyBuses)++] = bus2;
        buses[bus2].active = false;
        (*totalBuses)--;
        return true;
    }

    bool validSeat(int busID, int seatNo)
    {
        if (seatNo >= MAX_SEATS || !buses[busID].active)
        {
            return false;
        }
        return true;
    }

    int seatAvailability(int busID, int seatNo)
    {
        if (buses[busID].seats[seatNo] == 0)
            return 0; // seat booked
        if (buses[busID].seats[seatNo] == 1)
            return 1; // seat free
        return 2;     // seat selected
    }

    bool validClient(int clientID)
    {
        if (clientID >= MAX_CLIENTS)
            return false;
        return true;
    }

    int validSeatClient(int busID, int seatNo, int clientID)
    {
        if (!validClient(clientID))
        {
            cout << "Enter a valid client ID less than " << MAX_CLIENTS << endl;
            return CLIENT_INFO_INVALID_CLIENT;
        }
        if (!validSeat(busID, seatNo))
        {
            cout << "Invalid Bus or Seat" << endl;
            return CLIENT_INFO_INVALID_SEAT_BUS;
        }
        return 0;
    }

    int bookSeat(int busID, int seatNo, int clientID)
    {

        int validSeatClientStatus = validSeatClient(busID, seatNo, clientID);
        if (validSeatClientStatus == CLIENT_INFO_INVALID_CLIENT || validSeatClientStatus == CLIENT_INFO_INVALID_SEAT_BUS)
        {
            return validSeatClientStatus;
        }
        int seatStatus = seatAvailability(busID, seatNo);
        if (seatStatus == 0)
        {
            cout << "Seat already booked" << endl;
            return CLIENT_INFO_SEAT_BOOKED_ALREADY;
        }

        else if (seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) == clients[clientID].seatsSelected[busID].end())
        {
            cout << "Seat selected by someone else" << endl;
            return CLIENT_INFO_SEAT_NOT_SELECTED_BY_CLIENT;
        }

        buses[busID].seats[seatNo] = 0;
        ++buses[busID].bookedSeats;
        cout << "Booked Seats: " << buses[busID].bookedSeats << endl;
        clients[clientID].seatsBooked[busID].insert(seatNo);
        if (clients[clientID].seatsSelected[busID].find(seatNo) != clients[clientID].seatsSelected[busID].end())
        {
            clients[clientID].seatsSelected[busID].erase(seatNo);
        }
        if (clients[clientID].seatsSelected[busID].size() == 0)
        {
            clients[clientID].seatsSelected.erase(busID);
        }
        seatToClientMap[busID][seatNo] = clientID;
        loadChanger(busID);
        cout << "Client" << clientID << " booked seat " << seatNo << " on bus id " << busID << endl;
        return CLIENT_SUCCESS_SEAT_BOOKING;
    }

    int cancelSeat(int busID, int seatNo, int clientID)
    {
        int validSeatClientStatus = validSeatClient(busID, seatNo, clientID);
        if (validSeatClientStatus == CLIENT_INFO_INVALID_CLIENT || validSeatClientStatus == CLIENT_INFO_INVALID_SEAT_BUS)
        {
            return validSeatClientStatus;
        }

        int seatStatus = seatAvailability(busID, seatNo);
        if (seatStatus == 1)
        {
            cout << "Seat not booked" << endl;
            return CLIENT_INFO_SEAT_NOT_BOOKED;
        }
        if (seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) == clients[clientID].seatsSelected[busID].end())
        { // if locked, but u didnt do it
            cout << "Cannot cancel as Seat is not selected by you" << endl;
            return CLIENT_INFO_SEAT_NOT_SELECTED_BY_CLIENT;
        }
        if (seatStatus == 0 && clients[clientID].seatsBooked[busID].find(seatNo) == clients[clientID].seatsBooked[busID].end())
        { // if booked, but u didnt do it
            cout << "Cannot cancel Seat is not booked by you" << endl;
            return CLIENT_INFO_SEAT_NOT_BOOKED_BY_CLIENT;
        }

        if (seatStatus == 0)
        {
            clients[clientID].seatsBooked[busID].erase(seatNo);
            if (clients[clientID].seatsBooked[busID].size() == 0)
            {
                clients[clientID].seatsBooked.erase(busID);
            }
            seatToClientMap[busID].erase(seatNo);
            --buses[busID].bookedSeats;
            loadChanger(busID);
        }
        else if (seatStatus == 2)
        {
            clients[clientID].seatsSelected[busID].erase(seatNo);
            if (clients[clientID].seatsSelected[busID].size() == 0)
            {
                clients[clientID].seatsSelected.erase(busID);
            }
            seatToClientMap[busID].erase(seatNo);
        }
        buses[busID].seats[seatNo] = 1;
        cout << "Client" << clientID << " cancelled seat " << seatNo << " on bus id " << busID << endl;
        return CLIENT_SUCCESS_SEAT_CANCEL;
    }

    int selectSeat(int busID, int seatNo, int clientID)
    {
        int validSeatClientStatus = validSeatClient(busID, seatNo, clientID);
        if (validSeatClientStatus == CLIENT_INFO_INVALID_CLIENT || validSeatClientStatus == CLIENT_INFO_INVALID_SEAT_BUS)
        {
            return validSeatClientStatus;
        }

        int seatStatus = seatAvailability(busID, seatNo);
        if (seatStatus == 0)
        {
            cout << "Seat already booked" << endl;
            return CLIENT_INFO_SEAT_BOOKED_ALREADY;
        }

        if (seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) == clients[clientID].seatsSelected[busID].end())
        {
            cout << "Seat selected by someone else" << endl;
            return CLIENT_INFO_SEAT_NOT_SELECTED_BY_CLIENT;
        }
        if (seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) != clients[clientID].seatsSelected[busID].end())
        {
            cout << "Already selected by you !!" << endl;
            return CLIENT_INFO_SEAT_ALREADY_SELECTED_BY_CLIENT;
        }
        buses[busID].seats[seatNo] = 2;
        buses[busID].seatLockDurations[seatNo] = chrono::steady_clock::now();
        clients[clientID].seatsSelected[busID].insert(seatNo);
        seatToClientMap[busID][seatNo] = clientID;
        cout << "Client" << clientID << " selected seat " << seatNo << " on bus id " << busID << endl;
        return CLIENT_SUCCESS_SEAT_SELECT;
    }

    void timerEvents()
    {
        auto now = chrono::steady_clock::now();
        for (int bus = 0; bus < MAX_BUSES; ++bus)
        {
            for (int seat = 0; buses[bus].active && seat < MAX_SEATS; ++seat)
            {
                if (buses[bus].seats[seat] == 2)
                {
                    auto duration = chrono::duration_cast<chrono::seconds>(now - buses[bus].seatLockDurations[seat]).count();
                    if (duration >= TIMER)
                    {
                        buses[bus].seats[seat] = 1;
                        int clientID = seatToClientMap[bus][seat];
                        clients[clientID].seatsSelected[bus].erase(seat);
                        if (clients[clientID].seatsSelected[bus].size() == 0)
                        {
                            clients[clientID].seatsSelected.erase(bus);
                        }
                        cout << "Seat Released for seat " << seat << " on bus id " << bus << endl;
                    }
                }
            }
        }
    }
};

class Server : public BusBooking
{
public:
    mutex mtxAction;

protected:
    int serveClient(int busID, int seatNo, int clientID, string action)
    {
        int status = 0;
        if (action == "select")
        {
            // lock_guard<mutex> lock(mtxAction);
            status = selectSeat(busID, seatNo, clientID);
        }
        else if (action == "book")
        {
            // lock_guard<mutex> lock(mtxAction);
            status = bookSeat(busID, seatNo, clientID);
        }
        else if (action == "cancel")
        {
            // lock_guard<mutex> lock(mtxAction);
            status = cancelSeat(busID, seatNo, clientID);
        }
        else if (action == "details")
        {
            // lock_guard<mutex> lock(mtxAction);
            status = clientDetails(clientID);
            printer();
        }
        return status;
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    bool mergingInProgress = false;
    std::vector<int> clients; // Store active client sockets

public:
    void serveClientSocket(int socket_fd)
    {
        {
            std::lock_guard<std::mutex> lock(mtx);
            clients.push_back(socket_fd); // Add the client to the list
        }

        char buffer[MAX_BUFFER] = {0};
        int clientID;
        std::cout << "Client Connected: " << socket_fd << std::endl;
        recv(socket_fd, buffer, MAX_BUFFER, 0);
        std::string clientIDString = std::string(buffer);
        std::cout << clientIDString << std::endl;

        try
        {
            clientID = std::stoi(clientIDString);
            if (clientID < 0 || clientID >= MAX_CLIENTS)
            {
                std::string response = "Enter correct client ID\n";
                send(socket_fd, response.c_str(), response.size(), 0);
                return;
            }
            else
            {
                std::string response = "Login Success\n";
                send(socket_fd, response.c_str(), response.size(), 0);
            }
        }
        catch (const std::exception &e)
        {
            std::string response = "Invalid Input\n";
            send(socket_fd, response.c_str(), response.size(), 0);
            return;
        }

        while (true)
        {
            std::string action;
            int busID, seatNo;
            char actionBuffer[100];
            recv(socket_fd, buffer, MAX_BUFFER, 0);
            sscanf(buffer, "%s %d %d", actionBuffer, &busID, &seatNo);
            action = actionBuffer;

            // Handle Admin Actions
            if (action == "admin")
            {
                std::string response = "Enter show or merge\n";
                send(socket_fd, response.c_str(), response.size(), 0);
                handleAdminAction(socket_fd);  // Process admin actions
                continue;
            }

            std::cout << "Action: " << action << std::endl;
            {
                std::unique_lock<std::mutex> lock(mtx);
                if (mergingInProgress)
                {
                    std::string response = "Bus merger in progress, please wait...\n";
                    send(socket_fd, response.c_str(), response.size(), 0);
                    cv.wait(lock, [&] { return !mergingInProgress; });
                }
            }

            // Process client actions
            processClientAction(socket_fd, action, busID, seatNo, clientID);
        }
        close(socket_fd);
    }

    // Handle client actions like booking, selecting, cancelling
    void processClientAction(int socket_fd, const std::string& action, int busID, int seatNo, int clientID)
    {
        std::lock_guard<std::mutex> lock(mtx);
        // Process "select", "book", "cancel", and "details" actions
        if (action == "select" || action == "book" || action == "cancel" || action == "details")
        {
            int serverResponse = serveClient(busID, seatNo, clientID, action);
            std::string response = std::to_string(serverResponse) + "\n";
            send(socket_fd, response.c_str(), response.size(), 0);
        }
        else
        {
            std::string response = "Invalid Action\n";
            send(socket_fd, response.c_str(), response.size(), 0);
        }
    }

    // Handle admin actions like show merge possibility and merge
    void handleAdminAction(int socket_fd)
    {
        char actionBuffer[100];
        char buffer[MAX_BUFFER];
        recv(socket_fd, buffer, MAX_BUFFER, 0);
        int bus1, bus2;
        sscanf(buffer, "%s %d %d", actionBuffer, &bus1, &bus2);
        std::string adminAction = actionBuffer;

        std::vector<pair<int,int>> mergePossibilityVector = mergerPossiblities();
        if (adminAction == "show")
        {
            std::string mergePossibility;
            for(auto &pairs: mergePossibilityVector){
                mergePossibility += "Bus " + to_string(pairs.first) + " and Bus " + to_string(pairs.second) + "\n";
            }
            cout<<"Possibility: "<<endl;
            cout<<mergePossibility<<endl;
            send(socket_fd, mergePossibility.c_str(), mergePossibility.size(), 0);
        }
        else if (adminAction == "merge")
        {
            if(merge(bus1, bus2,mergePossibilityVector)){
                notifyClientsOfMerge();
                std::lock_guard<std::mutex> lock(mtx);
                mergingInProgress = true;
                std::string response = "Merging Completed\n";
                send(socket_fd, response.c_str(), response.size(), 0);
                mergingInProgress = false;
                notifyClientsMergeComplete();
            }
            else{
                std::string response = "Invalid Buses\n";
                send(socket_fd, response.c_str(), response.size(), 0);
            }            
        }
        else
        {
            std::string response = "Invalid Admin Action\n";
            send(socket_fd, response.c_str(), response.size(), 0);
        }
    }

    // Notify clients that bus merger is happening
    void notifyClientsOfMerge()
    {
        std::lock_guard<std::mutex> lock(mtx);
        for (int clientSocket : clients)
        {
            std::string response = "Bus merger is ongoing. Please wait...\n";
            send(clientSocket, response.c_str(), response.size(), 0);
        }
    }

    // Notify clients that bus merger is complete
    void notifyClientsMergeComplete()
    {
        std::lock_guard<std::mutex> lock(mtx);
        cv.notify_all();
        for (int clientSocket : clients)
        {
            std::string response = "Bus merger completed. You may now continue.\n";
            send(clientSocket, response.c_str(), response.size(), 0);
        }
    }
};

int main()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Binding socket to port 8080
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listening on the socket
    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    cout << "BUS BOOKING SYSTEM SERVER RUNNING" << endl;

    Server server; // Create the server instance

    thread timerThread([&server]()
                       {
        while (true) {
            server.timerEvents();  // Check the seat status
            std::this_thread::sleep_for(std::chrono::seconds(3));  // Adjust interval as needed
        } });
    timerThread.detach();

    while (true)
    {
        cout << "Waiting for connections ...." << endl;

        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0)
        {
            perror("accept");
            continue; // Instead of exiting, log the error and continue accepting other clients
        }

        cout << "Connection Accepted" << endl;

        // Handle the client in a separate thread (or iteratively without threads)
        std::thread clientThread([&server, new_socket]()
                                 { server.serveClientSocket(new_socket); });
        clientThread.detach(); // Detach to handle clients asynchronously
    }

    close(server_fd); // Close the server socket when done
}
