class ThreadServing : public Serving
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
