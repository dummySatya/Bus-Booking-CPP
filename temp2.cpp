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
    //socket connection with client
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
            std::thread adminThread([&server, new_socket]()
                                    { server.serveAdminSocket(new_socket); });
            logWriter("SUCCESS ADMIN THREAD CREATED");
            adminThread.detach();
        }
        else if (mode == "client")
        {
            std::thread clientThread([&server, new_socket]()
                                     { server.serveClientSocket(new_socket); });
            logWriter("SUCCESS CLIENT THREAD CREATED");
            clientThread.detach(); // Detach to handle clients asynchronously
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

now i have written this server which uses thread to handle each client, this variables has no problem bcs it can handle asynchrounslly and also have the varibles present
but now i want to make it as a forked server where i fork for each client and things get handled inside that 
but for this, the variables(struct bus, client and other variables) must be shared across the process
i need to use shared memory for this
now ive used maps and all in this, which is dynamic, so mmap wont be working here so i want to use boost cpp 
now convert this to forked server