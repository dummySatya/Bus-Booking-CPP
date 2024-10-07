#include <bits/stdc++.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CLIENTS 10
#define MAX_BUSES 100
#define MAX_SEATS 40
#define TIMER 10 // 10 seconds
#define BUSES_TO_BE_ADDED 2

using namespace std;

class BusBooking
{
protected:
    struct Client
    {
        string clientID;
        unordered_map<int,set<int>>seatsBooked; // bus:seats mapping
        // chose set over vector bcs deletion is much more costlier in vector
        unordered_map<int,set<int>>seatsSelected;
    };
    struct Bus
    {
        int seats[MAX_SEATS]; // 0 booked, 1 freee, 2 locked
        chrono::time_point<chrono::steady_clock> seatLockDurations[MAX_SEATS];
        int bookedSeats;
        bool active; // is false only when some merger has happened
    };
    // Bus buses[MAX_BUSES];
    // Client clients[MAX_CLIENTS];
    vector<Bus>buses;
    vector<Client> clients;

    int emptyBuses[MAX_BUSES] ; // array of empty buses
    int *numEmptyBuses = new int;
    int * totalBuses = new int;
    int * busesExceedingLoad = new int;
    // initializing it bcs it not would lead to segerr
public:
    BusBooking(int numBuses = 10) //initial no. of buses
    {
        buses.resize(numBuses);
        for (auto &bus : buses)
        {
            bus.bookedSeats = 0;
            bus.active = true;
        }
        *totalBuses = numBuses;
        *busesExceedingLoad = 0;
        *numEmptyBuses = 0;
    }
    // virtual ~BusBooking() {}

    double loadFactor(int busID){
        double load = buses[busID].bookedSeats / MAX_SEATS;
        return load;
    }

    bool checkLoadExceeds(double load){
        if(load >= 0.8){
            return true;
        }
        return false;
    }

    void busAdder(){
        int busesExpansion = BUSES_TO_BE_ADDED - (*emptyBuses).size();
        if(busesExpansion > 0){
            for(int busesToBeAdded = 0 ;busesToBeAdded < busesExpansion; busesToBeAdded++){
                buses.emplace_back();
                buses.back().active = true;
                buses.back().bookedSeats = 0;
                *totalBuses ++;

            }
            *totalBuses += busesExpansion;
        }
        else{
            for(int busesToBeAdded = 0 ;busesToBeAdded < BUSES_TO_BE_ADDED; busesToBeAdded++){
                int busid = (*emptyBuses).front();
                emptyBuses = emptyBuses + 1;
                buses[busid].seats.resize(MAX_SEATS,true);
                buses[busid].seatLockDurations.resize(MAX_SEATS);
                buses[busid].active = true;
                buses[busid].bookedSeats = 0;
            }
            *totalBuses += BUSES_TO_BE_ADDED;
        }
    }

    void loadChanger(int busID){
        double load = loadFactor(busID);;
        if(checkLoadExceeds(load)){
            ++ *busesExceedingLoad;
        }
        if(*busesExceedingLoad == *totalBuses){
            busAdder();
        }
    }

    vector<pair<int,int>> mergerPossiblities(){
        // returns bus ids of possible buses which can be merged
        vector<pair<int,int>>possibleBuses;
        for(int bus1 = 0; bus1 < buses.size() - 1; bus1++){
            for(int bus2 = bus1 + 1; buses[bus1].active && bus2 < buses.size(); bus2++ ){
                bool possible = true;
                for(int seat = 0; seat < MAX_SEATS; seat++){
                    if(! (buses[bus1].seats[seat] || buses[bus1].seats[seat])){
                        possible = false;
                        break;
                    }
                }
                if(possible){
                    possibleBuses.push_back({bus1,bus2});
                }
            }
        }
        return possibleBuses;
    }

    bool merge(int bus1, int bus2){
        if(bus2<bus1) {
            swap(bus1,bus2); // merging into a smaller bus id
        }
        for(int seat = 0; seat < MAX_SEATS; seat++){
            buses[bus1].seats[seat] = buses[bus1].seats[seat] || buses[bus2].seats[seat];
        }
        (*emptyBuses).push_back(bus2);
        buses[bus2].active = false;
    }

    bool validSeat(int busID, int seatNo){
        if(busID >= buses.size() || seatNo >= MAX_SEATS || ! buses[busID].active){
            return false;
        }
        return true;
    }
    
    bool seatAvailability(int busID, int seatNo){
        if(! buses[busID].seats[seatNo]){
            return false; // seat not available
        }
        return true; // seat available
    }

    void bookSeat(int busID, int seatNo, int clientID){
        if(! validSeat(busID, seatNo)){
            cout<<"Invalid Seat"<<endl;
            return ;
        }
        if(! seatAvailability(busID, seatNo)){
            cout<<"Seat already booked"<<endl;
            return ;
        }
        buses[busID].seats[seatNo] = false;
        buses[busID].seatLockDurations[seatNo] = chrono::steady_clock::now();
        ++ buses[busID].bookedSeats;
        loadChanger(busID);
        cout<< "Client"<< clientID << " booked seat "<< seatNo<<" on bus id "<<busID<<endl;
    }

    void cancelSeat(int busID, int seatNo, int clientID){
        if(! validSeat(busID, seatNo)){
            cout<<"Invalid Seat"<<endl;
            return ;
        }

        if(seatAvailability(busID, seatNo)){
            cout<<"Seat not booked"<<endl;
            return ;
        }
        buses[busID].seats[seatNo] = true;
        -- buses[busID].bookedSeats;
        loadChanger(busID);
        cout<< "Client"<< clientID << " cancelled seat "<< seatNo<<" on bus id "<<busID<<endl;
    }

    void selectSeat(int busID, int seatNo, int clientID){
        if(! validSeat(busID, seatNo)){
            cout<<"Invalid Seat"<<endl;
            return ;
        }
        if(! seatAvailability(busID, seatNo)){
            cout<<"Seat already booked"<<endl;
            return ;
        }
        buses[busID].seats[seatNo] = false;
        cout<< "Client"<< clientID << " selected seat "<< seatNo<<" on bus id "<<busID<<endl;
    }

    void timerEvents(){
        auto now = chrono::steady_clock::now();
        for(int bus = 0;bus < buses.size(); ++bus){
            for(int seat = 0; seat < MAX_SEATS; ++seat){
                if(!buses[bus].seats[seat]){
                    auto duration = chrono::duration_cast<chrono::seconds>(now - buses[bus].seatLockDurations[seat]).count();
                    if(duration >= TIMER){
                        buses[bus].seats[seat] = true;
                        cout<< "Seat Released for seat " << seat<<" on bus id "<<bus<<endl;
                    }
                }
            }
        }
    }
};

class Serving : public BusBooking
{
protected:
    void serveClient(int busID, int seatNo, int clientID, string action){
        if(action == "select"){
            selectSeat(busID, seatNo, clientID);
        }
        else if(action == "book"){
            bookSeat(busID, seatNo, clientID);
        }
        else if(action == "cancel"){
            cancelSeat(busID, seatNo, clientID);
        }
    }    
};

class IterativeServing : public Serving
{
public:
    void serveClients(){
        int status = 1;
        do{
            string action;
            int busID, seatNo, clientID;
            cout<<"Enter action: "<<endl;
            cin>>action;
            cout<<"Enter busID, seatNo, clientID"<<endl;
            cin>>busID>>seatNo>>clientID;
            serveClient(busID, seatNo, clientID, action);
            cout<<"Enter 0 to exit or 1 to continue"<<endl;
            cin>>status;
        }   
        while(status);
    }    
};

class ThreadServing : public Serving
{
    mutex mtx;
public:
    void serveClients(){
        int status = 1;
        do{
            string action;
            int busID, seatNo, clientID;
            cout<<"Enter action: "<<endl;
            cin>>action;
            cout<<"Enter busID, seatNo, clientID"<<endl;
            cin>>busID>>seatNo>>clientID;
            thread clientThread([this, busID, seatNo, clientID, action](){
                lock_guard<mutex>lock(mtx);
                serveClient(busID, seatNo, clientID, action);
            });
            clientThread.detach();
            cout<<"Enter 0 to exit or 1 to continue"<<endl;
            cin>>status;
        }   
        while(status);
    }
};

class ForkedServing : public Serving
{
public:
    ForkedServing(){
        void * shared_buses_memory = mmap(NULL,sizeof(Bus)*buses.size(), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if(shared_buses_memory == MAP_FAILED){
            perror("mmap");
            exit(1);
        }
        memcpy(shared_buses_memory, buses.data(), sizeof(Bus) * buses.size());
        buses = *reinterpret_cast<vector<Bus>*>(shared_buses_memory);

        void * shared_empty_buses_memory = mmap(NULL, sizeof(queue<int>), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if(shared_empty_buses_memory == MAP_FAILED){
            perror("mmap");
            exit(1);
        }
        emptyBuses = new (shared_empty_buses_memory) queue<int>();
        emptyBuses->push(1);
        totalBuses = (int *)mmap(NULL,sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if(totalBuses == MAP_FAILED){
            perror("mmap");
            exit(1);
        }
        *totalBuses = buses.size();

        busesExceedingLoad = (int *)mmap(NULL,sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if(busesExceedingLoad == MAP_FAILED){
            perror("mmap");
            exit(1);
        }
        *busesExceedingLoad = 0;
    }
};