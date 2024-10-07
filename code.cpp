#include <bits/stdc++.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <bitset>

#define MAX_SEATS 40
#define TIMER 10 // 10 seconds
#define BUSES_TO_BE_ADDED 2

using namespace std;

class BusBooking
{
protected:
    struct Bus
    {
        bitset<MAX_SEATS> seats;  // Using bitset instead of vector<bool>
        vector<chrono::time_point<chrono::steady_clock>> seatLockDurations;
        int bookedSeats;
        bool active; // is false only when some merger has happened
    };

    vector<Bus> buses;
    int *emptyBuses; // Using int array in shared memory for fixed size
    int *totalBuses;
    int *buses_exceeding_load;

public:
    BusBooking(int numBuses = 10)
    {
        buses.resize(numBuses);
        for (auto &bus : buses)
        {
            bus.seatLockDurations.resize(MAX_SEATS);
            bus.bookedSeats = 0;
            bus.active = true;
        }
        *totalBuses = numBuses;
        *buses_exceeding_load = 0;
    }

    double loadFactor(int busID)
    {
        return static_cast<double>(buses[busID].bookedSeats) / MAX_SEATS;
    }

    bool checkLoadExceeds(double load)
    {
        return load >= 0.8;
    }

    void busAdder()
    {
        int busesExpansion = BUSES_TO_BE_ADDED - *emptyBuses;
        if (busesExpansion > 0)
        {
            for (int busesToBeAdded = 0; busesToBeAdded < busesExpansion; busesToBeAdded++)
            {
                buses.emplace_back();
                buses.back().seatLockDurations.resize(MAX_SEATS);
                buses.back().active = true;
                buses.back().bookedSeats = 0;
            }
            *totalBuses += busesExpansion;
        }
        else
        {
            for (int busesToBeAdded = 0; busesToBeAdded < BUSES_TO_BE_ADDED; busesToBeAdded++)
            {
                int busid = *emptyBuses; // assuming this to be fixed size int array in shared mem
                (*emptyBuses)++; // Increment pointer to next empty bus
                buses[busid].seatLockDurations.resize(MAX_SEATS);
                buses[busid].active = true;
                buses[busid].bookedSeats = 0;
            }
            *totalBuses += BUSES_TO_BE_ADDED;
        }
    }

    void loadChanger(int busID)
    {
        double load = loadFactor(busID);
        if (checkLoadExceeds(load))
        {
            ++(*buses_exceeding_load);
        }
        if (*buses_exceeding_load == *totalBuses)
        {
            busAdder();
        }
    }

    vector<pair<int, int>> mergerPossibilities()
    {
        vector<pair<int, int>> possibleBuses;
        for (int bus1 = 0; bus1 < buses.size() - 1; bus1++)
        {
            for (int bus2 = bus1 + 1; buses[bus1].active && bus2 < buses.size(); bus2++)
            {
                bool possible = true;
                for (int seat = 0; seat < MAX_SEATS; seat++)
                {
                    if (!(buses[bus1].seats[seat] || buses[bus2].seats[seat]))
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
            swap(bus1, bus2);
        }
        for (int seat = 0; seat < MAX_SEATS; seat++)
        {
            buses[bus1].seats[seat] = buses[bus1].seats[seat] || buses[bus2].seats[seat];
        }
        *emptyBuses = bus2;  // storing in shared memory array
        buses[bus2].active = false;
    }

    bool validSeat(int busID, int seatNo)
    {
        return busID < buses.size() && seatNo < MAX_SEATS && buses[busID].active;
    }

    bool seatAvailability(int busID, int seatNo)
    {
        return buses[busID].seats[seatNo];
    }

    void selectSeat(int busID, int seatNo, int clientID)
    {
        if (!validSeat(busID, seatNo))
        {
            cout << "Invalid Seat" << endl;
            return;
        }
        if (!seatAvailability(busID, seatNo))
        {
            cout << "Seat already booked" << endl;
            return;
        }
        buses[busID].seats[seatNo] = false;
        buses[busID].seatLockDurations[seatNo] = chrono::steady_clock::now();
        cout << "Client " << clientID << " booked seat " << seatNo << " on bus id " << busID << endl;
    }
    void bookSeat(int busID, int seatNo, int clientID)
    {
        if (!validSeat(busID, seatNo))
        {
            cout << "Invalid Seat" << endl;
            return;
        }
        if (!seatAvailability(busID, seatNo))
        {
            cout << "Seat already booked" << endl;
            return;
        }
        buses[busID].seats[seatNo] = false;
        ++buses[busID].bookedSeats;
        loadChanger(busID);
        cout << "Client " << clientID << " booked seat " << seatNo << " on bus id " << busID << endl;
    }
    void cancelSeat(int busID, int seatNo, int clientID)
    {
        if (!validSeat(busID, seatNo))
        {
            cout << "Invalid Seat" << endl;
            return;
        }
        if (seatAvailability(busID, seatNo))
        {
            cout << "Seat not booked" << endl;
            return;
        }
        buses[busID].seats[seatNo] = true;
        --buses[busID].bookedSeats;
        loadChanger(busID);
        cout << "Client " << clientID << " cancelled seat " << seatNo << " on bus id " << busID << endl;
    }

    void timerEvents()
    {
        auto now = chrono::steady_clock::now();
        for (int bus = 0; bus < buses.size(); ++bus)
        {
            for (int seat = 0; seat < MAX_SEATS; ++seat)
            {
                if (!buses[bus].seats[seat])
                {
                    auto duration = chrono::duration_cast<chrono::seconds>(now - buses[bus].seatLockDurations[seat]).count();
                    if (duration >= TIMER)
                    {
                        buses[bus].seats[seat] = true;
                        cout << "Seat Released for seat " << seat << " on bus id " << bus << endl;
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

class ForkedServing : public BusBooking
{
public:
    ForkedServing()
    {
        void *shared_buses_memory = mmap(NULL, sizeof(Bus) * buses.size(), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (shared_buses_memory == MAP_FAILED)
        {
            perror("mmap");
            exit(1);
        }
        memcpy(shared_buses_memory, buses.data(), sizeof(Bus) * buses.size());
        buses = *reinterpret_cast<vector<Bus> *>(shared_buses_memory);

        void *shared_empty_buses_memory = mmap(NULL, sizeof(int) * 10, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (shared_empty_buses_memory == MAP_FAILED)
        {
            perror("mmap");
            exit(1);
        }
        emptyBuses = reinterpret_cast<int *>(shared_empty_buses_memory);
        *emptyBuses = 1;

        totalBuses = (int *)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (totalBuses == MAP_FAILED)
        {
            perror("mmap");
            exit(1);
        }
        *totalBuses = buses.size();

        buses_exceeding_load = (int *)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (buses_exceeding_load == MAP_FAILED)
        {
            perror("mmap");
            exit(1);
        }
        *buses_exceeding_load = 0;
    }
};
