#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <set>
#include <unordered_map>
#include <chrono>

#define INITIAL_BUSES 20
#define MAX_CLIENTS 10000
#define MAX_BUSES 100
#define MAX_SEATS 30
#define TIMER 60
#define BUSES_TO_BE_ADDED 2
#define LOAD_EXCEED_FACTOR 0.8
#define LOAD_LESS_FACTOR 0.2

using namespace std;

class BusBooking
{
protected:
    struct Client
    {
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

    Data* sharedData;  // Pointer to shared memory

    // Constructor now takes the sharedData pointer as a member
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

    // Function to book a seat
    void book(int b, int s, int c){
        sharedData->buses[b].seats[s] = 0;
        cout << "Seat Booked in bus " << b << " seat " << s << endl;
    }
};

int main()
{
   // Creating and mapping shared memory
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
   BusBooking server(sharedData);

   pid_t pid = fork();
   if (pid == 0)
   {
      // In child process: book a seat
      server.book(1, 3, 2);
      std::cout << "Child process: Booked seat 1 3." << std::endl;
      exit(0);
   }
   else
   {
      // In parent process
      // sleep(3); // Wait for the child process to complete its booking
      std::cout << "Parent process: Checking seat 1 3 status: " 
                << (sharedData->buses[1].seats[3] == 0 ? "Booked" : "Free") 
                << std::endl;
   }

   // Cleanup shared memory
   munmap(sharedData, SHARED_MEMORY_SIZE);
   close(fd);
   shm_unlink("/shared_data");

   return 0;
}
