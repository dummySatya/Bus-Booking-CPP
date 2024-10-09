import socket
import random
import threading
import time

HOST = '127.0.0.1' 
PORT = 8081

ACTIONS = ['book', 'select', 'cancel']
ACTION_PROBABILITY = [0.9, 0.05, 0.05]  # 90% book, 5% select, 5% cancel

MAX_CLIENTS = 1000  # Number of clients for load test
ITERATIONS = 100    # Number of actions each client performs
INITIAL_BUSES = 20  # Initial number of buses
MAX_BUSES = 100     # Maximum number of buses
BUS_INCREMENT = 10  # Number of buses to add at each step
MAX_SEATS = 30      # Max number of seats per bus

current_buses = INITIAL_BUSES

def client_thread(client_id):
    global current_buses
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((HOST, PORT))

        sock.sendall(b'client')
        response = sock.recv(1024).decode()

        if "Mode selection Success" in response:
            print(f"Client {client_id} connected successfully.")

            sock.sendall(f"{client_id}\n".encode())
            response = sock.recv(1024).decode()

            if "Client Login Success" in response:
                print(f"Client {client_id} login successful.")

            # Perform random actions
            for i in range(ITERATIONS):
                action = random.choices(ACTIONS, ACTION_PROBABILITY)[0]
                bus_id = random.randint(0, current_buses - 1)
                seat_no = random.randint(1, MAX_SEATS)

                if action == 'book':
                    command = f"book {bus_id} {seat_no}\n"
                elif action == 'select':
                    command = f"select {bus_id} {seat_no}\n"
                else:
                    command = f"cancel {bus_id} {seat_no}\n"

                sock.sendall(command.encode())
                response = sock.recv(1024).decode()
                print(f"Client {client_id} performed action: {command.strip()}, Server response: {response.strip()}")

                # small delay to simulate time between actions
                time.sleep(random.uniform(0.1, 0.5))
        sock.close()
    except Exception as e:
        print(f"Client {client_id} encountered an error: {e}")
        return

def load_test():
    threads = []
    global current_buses
    for client_id in range(MAX_CLIENTS):
        thread = threading.Thread(target=client_thread, args=(client_id,))
        threads.append(thread)
        thread.start()
        if client_id % 300 == 0 and current_buses < MAX_BUSES:
            current_buses += 2
            print(f"Bus count increased to {current_buses}.")

    # Wait for all clients to finish
    for thread in threads:
        thread.join()

    print("Load test completed.")


if __name__ == "__main__":
    load_test()
