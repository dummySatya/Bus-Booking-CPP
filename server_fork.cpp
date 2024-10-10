#include <bits/stdc++.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <fcntl.h>

#define INITIAL_BUSES 20
#define MAX_CLIENTS 10000
#define MAX_BUSES 100
#define MAX_SEATS 30
#define TIMER 60
#define BUSES_TO_BE_ADDED 2
#define LOAD_EXCEED_FACTOR 0.8
#define LOAD_LESS_FACTOR 0.2

#define PORT 8081

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

mutex fileMtx;

queue<string> logQueue;

void logFileWriter()
{
    int n = logQueue.size();
    auto ptr = fopen("archive.txt", "a");
    while (n--)
    {
        string logText = logQueue.front();
        fprintf(ptr, "%s", logText.c_str());
        logQueue.pop();
    }
    fclose(ptr);
}
void logWriter(string logText)
{
    lock_guard<mutex> lock(fileMtx);
    auto sysNow = chrono::system_clock::now();
    auto timeCFormat = chrono::system_clock::to_time_t(sysNow);
    string currTime = ctime(&timeCFormat);
    currTime.replace(currTime.size() - 1, 1, ""); // to remove the \n from ctime returned string
    logText = currTime + " : " + logText + "\n";
    logQueue.push(logText);
}
class BusBooking
{

protected:
    struct Client
    {
        int busesBooked[MAX_BUSES];
        int busesSelected[MAX_BUSES];

        int seatsBooked[MAX_BUSES][MAX_SEATS]; // bus:seats mapping
        int seatsSelected[MAX_BUSES][MAX_SEATS];
    };
    struct Bus
    {
        int seats[MAX_SEATS]; // 0 booked, 1 free, 2 locked
        chrono::time_point<chrono::steady_clock> seatLockDurations[MAX_SEATS];
        int bookedSeats;
        bool active;        // false when a merger happens
        bool loadExceeding;
    };
    
public:
    struct Data{
        Bus buses[MAX_BUSES];
        Client clients[MAX_CLIENTS];
        int seatToClientMap[MAX_BUSES][MAX_SEATS];
        int emptyBuses[MAX_BUSES];    // array of empty buses
        int numEmptyBuses; // empty buses are only formed after a merger
        int totalBuses;
        int busesExceedingLoad;
    };

    Data * sharedData;

    BusBooking(Data* sharedDataPtr) : sharedData(sharedDataPtr) 
    {
        initialize();
    }

    // BusBooking Constructor to initialize buses
    void initialize() 
    {
        for (int c = 0; c < INITIAL_BUSES; c++)
        {
            auto &bus = sharedData->buses[c];
            for (int i = 0; i < MAX_SEATS; i++)
            {
                bus.seats[i] = 1; // Initialize all seats as free
            }
            bus.bookedSeats = 0;
            bus.active = true;
            bus.loadExceeding = false;
        }
        sharedData->totalBuses = INITIAL_BUSES;
        sharedData->busesExceedingLoad = 0;
        sharedData->numEmptyBuses = 0;
    }

    // virtual ~BusBooking() {}

    // pair<int,vector<int>>clientSeats()

    void printer()
    {
        for (int i = 0; i < MAX_BUSES; i++)
        {
            auto bus = sharedData->buses[i];
            if (bus.active != true)
                continue;
            cout << "Seat Status, Bus ID: " << i << endl;
            for (auto y : bus.seats)
                cout << y << " ";
            cout << endl;
            cout << "Booked Seats: " << bus.bookedSeats << endl;
        }
        cout << "Total Buses: " << (sharedData->totalBuses) << endl;
        cout << "No. of empty buses(post merger formed): " << (sharedData->numEmptyBuses) << endl;
        cout << "Empty Buses (post merger): " << endl;
        for (int i = 0; i < (sharedData->numEmptyBuses); i++)
            cout << sharedData->emptyBuses[i] << " ";
        cout << endl;
        cout << "Buses exceeding load: " << (sharedData->busesExceedingLoad) << endl;
    }

    void seatsClient(int clientID){
        auto &client = sharedData->clients[clientID];
        for(int busID = 0; busID < MAX_BUSES; busID++){
            if(client.busesBooked[busID] == 1){
                cout << "BusID " << busID << " : ";
                for(int seatNo = 0; seatNo < MAX_SEATS; seatNo++){
                    if(client.seatsBooked[busID][seatNo] == 1){
                        cout<< seatNo<< " ";
                    }
                }

            }
        }
    }
    int clientDetails(int clientID)
    {
        cout << "Details of Client " << clientID << endl;
        cout << "Seats Booked: " << endl;
        auto &client = sharedData->clients[clientID];
        for(int busID = 0; busID < MAX_BUSES; busID++){
            if(client.busesBooked[busID] == 1){
                cout << "BusID " << busID << " : ";
                for(int seatNo = 0; seatNo < MAX_SEATS; seatNo++){
                    if(client.seatsBooked[busID][seatNo] == 1){
                        cout<< seatNo<< " ";
                    }
                }
                cout<<endl;
            }
        }
        cout << endl;
        cout << "Seats Selected: " << endl;
        for(int busID = 0; busID < MAX_BUSES; busID++){
            if(client.busesSelected[busID] == 1){
                cout << "BusID " << busID << " : ";
                for(int seatNo = 0; seatNo < MAX_SEATS; seatNo++){
                    if(client.seatsSelected[busID][seatNo] == 1){
                        cout<< seatNo<< " ";
                    }
                }
                cout<<endl;
            }
        }
        cout << endl;
        logWriter("SUCCESS CLIENT_DETAILS Client ID " + to_string(clientID) + " details printed");
        return CLIENT_SUCCESS;
    }

    double loadFactor(int busID)
    {
        double load = (double)sharedData->buses[busID].bookedSeats / MAX_SEATS;
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
        logWriter("TRIGGERED BUSES ADDITION on overload");
        int busesExpansion = BUSES_TO_BE_ADDED - sharedData->numEmptyBuses;
        if (busesExpansion > 0)
        {
            //
            for (int bus = 0; bus < sharedData->numEmptyBuses; bus++)
            {
                int busid = sharedData->emptyBuses[bus];
                for (int i = 0; i < MAX_SEATS; i++)
                {
                    sharedData->buses[busid].seats[i] = 1;
                }
                sharedData->buses[busid].active = true;
                sharedData->buses[busid].bookedSeats = 0;
                sharedData->buses[busid].loadExceeding = false;
            }
            (sharedData->totalBuses) += (sharedData->numEmptyBuses);
            sharedData->numEmptyBuses = 0;
            for (int busesToBeAdded = 0; busesToBeAdded < busesExpansion; busesToBeAdded++)
            {
                for (int i = 0; i < MAX_SEATS; i++)
                {
                    sharedData->buses[(sharedData->totalBuses)].seats[i] = 1;
                }
                sharedData->buses[(sharedData->totalBuses)].active = true;
                sharedData->buses[(sharedData->totalBuses)].bookedSeats = 0;
                sharedData->buses[(sharedData->totalBuses)].loadExceeding = false;
                (sharedData->totalBuses)++;
            }
        }
        else
        {
            for (int bus = 0; bus < BUSES_TO_BE_ADDED; bus++)
            {
                int busid = sharedData->emptyBuses[bus];
                for (int i = 0; i < MAX_SEATS; i++)
                {
                    sharedData->buses[busid].seats[i] = 1;
                }
                sharedData->buses[busid].active = true;
                sharedData->buses[busid].bookedSeats = 0;
                sharedData->buses[busid].loadExceeding = false;
                (sharedData->numEmptyBuses)--;
            }
            sharedData->totalBuses += BUSES_TO_BE_ADDED;
        }
        logWriter("SUCCESS BUSES ADDED " + to_string(BUSES_TO_BE_ADDED) + " buses added");
    }

    void loadChanger(int busID)
    {
        double load = loadFactor(busID);
        cout << "Load: " << load << endl;
        if ((!sharedData->buses[busID].loadExceeding) && checkLoadExceeds(load)) // during book
        {
            sharedData->buses[busID].loadExceeding = true;
            ++(sharedData->busesExceedingLoad);
        }
        else if (sharedData->buses[busID].loadExceeding && (!checkLoadExceeds(load))) // during cancel
        {
            sharedData->buses[busID].loadExceeding = false;
            --(sharedData->busesExceedingLoad);
        }
        if ((sharedData->busesExceedingLoad) == (sharedData->totalBuses))
        {
            busAdder();
        }
        logWriter("SUCCESS LOAD CHANGED FOR Bus ID " + to_string(busID));
    }

    vector<pair<int, int>> mergerPossiblities()
    {
        // returns bus ids of possible buses which can be merged
        logWriter("TRIGGERED MERGE POSSIBILITIES");
        vector<pair<int, int>> possibleBuses;
        for (int bus1 = 0; bus1 < MAX_BUSES - 1; bus1++)
        {
            for (int bus2 = bus1 + 1; sharedData->buses[bus1].active && bus2 < MAX_BUSES; bus2++)
            {
                if (!sharedData->buses[bus2].active)
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
                    if ((sharedData->buses[bus1].seats[seat] == 0 && sharedData->buses[bus2].seats[seat] == 0))
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
        logWriter("SUCCESS RETURNING POSSIBLE BUSES FOR MERGER");
        return possibleBuses;
    }

    int countSeats(int *arr){
        int c = 0;
        for(int i = 0;i < MAX_SEATS; i++){
            if(arr[i] == 1) c++;
        }
        return c;
    }

    bool merge(int bus1, int bus2, vector<pair<int, int>> &possibilities)
    {
        logWriter("TRIGGERED MERGER FOR Bus ID " + to_string(bus1) + " and Bus ID" + to_string(bus2));
        if (bus2 < bus1)
        {
            swap(bus1, bus2); // merging into a smaller bus id
        }
        bool pairFound = false;
        for (auto &pairs : possibilities)
        {
            cout << "Poss: " << pairs.first << " " << pairs.second << endl;
            if (pairs.first == bus1 && pairs.second == bus2)
            {
                pairFound = true;
                break;
            }
        }
        if (!pairFound)
        {
            logWriter("FAILED MERGER FOR Bus ID " + to_string(bus1) + " and Bus ID" + to_string(bus2) + " invalid bus ids");
            return false;
        }
        for (int seat = 0; seat < MAX_SEATS; seat++)
        {
            if (sharedData->buses[bus1].seats[seat] != 0 && sharedData->buses[bus2].seats[seat] == 0)
            {
                sharedData->buses[bus1].seats[seat] = 0;
                int clientID = sharedData->seatToClientMap[bus2][seat];

                sharedData->seatToClientMap[bus1][seat] = clientID;
                sharedData->clients[clientID].seatsBooked[bus1][seat] = 1;

                sharedData->seatToClientMap[bus2][seat] = -1;
                sharedData->clients[clientID].seatsBooked[bus2][seat] = -1;

            }
        }
        sharedData->clients->busesBooked[bus2] = -1;
        sharedData->emptyBuses[(sharedData->numEmptyBuses)++] = bus2;
        sharedData->buses[bus2].active = false;
        (sharedData->totalBuses)--;
        logWriter("SUCCESS MERGER FOR Bus ID " + to_string(bus1) + " and Bus ID" + to_string(bus2));
        return true;
    }

    bool validSeat(int busID, int seatNo)
    {
        if (seatNo >= MAX_SEATS || !sharedData->buses[busID].active)
        {
            return false;
        }
        return true;
    }

    int seatAvailability(int busID, int seatNo)
    {
        if (sharedData->buses[busID].seats[seatNo] == 0)
            return 0; // seat booked
        if (sharedData->buses[busID].seats[seatNo] == 1)
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
        logWriter("TRIGGERED BOOKING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID));
        int validSeatClientStatus = validSeatClient(busID, seatNo, clientID);
        if (validSeatClientStatus == CLIENT_INFO_INVALID_CLIENT || validSeatClientStatus == CLIENT_INFO_INVALID_SEAT_BUS)
        {
            logWriter("FAILED BOOKING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "INVALID SEAT/BUS");
            return validSeatClientStatus;
        }
        int seatStatus = seatAvailability(busID, seatNo);
        if (seatStatus == 0)
        {
            cout << "Seat already booked" << endl;
            logWriter("FAILED BOOKING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT BOOKED ALREADY");
            return CLIENT_INFO_SEAT_BOOKED_ALREADY;
        }

        else if (seatStatus == 2 && sharedData->clients[clientID].seatsSelected[busID][seatNo] == -1)
        {
            cout << "Seat selected by someone else" << endl;
            logWriter("FAILED BOOKING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT SELECTED ALREADY");
            return CLIENT_INFO_SEAT_NOT_SELECTED_BY_CLIENT;
        }

        sharedData->buses[busID].seats[seatNo] = 0;
        ++sharedData->buses[busID].bookedSeats;
        cout << "Booked Seats: " << sharedData->buses[busID].bookedSeats << endl;
        sharedData->clients[clientID].seatsBooked[busID][seatNo] = 1;
        if (sharedData->clients[clientID].seatsSelected[busID][seatNo] == 1)
        {
            sharedData->clients[clientID].seatsSelected[busID][seatNo] = -1;
        }
        if (countSeats(sharedData->clients[clientID].seatsSelected[busID]) == 0)
        {
            sharedData->clients[clientID].busesSelected[busID] = -1;
        }
        sharedData->seatToClientMap[busID][seatNo] = clientID;
        loadChanger(busID);
        cout << "Client" << clientID << " booked seat " << seatNo << " on bus id " << busID << endl;
        logWriter("SUCCESS BOOKING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID));
        return CLIENT_SUCCESS_SEAT_BOOKING;
    }

    int cancelSeat(int busID, int seatNo, int clientID)
    {
        int validSeatClientStatus = validSeatClient(busID, seatNo, clientID);
        if (validSeatClientStatus == CLIENT_INFO_INVALID_CLIENT || validSeatClientStatus == CLIENT_INFO_INVALID_SEAT_BUS)
        {
            logWriter("FAILED CANCELLING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "INVALID SEAT/BUS");
            return validSeatClientStatus;
        }

        int seatStatus = seatAvailability(busID, seatNo);
        if (seatStatus == 1)
        {
            cout << "Seat not booked" << endl;
            logWriter("FAILED CANCELLING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT NOT BOOKED");
            return CLIENT_INFO_SEAT_NOT_BOOKED;
        }
        if (seatStatus == 2 && sharedData->clients[clientID].seatsSelected[busID][seatNo] == -1)
        { // if locked, but u didnt do it
            cout << "Cannot cancel as Seat is not selected by you" << endl;
            logWriter("FAILED CANCELLING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT NOT SELECTED BY CLIENT");

            return CLIENT_INFO_SEAT_NOT_SELECTED_BY_CLIENT;
        }
        if (seatStatus == 0 && sharedData->clients[clientID].seatsBooked[busID][seatNo] == -1)
        { // if booked, but u didnt do it
            cout << "Cannot cancel Seat is not booked by you" << endl;
            logWriter("FAILED CANCELLING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT NOT BOOKED BY CLIENT");

            return CLIENT_INFO_SEAT_NOT_BOOKED_BY_CLIENT;
        }

        if (seatStatus == 0)
        {
            sharedData->clients[clientID].seatsBooked[busID][seatNo] = 1;
            if (countSeats(sharedData->clients[clientID].seatsBooked[busID]) == 0)
            {
                sharedData->clients[clientID].busesBooked[busID] = -1;
            }
            sharedData->seatToClientMap[busID][seatNo] = -1;
            --sharedData->buses[busID].bookedSeats;
            loadChanger(busID);
        }
        else if (seatStatus == 2)
        {
            sharedData->clients[clientID].seatsSelected[busID][seatNo] = -1;;
            if (countSeats(sharedData->clients[clientID].seatsSelected[busID]) == 0)
            {
                sharedData->clients[clientID].busesSelected[busID] = -1;
            }
            sharedData->seatToClientMap[busID][seatNo] = -1;
        }
        sharedData->buses[busID].seats[seatNo] = 1;
        cout << "Client" << clientID << " cancelled seat " << seatNo << " on bus id " << busID << endl;
        logWriter("SUCCESS CANCELLING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID));

        return CLIENT_SUCCESS_SEAT_CANCEL;
    }

    int selectSeat(int busID, int seatNo, int clientID)
    {
        int validSeatClientStatus = validSeatClient(busID, seatNo, clientID);
        if (validSeatClientStatus == CLIENT_INFO_INVALID_CLIENT || validSeatClientStatus == CLIENT_INFO_INVALID_SEAT_BUS)
        {
            logWriter("FAILED SELECTING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "INVALID SEAT/BUS");
            return validSeatClientStatus;
        }

        int seatStatus = seatAvailability(busID, seatNo);
        if (seatStatus == 0)
        {
            cout << "Seat already booked" << endl;
            logWriter("FAILED SELECTING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT BOOKED ALREADY");

            return CLIENT_INFO_SEAT_BOOKED_ALREADY;
        }

        if (seatStatus == 2 && sharedData->clients[clientID].seatsSelected[busID][seatNo] == -1)
        {
            cout << "Seat selected by someone else" << endl;
            logWriter("FAILED SELECTING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT SELECTED ALREADY");

            return CLIENT_INFO_SEAT_NOT_SELECTED_BY_CLIENT;
        }
        if (seatStatus == 2 && sharedData->clients[clientID].seatsSelected[busID][seatNo] == 1)
        {
            cout << "Already selected by you !!" << endl;
            logWriter("FAILED SELECTING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT ALREADY SELECTED BY CLIENT");

            return CLIENT_INFO_SEAT_ALREADY_SELECTED_BY_CLIENT;
        }
        sharedData->buses[busID].seats[seatNo] = 2;
        sharedData->buses[busID].seatLockDurations[seatNo] = chrono::steady_clock::now();
        sharedData->clients[clientID].seatsSelected[busID][seatNo] = 1;
        sharedData->seatToClientMap[busID][seatNo] = clientID;
        cout << "Client" << clientID << " selected seat " << seatNo << " on bus id " << busID << endl;
        logWriter("SUCCESS SELECTING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID));

        return CLIENT_SUCCESS_SEAT_SELECT;
    }

    void timerEvents()
    {
        auto now = chrono::steady_clock::now();
        for (int bus = 0; bus < MAX_BUSES; ++bus)
        {
            for (int seat = 0; sharedData->buses[bus].active && seat < MAX_SEATS; ++seat)
            {
                if (sharedData->buses[bus].seats[seat] == 2)
                {
                    auto duration = chrono::duration_cast<chrono::seconds>(now - sharedData->buses[bus].seatLockDurations[seat]).count();
                    if (duration >= TIMER)
                    {
                        sharedData->buses[bus].seats[seat] = 1;
                        int clientID = sharedData->seatToClientMap[bus][seat];
                        sharedData->clients[clientID].seatsSelected[bus][seat] = -1;
                        if (countSeats(sharedData->clients[clientID].seatsSelected[bus]) == 0)
                        {
                            sharedData->clients[clientID].busesSelected[bus] = -1;
                        }
                        cout << "Seat Released for seat " << seat << " on bus id " << bus << endl;
                        logWriter("SUCCESS AUTO SEAT RELEASE Bus ID " + to_string(bus) + " Seat No. " + to_string(seat) + " Client ID" + to_string(clientID));
                    }
                }
            }
        }
    }
};

class Server : public BusBooking
{
public:
    mutex mtx;

protected:
    int serveClient(int busID, int seatNo, int clientID, string action)
    {
        logWriter("TRIGGERED SERVE CLIENT Action: " + action + " Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID));

        int status = 0;
        if (action == "select")
        {
            lock_guard<mutex> lock(mtx);
            status = selectSeat(busID, seatNo, clientID);
        }
        else if (action == "book")
        {
            lock_guard<mutex> lock(mtx);
            status = bookSeat(busID, seatNo, clientID);
        }
        else if (action == "cancel")
        {
            lock_guard<mutex> lock(mtx);
            status = cancelSeat(busID, seatNo, clientID);
        }
        else if (action == "details")
        {
            lock_guard<mutex> lock(mtx);
            status = clientDetails(clientID);
            printer();
        }
        logWriter("SUCCESS SERVE CLIENT Action: " + action + " Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID));
        return status;
    }

public:
    Server(Data * sharedData) : BusBooking(sharedData){};
    void serveAdminSocket(int socket_fd)
    {
        logWriter("TRIGGERED SERVE ADMIN SOCKET socket_fd: " + to_string(socket_fd));

        char buffer[1024] = {0};
        cout << "Admin Connected: " << socket_fd << endl;
        string response = "Admin Login Success\n";
        send(socket_fd, response.c_str(), response.size(), 0);
        logWriter("SUCCESS ADMIN CONNECTED");

        int bytes_received = recv(socket_fd, buffer, 1024, 0); // Get action (show or merge)
        if (bytes_received <= 0)
        {
            cout << "Admin disconnected or error occurred." << endl;
            close(socket_fd);
            logWriter("CLOSE/ERROR ADMIN LEFT");
            return; // Exit the function, but don't stop the server
        }
        while (true)
        {
            char actionBuffer[1024] = {0};
            int bus1, bus2;
            sscanf(buffer, "%s %d %d", actionBuffer, &bus1, &bus2);
            string action = actionBuffer;

            vector<pair<int, int>> mergePossibilityVector;
            {
                lock_guard<mutex> lock(mtx);
                mergePossibilityVector = mergerPossiblities();
            }

            if (action == "show")
            {
                logWriter("TRIGGERED ADMIN ACTION show");
                cout << "Merger Possibility Called !!" << endl;
                string mergePossibility;
                for (auto &pairs : mergePossibilityVector)
                {
                    mergePossibility += "Bus " + to_string(pairs.first) + " and Bus " + to_string(pairs.second) + "\n";
                }
                cout << "Possibility: " << endl;
                cout << mergePossibility << endl;
                string response = mergePossibility;
                if (response.empty())
                {
                    response = "No mergers possible\n";
                }
                cout << response << endl;
                send(socket_fd, response.c_str(), response.size(), 0);
                logWriter("SUCCESS ADMIN ACTION show response returned");
            }
            else if (action == "merge")
            {
                logWriter("TRIGGERED ADMIN ACTION merge");
                string response;
                cout << "Merger Called for " << bus1 << " and " << bus2 << endl;
                if (bus1 >= 0 && bus1 < MAX_BUSES && bus2 >= 0 && bus2 < MAX_BUSES)
                {
                    bool mergeStatus = merge(bus1, bus2, mergePossibilityVector);
                    if (mergeStatus)
                    {
                        response = "Merger Complete\n";
                        logWriter("SUCCESS ADMIN ACTION merge FOR Bus ID " + to_string(bus1) + " and Bus ID " + to_string(bus2));
                    }
                    else
                    {
                        response = "Merge Invalid, check possible buses again !!\n";
                        logWriter("FAILED ADMIN ACTION merge FOR Bus ID " + to_string(bus1) + " and Bus ID " + to_string(bus2));
                    }
                }
                else
                {
                    response = "Invalid Buses\n";
                    logWriter("FAILED ADMIN ACTION merge FOR Bus ID " + to_string(bus1) + " and Bus ID " + to_string(bus2) + " INVALID BUSES");
                }
                send(socket_fd, response.c_str(), response.size(), 0);
            }
            else
            {
                string response = "Invalid Admin Action\n";
                send(socket_fd, response.c_str(), response.size(), 0);
                logWriter("FAILED ADMIN ACTION INVALID ACTION");
            }
            bytes_received = recv(socket_fd, buffer, 1024, 0);
            if (bytes_received <= 0)
            {
                cout << "Admin disconnected or error occurred during action." << endl;
                close(socket_fd);
                logWriter("CLOSE/ERROR ADMIN LEFT");
                return;
            }
        }
        close(socket_fd);
        logWriter("CLOSE/ERROR ADMIN LEFT");
    }
    void serveClientSocket(int socket_fd)
    {
        logWriter("TRIGGERED SERVE CLIENT SOCKET socket_fd: " + to_string(socket_fd));
        char buffer[1024] = {0};
        int clientID;
        cout << "Client Connected: " << socket_fd << endl;
        logWriter("SUCCESS CLIENT CONNECTED");
        string response = "Mode selection Success\n";
        send(socket_fd, response.c_str(), response.size(), 0);

        int bytes_received = recv(socket_fd, buffer, 1024, 0); // getting client ID
        if (bytes_received <= 0)
        {
            cout << "Client disconnected or error occurred." << endl;
            close(socket_fd);
            logWriter("CLOSE/ERROR CLIENT LEFT");
            return; // Exit the function, but don't stop the server
        }

        string clientIDString = string(buffer);
        cout << clientIDString << endl;
        try
        {
            clientID = stoi(clientIDString);
            if (clientID < 0 || clientID >= MAX_CLIENTS)
            {
                string response = "Enter correct client ID\n";
                send(socket_fd, response.c_str(), response.size(), 0);
                logWriter("FAILED INVALID CLIENT ID " + clientIDString);
                return; // Invalid client, close this connection
            }
            else
            {
                string response = "Client Login Success\n";
                send(socket_fd, response.c_str(), response.size(), 0);
                logWriter("SUCCESS CLIENT LOGIN Client ID " + clientIDString);
            }
        }
        catch (const exception &e)
        {
            string response = "Invalid Input\n";
            send(socket_fd, response.c_str(), response.size(), 0);
            close(socket_fd);
            logWriter("FAILED INVALID INPUT");
            return;
        }

        while (true)
        {
            memset(buffer, 0, sizeof(buffer)); // Clear buffer
            bytes_received = recv(socket_fd, buffer, sizeof(buffer), 0);

            if (bytes_received <= 0)
            {
                cout << "Client disconnected or error occurred during action." << endl;
                close(socket_fd);
                logWriter("CLOSE/ERROR CLIENT LEFT");
                return;
            }

            string action;
            char actionBuffer[1024];
            int busID, seatNo;
            sscanf(buffer, "%s %d %d", actionBuffer, &busID, &seatNo);
            action = actionBuffer;
            if (action != "select" && action != "book" && action != "cancel" && action != "details")
            {
                string response = "Invalid Action\n";
                send(socket_fd, response.c_str(), response.size(), 0);
                logWriter("FAILED INVALID CLIENT ACTION action: " + action);
                continue;
            }

            cout << "Action: " << action << " " << busID << " " << seatNo << endl;
            int serverReponse = serveClient(busID, seatNo, clientID, action);
            string response = to_string(serverReponse) + "\n";
            send(socket_fd, response.c_str(), response.size(), 0);
            logWriter("SUCCESS SENDING RESPONSE TO CLIENT");
        }
        close(socket_fd);
        logWriter("CLOSE/ERROR CLIENT LEFT");
    }
};
int main()
{
    size_t SHARED_MEMORY_SIZE = sizeof(BusBooking::Data);
    int fd = shm_open("/shared_data", O_CREAT | O_RDWR, 0666);
    if (fd == -1)
    {
        std::cerr << "Failed to create shared memory!" << std::endl;
        return 1;
    }

    // Set the size of the shared memory
    if (ftruncate(fd, SHARED_MEMORY_SIZE) == -1)
    {
        std::cerr << "Failed to set the size of shared memory!" << std::endl;
        return 1;
    }

    // Map the shared memory to process address space
    BusBooking::Data *sharedData = static_cast<BusBooking::Data *>(mmap(nullptr, SHARED_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (sharedData == MAP_FAILED)
    {
        std::cerr << "Failed to map shared memory!" << std::endl;
        return 1;
    }

    // Initialize the BusBooking class with shared memory
    Server server(sharedData);

    thread loggerThread([]()
                        {
        while (true) {
            logFileWriter(); // calling log func every 3 seconds to log all activities
            std::this_thread::sleep_for(std::chrono::seconds(3)); 
        } });
    loggerThread.detach();

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        logWriter("FAILED SOCKET FILE DESCRIPTOR");
        exit(EXIT_FAILURE);
    }
    logWriter("SUCCESS SOCKET FILE DESCRIPTOR");

    // Binding socket to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        logWriter("FAILED BIND SOCKET TO PORT " + PORT);
        exit(EXIT_FAILURE);
    }

    logWriter("SUCCESS BIND SOCKET TO PORT " + PORT);

    // Listening on the socket
    if (listen(server_fd, 100) < 0)
    {
        perror("listen");
        logWriter("SUCCESS LISTENING ON SOCKET");
        exit(EXIT_FAILURE);
    }

    logWriter("SUCCESS LISTENING ON SOCKET");

    cout << "BUS BOOKING SYSTEM SERVER RUNNING" << endl;


    // Timer thread for periodic server tasks
    thread timerThread([&server]()
                       {
        while (true) {
            server.timerEvents();
            std::this_thread::sleep_for(std::chrono::seconds(3)); 
        } });
    logWriter("SUCCESS TIMER THREAD CREATED");

    timerThread.detach();

    while (true)
    {
        cout << "Waiting for connections ...." << endl;

        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket < 0)
        {
            perror("accept");
            logWriter("FAILED CONNECTING TO CLIENT");
            continue; // Log the error and continue accepting other clients
        }

        cout << "Connection Accepted" << endl;
        logWriter("SUCCESS CLIENT CONNECTED");

        // Fork a new process for each client connection
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork failed");
            logWriter("FAILED TO FORK");
            close(new_socket);
            continue;
        }
        else if (pid == 0)
        {
            // Child process: handle client request
            close(server_fd); // Child doesn't need the listening socket

            char buffer[1024] = {0};
            int bytes_received = recv(new_socket, buffer, 1024, 0);
            string mode = buffer;

            if (mode == "admin")
            {
                server.serveAdminSocket(new_socket);
                logWriter("SUCCESS ADMIN PROCESS HANDLING");
            }
            else if (mode == "client")
            {
                server.serveClientSocket(new_socket);
                logWriter("SUCCESS CLIENT PROCESS HANDLING");
            }
            else
            {
                cout << "Invalid Mode: " << mode << endl;
                string response = "Invalid Mode\n";
                send(new_socket, response.c_str(), response.size(), 0);
                logWriter("FAILED INVALID MODE");
            }

            close(new_socket); // Close socket after handling the client
            logWriter("SUCCESS CLIENT CONNECTION CLOSED");
            exit(0); // Exit child process
        }
        else
        {
            // Parent process: continue to accept new connections
            close(new_socket); // Parent doesn't need this socket
        }
    }

    close(server_fd);
    logWriter("SERVER CLOSED");
}
