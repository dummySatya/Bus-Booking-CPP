#include <bits/stdc++.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
        logWriter("INITIALIZE BUSES");
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
        logWriter("SUCCESS CLIENT_DETAILS Client ID " + to_string(clientID) + " details printed");
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
        logWriter("TRIGGERED BUSES ADDITION on overload");
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
        logWriter("SUCCESS BUSES ADDED " + to_string(BUSES_TO_BE_ADDED) + " buses added");
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
        logWriter("SUCCESS LOAD CHANGED FOR Bus ID " + to_string(busID));
    }

    vector<pair<int, int>> mergerPossiblities()
    {
        // returns bus ids of possible buses which can be merged
        logWriter("TRIGGERED MERGE POSSIBILITIES");
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
        logWriter("SUCCESS RETURNING POSSIBLE BUSES FOR MERGER");
        return possibleBuses;
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
            if (buses[bus1].seats[seat] != 0 && buses[bus2].seats[seat] == 0)
            {
                buses[bus1].seats[seat] = 0;
                int clientID = seatToClientMap[bus2][seat];

                seatToClientMap[bus1][seat] = clientID;
                clients[clientID].seatsBooked[bus1].insert(seat);

                seatToClientMap[bus2].erase(seat);
                clients[clientID].seatsBooked[bus2].erase(seat);
                if (clients[clientID].seatsBooked[bus2].size() == 0)
                {
                    clients[clientID].seatsBooked.erase(bus2);
                }
            }
        }
        emptyBuses[(*numEmptyBuses)++] = bus2;
        buses[bus2].active = false;
        (*totalBuses)--;
        logWriter("SUCCESS MERGER FOR Bus ID " + to_string(bus1) + " and Bus ID" + to_string(bus2));
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

        else if (seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) == clients[clientID].seatsSelected[busID].end())
        {
            cout << "Seat selected by someone else" << endl;
            logWriter("FAILED BOOKING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT SELECTED ALREADY");
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
        if (seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) == clients[clientID].seatsSelected[busID].end())
        { // if locked, but u didnt do it
            cout << "Cannot cancel as Seat is not selected by you" << endl;
            logWriter("FAILED CANCELLING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT NOT SELECTED BY CLIENT");

            return CLIENT_INFO_SEAT_NOT_SELECTED_BY_CLIENT;
        }
        if (seatStatus == 0 && clients[clientID].seatsBooked[busID].find(seatNo) == clients[clientID].seatsBooked[busID].end())
        { // if booked, but u didnt do it
            cout << "Cannot cancel Seat is not booked by you" << endl;
            logWriter("FAILED CANCELLING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT NOT BOOKED BY CLIENT");

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

        if (seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) == clients[clientID].seatsSelected[busID].end())
        {
            cout << "Seat selected by someone else" << endl;
            logWriter("FAILED SELECTING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT SELECTED ALREADY");

            return CLIENT_INFO_SEAT_NOT_SELECTED_BY_CLIENT;
        }
        if (seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) != clients[clientID].seatsSelected[busID].end())
        {
            cout << "Already selected by you !!" << endl;
            logWriter("FAILED SELECTING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID) + "SEAT ALREADY SELECTED BY CLIENT");

            return CLIENT_INFO_SEAT_ALREADY_SELECTED_BY_CLIENT;
        }
        buses[busID].seats[seatNo] = 2;
        buses[busID].seatLockDurations[seatNo] = chrono::steady_clock::now();
        clients[clientID].seatsSelected[busID].insert(seatNo);
        seatToClientMap[busID][seatNo] = clientID;
        cout << "Client" << clientID << " selected seat " << seatNo << " on bus id " << busID << endl;
        logWriter("SUCCESS SELECTING Bus ID " + to_string(busID) + " Seat No. " + to_string(seatNo) + " Client ID" + to_string(clientID));

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

    Server server;

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
            continue; // Instead of exiting, log the error and continue accepting other clients
        }

        cout << "Connection Accepted" << endl;
        logWriter("SUCCESS CLIENT CONNECTED");

        char buffer[1024] = {0};
        int bytes_received = recv(new_socket, buffer, 1024, 0);
        string mode = buffer;
        if (mode == "admin")
        {
            server.serveAdminSocket(new_socket);
        }
        else if (mode == "client")
        {
            server.serveClientSocket(new_socket);
        }
        else
        {
            cout<<"Invalid Mode: "<<mode<<endl;
            string response = "Invalid Mode\n";
            send(new_socket, response.c_str(), response.size(), 0);
            logWriter("FAILED INVALID MODE");
        }
    }

    close(server_fd);
    logWriter("SERVER CLOSED");
}
