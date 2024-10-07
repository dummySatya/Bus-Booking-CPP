#include <bits/stdc++.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define MAX_BUSES 4
#define MAX_SEATS 5
#define TIMER 5
#define BUSES_TO_BE_ADDED 2
#define LOAD_EXCEED_FACTOR 0.8
#define LOAD_LESS_FACTOR 0.2

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
        bool active; // is false only when some merger has happened
        bool loadExceeding;  // to set the flag to true only when first time the bus exceedingm so that even if seat is booked on an overloaded bus, the busesExceeding wont again trigger and increment, also becomes false if load less than threshhold
    };

    Bus buses[MAX_BUSES];
    Client clients[MAX_CLIENTS];
    unordered_map <int,unordered_map<int,int>>seatToClientMap;
    int emptyBuses[MAX_BUSES]; // array of empty buses
    int *numEmptyBuses = new int; // empty buses are only formed after a merger
    int *totalBuses = new int;
    int *busesExceedingLoad = new int;
    // initializing it bcs it not would lead to segerr
public:
    BusBooking(int numBuses = 2) // initial no. of buses
    {
        for (auto &bus : buses)
        {
            for (int i = 0; i < MAX_SEATS; i++)
            {
                bus.seats[i] = 1;
            }
            bus.bookedSeats = 0;
            bus.active = true;
            bus.loadExceeding = false;
        }
        *totalBuses = numBuses;
        *busesExceedingLoad = 0;
        *numEmptyBuses = 0;
    }
    // virtual ~BusBooking() {}

    void printer()
    {
        for (int i = 0; i< MAX_BUSES; i++)
        {
            auto bus = buses[i];
            if(! bus.active) continue;
            cout << "Seat Status, Bus ID: " << i << endl;
            for (auto y : bus.seats)
                cout << y << " ";
            cout << endl;
            cout << "Booked Seats: " << bus.bookedSeats << endl;
        }
        cout << "Total Buses: " << (*totalBuses) << endl;
        cout<< "No. of empty buses(post merger formed): "<<(*numEmptyBuses)<<endl;
        cout<< "Empty Buses (post merger): "<<endl;
        for(int i=0 ; i< (*numEmptyBuses); i++) cout<<emptyBuses[i]<< " ";
        cout<<endl;
        cout<< "Buses exceeding load: "<<(*busesExceedingLoad)<<endl;
    }

    void clientDetails(int clientID){
        cout<<"Details of Client "<<clientID<<endl;
        auto client = clients[clientID];
        cout<<"Seats Booked: "<<endl;
        for(auto seatDetails: client.seatsBooked){
            cout<<"BusID "<<seatDetails.first<<" : ";
            for(auto seat: seatDetails.second){
                cout<<seat<<" ";
            }
            cout<<endl;
        }
        cout<<endl;
        cout<<"Seats Selected: "<<endl;
        for(auto seatDetails: client.seatsSelected){
            cout<<"BusID "<<seatDetails.first<<" : ";
            for(auto seat: seatDetails.second){
                cout<<seat<<" ";
            }
            cout<<endl;
        }
        cout<<endl;
    }
    
    double loadFactor(int busID)
    {
        double load = (double)buses[busID].bookedSeats / MAX_SEATS;
        return load;
    }

    bool checkLoadExceeds(double load)
    {
        if (load >= LOAD_EXCEED_FACTOR){
            return true;
        }
        return false;
    }

    bool checkLoadLessThan(double load){
        if(load <= LOAD_LESS_FACTOR){
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
                for(int i = 0;i < MAX_SEATS; i++){
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
                for(int i = 0;i < MAX_SEATS; i++){
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
        cout<<"Load: "<<load<<endl;
        if ((!buses[busID].loadExceeding) && checkLoadExceeds(load)) // during book
        {
            buses[busID].loadExceeding = true;
            ++(*busesExceedingLoad);
        }
        else if(buses[busID].loadExceeding && (!checkLoadExceeds(load))) // during cancel
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
            for (int bus2 = bus1 + 1;  buses[bus1].active && bus2 < MAX_BUSES; bus2++)
            {
                if(!buses[bus2].active) break;
                bool a = checkLoadLessThan(loadFactor(bus1));
                bool b = checkLoadLessThan(loadFactor(bus2));
                cout<<"Bus"<<bus1<<" bool load: "<<a<<endl;
                cout<<"Bus"<<bus2<<" bool load: "<<b<<endl;
                bool loadLessCondition = a && b;
                if(!loadLessCondition){
                    continue;
                }
                bool possible = true;
                for (int seat = 0; seat < MAX_SEATS; seat++)
                {
                    // not possible only if both seats are booked
                    if ((buses[bus1].seats[seat] == 0 && buses[bus2].seats[seat] == 0)  )
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

    bool merge(int bus1, int bus2)
    {
        if (bus2 < bus1)
        {
            swap(bus1, bus2); // merging into a smaller bus id
        }
        for (int seat = 0; seat < MAX_SEATS; seat++)
        {
            if(buses[bus1].seats[seat] !=0 && buses[bus2].seats[seat] == 0 ){
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
        (*totalBuses) --;
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
        if (buses[busID].seats[seatNo] == 0) return 0; // seat booked
        if (buses[busID].seats[seatNo] == 1) return 1; // seat free
        return 2; // seat selected
    }

    bool validClient(int clientID){
        if(clientID >= MAX_CLIENTS) return false;
        return true;
    }

    void bookSeat(int busID, int seatNo, int clientID)
    {
        if(!validClient(clientID)){
            cout<<"Enter a valid client ID less than "<<MAX_CLIENTS<<endl;
            return ;
        }
        if (!validSeat(busID, seatNo))
        {
            cout << "Invalid Bus or Seat" << endl;
            return;
        }

        int seatStatus = seatAvailability(busID, seatNo);
        if (seatStatus == 0)
        {
            cout << "Seat already booked" << endl;
            return;
        }

        else if(seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) == clients[clientID].seatsSelected[busID].end()){
            cout<<"Seat selected by someone else"<<endl;
            return ;
        }

        buses[busID].seats[seatNo] = 0;
        ++buses[busID].bookedSeats;
        cout<<"Booked Seats: "<<buses[busID].bookedSeats<<endl;
        clients[clientID].seatsBooked[busID].insert(seatNo);
        seatToClientMap[busID][seatNo] = clientID;
        loadChanger(busID);
        cout << "Client" << clientID << " booked seat " << seatNo << " on bus id " << busID << endl;
    }

    void cancelSeat(int busID, int seatNo, int clientID)
    {
        if(!validClient(clientID)){
            cout<<"Enter a valid client ID less than "<<MAX_CLIENTS<<endl;
            return ;
        }

        if (!validSeat(busID, seatNo))
        {
            cout << "Invalid Seat" << endl;
            return;
        }

        int seatStatus = seatAvailability(busID, seatNo);
        if (seatStatus == 1)
        {
            cout << "Seat not booked" << endl;
            return;
        }
        if(seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) == clients[clientID].seatsSelected[busID].end()){ // if locked, but u didnt do it
            cout<<"Cannot cancel as Seat is not selected by you"<<endl;
            return ;
        }
        if(seatStatus == 0 && clients[clientID].seatsBooked[busID].find(seatNo) == clients[clientID].seatsBooked[busID].end()){ // if booked, but u didnt do it
            cout<<"Cannot cancel Seat is not booked by you"<<endl;
            return ;
        }

        if(seatStatus == 0){
            clients[clientID].seatsBooked[busID].erase(seatNo);
            if(clients[clientID].seatsBooked[busID].size() == 0){
                clients[clientID].seatsBooked.erase(busID);
            }
            seatToClientMap[busID].erase(seatNo);
            --buses[busID].bookedSeats;
            loadChanger(busID);
        }
        else if(seatStatus == 2){
            clients[clientID].seatsSelected[busID].erase(seatNo);
            if(clients[clientID].seatsSelected[busID].size() == 0){
                clients[clientID].seatsSelected.erase(busID);
            }
            seatToClientMap[busID].erase(seatNo);
        }
        buses[busID].seats[seatNo] = 1;
        cout << "Client" << clientID << " cancelled seat " << seatNo << " on bus id " << busID << endl;
    }

    void selectSeat(int busID, int seatNo, int clientID)
    {
        if(!validClient(clientID)){
            cout<<"Enter a valid client ID less than "<<MAX_CLIENTS<<endl;
            return ;
        }

        if (!validSeat(busID, seatNo))
        {
            cout << "Invalid Seat" << endl;
            return;
        }

        int seatStatus = seatAvailability(busID, seatNo);
        if (seatStatus == 0)
        {
            cout << "Seat already booked" << endl;
            return;
        }

        if(seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) == clients[clientID].seatsSelected[busID].end()){
            cout<<"Seat selected by someone else"<<endl;
            return ;
        }
        if(seatStatus == 2 && clients[clientID].seatsSelected[busID].find(seatNo) != clients[clientID].seatsSelected[busID].end()){
            cout<<"Already selected by you !!"<<endl;
            return ;
        }
        buses[busID].seats[seatNo] = 2;
        buses[busID].seatLockDurations[seatNo] = chrono::steady_clock::now();
        clients[clientID].seatsSelected[busID].insert(seatNo);
        seatToClientMap[busID][seatNo] = clientID;
        cout << "Client" << clientID << " selected seat " << seatNo << " on bus id " << busID << endl;
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
                        if(clients[clientID].seatsSelected[bus].size() == 0){
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
protected:
    void serveClient(int busID, int seatNo, int clientID, string action)
    {
        if (action == "select")
        {
            selectSeat(busID, seatNo, clientID);
        }
        else if (action == "book")
        {
            bookSeat(busID, seatNo, clientID);
        }
        else if (action == "cancel")
        {
            cancelSeat(busID, seatNo, clientID);
        }
    }
public:
    void serveClients()
    {
        int status = 1;
        do
        {
            string action;
            int busID, seatNo, clientID;
            cout << "Enter action: " << endl;
            cin >> action;
            if (action != "select" && action != "book" && action != "cancel" && action != "details")
            {
                cout << "Invalid Action" << endl;
                continue;
            }
            if(action == "details"){
                printer();
                cout << "Enter 0 to exit or 1 to continue" << endl;
                cin >> status;
                continue;
            }
            cout << "Enter busID, seatNo, clientID" << endl;
            cin >> busID >> seatNo >> clientID;
            serveClient(busID, seatNo, clientID, action);
            cout << "Enter 0 to exit or 1 to continue" << endl;
            cin >> status;
        } while (status);
    }
};

class ThreadServing : public Server
{
    mutex mtx;

public:
    void serveClients()
    {
        int status = 1;
        int clientID;
        cout<<"Enter ClientID: ";
        cin>>clientID;
        do
        {
            string action;
            int busID, seatNo;
            cout << "Enter action: " << endl;
            cin >> action;
            if (action != "select" && action != "book" && action != "cancel" && action != "details")
            {
                cout << "Invalid Action" << endl;
                continue;
            }
            if(action == "details"){
                printer();
                clientDetails(clientID);
                cout << "Enter 0 to exit or 1 to continue" << endl;
                cin >> status;
                continue;
            }
            cout << "Enter busID, seatNo" << endl;
            cin >> busID >> seatNo;
            thread clientThread([this, busID, seatNo, clientID, action]()
                                {
                lock_guard<mutex>lock(mtx);
                serveClient(busID, seatNo, clientID, action); });

            clientThread.detach();
            cout << "Enter 0 to exit or 1 to continue" << endl;
            cin >> status;
        } while (status);
    }
};


int main()
{
    cout << "BUS BOOKING SYSTEM" << endl;
    Server server;
    int mode;
    do{
        cout<<"1. Admin"<<endl;
        cout<<"2. Client" << endl;
        cout<<"3. Exit" << endl;
        cout<<"Enter Mode: ";
        cin>>mode;
        if(mode == 1){
            cout<<"1. Show Possibilities\n2. Merge\n";
            int option;
            cin>>option;
            if(option == 1){
                vector<pair<int, int>> possibilities = server.mergerPossiblities();
                cout << "Possible merges:" << endl;
                for(auto &p : possibilities) {
                    cout << "Bus " << p.first << " and Bus " << p.second << endl;
                }
            }
            else if(option == 2){
                cout<<"Enter bus1 and bus2 to merge: ";
                int bus1,bus2;
                cin>>bus1>>bus2;
                bool status = server.merge(bus1, bus2);
                if(status){
                    cout<<"Bus Merger completed\n";
                }
                else{
                    cout<<"Bus Merger Error\n";
                }
            }
            else{
                cout<<"Invalid Option\n";
            }
        }
        else if(mode == 2){
            thread timerThread([&server]()
            {
                while (true) {
                    server.timerEvents();  // Check the seat status
                    std::this_thread::sleep_for(std::chrono::seconds(3));  // Adjust interval as needed
                } 
            });
            timerThread.detach(); // Detach the thread to allow it to run independently

            server.serveClients();
        }
        else if(mode != 3){
            cout<<"Invalid Mode\n";
            continue;
        }
    }while(mode != 3);
}