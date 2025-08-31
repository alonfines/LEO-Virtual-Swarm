//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2008 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include<iostream>
#include <vector>
#include <omnetpp.h>
#include <fstream>
#include "Packet_m.h"
#include <string>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <stdexcept>
using namespace omnetpp;

#define UP 1
#define DOWN 2
#define EAST 3
#define WEST 4


/**
 * Generates traffic for the network.
 */
class App : public cSimpleModule
{
  private:
    // configuration
    int myAddress;
    int myTTL;
    int myPlane;
    int myOrensIndex;
    bool isActive;
    int numPktReceived;
    int numPktSent;
    int numPktperMsg;
    std::unordered_map<int, std::pair<bool, int>>  neighbors; // neighbors of the module, {address, {connected, direction}}}
    std::vector<std::tuple<double, double, int, int>> connectionsEvents; // hold the next reconnection and disconnections of the module
    std::vector<std::tuple<int, int>> orensMapping;
    std::vector<int> activeAddresses;
    std::unordered_map<int, std::pair<int, int>>  activeAddressesTimes;
    std::vector<std::vector<int>> traffic;  // [time,is_val_north,traffic]
    std::unordered_map<std::string,bool> activeTraffics;  // activeAddress,Time->bool


    // state
    cMessage *generatePacket;
    Control *controlConnect;
    Control *controlDisconnect;
    cMessage *activeIn;
    cMessage *activeOut;
    int is_val_north;

//    // Changeable parameters
////    bool goLeft = false;
//    int maxBitsperMsg;


    // signals
    simsignal_t endToEndDelaySignal;
    simsignal_t hopCountSignal;
    simsignal_t sourceAddressSignal;

  public:
    App();
    virtual ~App();

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;

    // intilization function
    virtual void getOrensMapping();
    virtual void getNeighbors();
    virtual void getConnections();
    virtual void getCSVdata();
    virtual void removeRedundancy();
    virtual void extractSatelliteTimes();
    virtual void extractTrafficOfActive();

    // getting states
    virtual bool isNorth(int satAddress);
    virtual std::vector<int> getASorMAS(bool giveAS);
    virtual std::vector<int> getAPorMAP(bool giveAP);
    virtual std::vector<int> getAPS();

    // calculate for algo
    virtual std::vector<int> calculateVU();
    virtual std::vector<int> calculateb1b2();
    virtual int calculateJ(int vu);

    // update function / GUIS
    virtual void updateIsValNorth(int change);
    virtual void updateGUI(Packet* pk);
    virtual void changeActiveStatus(bool status);

    // config
    virtual void disconnectWith(int neighborAddr);
    virtual void reconnectWith(int neighborAddr);

    // orens index supplementary
    virtual int convertToOrensIndex(int address);
    virtual bool doesHaveInterPlane(int address);
    virtual bool doesOrenIndexHaveInterPlane(int orenIndex);

    virtual Packet* generateNewPacket(int collectedData, int numPktperMsg); //new
//    virtual Packet* generateNewPacket(int collectedData);//old
    virtual int getISL(int to);
    virtual int neighborDirection(int neighborAddr);
    virtual bool isDirectionFailed(int dir);

    virtual void writeRecvCSV(double time, int sender, double latency, int numpckPerMsg,int hopCount,int bitLength);
    virtual std::string getActiveAddressesAsString();
    virtual void writeSentCSV();

    virtual void finish() override;

    virtual void sendPacket(Packet *pk, int direction);

};

Define_Module(App);


App::App()
{
    generatePacket = NULL;
    controlConnect = NULL;
    controlDisconnect = NULL;
    activeIn = NULL;
    activeOut= NULL;
}

App::~App()
{
    if(generatePacket)
    {
        cancelAndDelete(generatePacket);
    }
    if(controlConnect)
    {
        cancelAndDelete(controlConnect);
    }
    if(controlDisconnect)
    {
        cancelAndDelete(controlDisconnect);
    }
}

void App::finish()
{
    for (auto & sat : activeAddresses) {
        if(myAddress==sat)
        {
            recordScalar("#sent",numPktSent);
            recordScalar("#received",numPktReceived);
            break;
        }
    }
}

void App::initialize()

{

    numPktperMsg =  0;
    numPktReceived = 0;
    numPktSent = 0;
    isActive = false;
    myAddress = par("address");
    myTTL = par("ttl");
    EV << myAddress << endl;
    myPlane = (myAddress) / 100;
    myOrensIndex = this->convertToOrensIndex(myAddress);
    (myAddress % 100 > 10 || myAddress % 100 < 3) ? this->updateIsValNorth(1) : this->updateIsValNorth(0);
    this->getNeighbors();
    this->getConnections();
    this->getOrensMapping();
    this->extractSatelliteTimes();
    // display my address
    std::string disp = "(" + std::to_string(myAddress) + "), " + std::to_string(myPlane)+ "," + std::to_string(myOrensIndex);
    getParentModule()->getDisplayString().setTagArg("t",0,disp.c_str());
    getParentModule()->getDisplayString().setTagArg("t",1,"t");
    //getParentModule()->getDisplayString().setTagArg("t",2,"black");

    // Color the node to be green, if this satellite is an active one
    // get all the active satellites addresses into a vector 'activeAddresses'
    // Read the CSV data as well
    const char *activeSatPar = getParentModule()->getParentModule()->par("active_sats");
    cStringTokenizer tokenizerSat(activeSatPar);
    const char *tokenSat;
    while ((tokenSat = tokenizerSat.nextToken())!=NULL)
    {
        activeAddresses.push_back(atoi(tokenSat));  // save the active satellite address
        if (myAddress == atoi(tokenSat))
        {
            // If I'm an active satellite, let me schedule my exit end enter time
            activeIn = new cMessage();
            scheduleAt(activeAddressesTimes[myAddress].first, activeIn);
            activeOut = new cMessage();
            scheduleAt(activeAddressesTimes[myAddress].second, activeOut);


            // I'm active so I have some data to send, read it
            this->getCSVdata();
            for (const auto& inner_vec : traffic) {
              // Iterate over each element of the inner vector
              for (const auto& elem : inner_vec) {
                EV << elem << " ";
              }
              EV << std::endl;
            }
        }
    }
    if (activeAddresses.size() < 1)
        throw cRuntimeError("At least two activate satellites must be specified!");

    WATCH(myAddress);

    // generate the next event for traffic generation at time 0
    generatePacket = new cMessage("nextPacket");
    scheduleAt(simTime(), generatePacket);

    // generate the next event for reconnection
    if (connectionsEvents.size() > 0){
        controlConnect = new Control();
        controlConnect->setTime(std::get<0>(connectionsEvents[0]));
        controlConnect->setAddress( std::get<2>(connectionsEvents[0]));
        controlConnect->setIsAsending(std::get<3>(connectionsEvents[0]));
        scheduleAt(controlConnect->getTime(), controlConnect);

        controlDisconnect = new Control();
        controlDisconnect->setTime(std::get<1>(connectionsEvents[0]));
        controlDisconnect->setAddress( std::get<2>(connectionsEvents[0]));
        scheduleAt(controlDisconnect->getTime(), controlDisconnect);}


    // disconnect with East and West
    for (const auto& item : neighbors)
    {
        if (item.second.second == EAST || item.second.second == WEST){
            this->disconnectWith(item.first);
        }
    }

//     hide Up and Down connection
    for (const auto& item : neighbors)
    {
        if (item.second.second == UP || item.second.second == DOWN)
        {
            for (int i = 0; i < this->getParentModule()->gateSize("port$o"); i++)
            {
                cGate *gate = this->getParentModule()->gate("port$o", i);
                cModule *connectedModule = gate->getNextGate()->getOwnerModule();
                if (connectedModule->par("address").intValue() == item.first)
                {
                    cDisplayString& connDispStr = gate->getDisplayString();
                    connDispStr.parse("ls=gray,0.5,d");
                }
            }
        }
    }


    EV << std::to_string(myOrensIndex) << "{ ";
    for (auto it = neighbors.begin(); it != neighbors.end(); ++it) {
        const auto& key = it->first;
        const auto& value = it->second;
        EV << key << ": (" << value.first << ", " << value.second << ") ";
    }
    EV << "}\n";

    // register signals for statistics afterwards
    endToEndDelaySignal = registerSignal("endToEndDelay");
    hopCountSignal =  registerSignal("hopCount");
    sourceAddressSignal = registerSignal("sourceAddress");



}

void App::handleMessage(cMessage* msg)
{
    int maxBitsperMsg = getParentModule()->getParentModule()->par("maxBitsperMsg");;
    bool goLeft = getParentModule()->getParentModule()->par("goLeft");;
    int scenerio_num = getParentModule()->getParentModule()->par("scenerio_num");;
    int minPlane = 3;
    int maxPlane = 5;
    if (scenerio_num == 2){
        int minPlane = 1;
    }
    else if (scenerio_num == 3){
        int minPlane = 1;
        int maxPlane = 3;
    }

    // Check if it's time to handle some internal event
    if (msg == generatePacket)
    {
        // Get the current simulation time
        simtime_t currentTime = simTime();

        // Loop through the vector of traffic and handle any that occur at the current time
        for (auto it = traffic.begin(); it != traffic.end();)
        {
            if ((simtime_t)(*it)[0] == currentTime)
            {
                // Change the status
                this->updateIsValNorth((*it)[1]);

                // Generate traffic if necessary
                if ((*it)[2] > 0)
                {
                    // If I'm sending traffic obviously I'm active
                    this->changeActiveStatus(true);

                    char to_bubble[64];
                    int trafficAmount = (*it)[2];
                    int numPktperMsg = 1; // new

                    // Algorithm 1 Satellite LPVS East BB Routing
                    while (trafficAmount > maxBitsperMsg)
                    {
                        Packet *pk = generateNewPacket(maxBitsperMsg, numPktperMsg);
                        numPktperMsg++; // new
                        numPktSent++;
                        trafficAmount -= maxBitsperMsg;

                        if (!goLeft)
                        {
                            sendPacket(pk, UP);
                            if (myOrensIndex != pk->getV())
                            {
                                sendPacket(pk, DOWN);
                            }
                        }
                        else if (goLeft)
                        {
                            sendPacket(pk, DOWN);
                            if (myOrensIndex != pk->getU())
                            {
                                sendPacket(pk, UP);
                            }
                        }
                    }

                    Packet *pk = generateNewPacket(trafficAmount, numPktperMsg);
                    numPktSent++;
                    if (!goLeft)
                    {
                        sendPacket(pk, UP);
                        if (myOrensIndex != pk->getV())
                        {
                            sendPacket(pk, DOWN);
                        }
                    }
                    else if (goLeft)
                    {
                        sendPacket(pk, DOWN);
                        if (myOrensIndex != pk->getU())
                        {
                            sendPacket(pk, UP);
                        }
                    }

                    this->writeSentCSV();
                    delete pk;
                }

                // Remove the event from the vector
                it = traffic.erase(it);
            }
            else
            {
                // No need to continue searching for events since they're sorted by time
                break;
            }
        }

        // Schedule the next event if there are more events
        if (!traffic.empty())
        {
            simtime_t nextTime = traffic.front()[0];
            scheduleAt(nextTime, generatePacket);
        }
    }

    else if(msg == controlConnect)
    {

        // connect event
        this->reconnectWith(controlConnect->getAddress());
        this->updateIsValNorth(controlConnect->getIsAsending());

        // schedule the next connect event
        if (connectionsEvents.size() > 0)
        {
            for(int i=0; i<connectionsEvents.size();i++)
            {
                if(((std::get<0>(connectionsEvents[i]))) > controlConnect->getTime())
                {
                    controlConnect->setTime(std::get<0>(connectionsEvents[i]));
                    controlConnect->setAddress( std::get<2>(connectionsEvents[i]));
                    controlConnect->setIsAsending(std::get<3>(connectionsEvents[i]));
                    scheduleAt(controlConnect->getTime(), controlConnect);
                    break;
                }
            }
        }
        //}
    }
    else if (msg == controlDisconnect)
    {
        // disconnection event
        this->disconnectWith(controlDisconnect->getAddress());
        // schedule the next reconnection event, if there is still new events
        if(!connectionsEvents.empty()){
            connectionsEvents.erase(connectionsEvents.begin());
        }
        //schedule new disconnection
        if(!connectionsEvents.empty())
        {
            controlDisconnect->setTime(std::get<1>(connectionsEvents[0]));
            controlDisconnect->setAddress( std::get<2>(connectionsEvents[0]));
            scheduleAt(controlDisconnect->getTime(), controlDisconnect);
        }
        //}
    }
    else if(msg == activeIn)
    {
        this->changeActiveStatus(true);
        delete msg;
    }
    else if(msg == activeOut)
    {
        this->changeActiveStatus(false);
        delete msg;
    }
    else
    {
        char to_bubble[64];
        Packet *pk = check_and_cast<Packet *>(msg);
        EV << "HANDLE MSG" << endl;
        // Handle incoming packet
        if (isActive)
        {
            if(!goLeft)
            {
                // Algorithm 1 Satellite LPVS East BB Routing
                // Active Satellite Received Data.
                /*
                 * 3) When Reading Msg from downISL:
                    Decrease T T L, send on UpISL.
                    4) When Reading Msg from upISL:
                    If indexonPlane! = v then decrease T T L, send
                    Msg on downISL.
                 */
                // check if I already saw this messegeEV << "Active recieved: " << std::to_string(myAddress) << endl;
                EV << "received packet " << pk->getName() << " after " << pk->getHopCount() << "hops" << endl;

                // check if I already saw this messege
                std::string key = std::to_string(pk->getSrcAddr()) + "," + std::to_string(std::lround(pk->getCreationTime().dbl())) + ","+std::to_string(pk->getnumPktperMsg()); //new

                // if I didnt saw this messege and its not mine msg then update statistics
                if(this->activeTraffics[key]==false && this->myAddress != pk->getSrcAddr())
                {   // update statistics
                    EV << "New data" << endl;
                    numPktReceived += 1;
                    emit(endToEndDelaySignal, simTime() - pk->getCreationTime());
                    emit(hopCountSignal, pk->getHopCount());
                    emit(sourceAddressSignal, pk->getSrcAddr());

                    // update GUI given the packet
//                    this->updateGUI(pk);
                    EV << key << ": " << std::boolalpha << this->activeTraffics[key] << endl;
                    this->activeTraffics[key]=true;
                    EV << key << ": " << std::boolalpha << this->activeTraffics[key] << endl;

                    // record to CSV
                    writeRecvCSV(pk->getCreationTime().dbl(), pk->getSrcAddr(), (simTime() - pk->getCreationTime()).dbl(),pk->getnumPktperMsg(),pk->getHopCount(),pk->getBitLength());

                }
                else // I havn't seen the data
                {
                    EV << key << ": " << std::boolalpha << this->activeTraffics[key] << endl;
                    EV << "Already saw this data" << endl;
                }
                //  When Reading Msg from downISL: Decrease TTL, send on upISL.
                if(this->neighborDirection(pk->getIntermediateSrcAddr()) == DOWN)   // Reading Msg from downISL
                {
                    // Decrease TTL
                    pk->setTTL(pk->getTTL() - 1);

                    // send to Up ISL
                    sendPacket(pk, UP);
                }
                else if(this->neighborDirection(pk->getIntermediateSrcAddr()) == UP)   // Reading Msg from upISL
                {
                    //When Reading Msg from upISL: If indexonPlane! = v then decrease TTL, send Msg on downISL.
                    if(myOrensIndex != pk->getV())
                    {
                        // Decrease TTL
                        pk->setTTL(pk->getTTL() - 1);

                        // send to Down ISL
                        sendPacket(pk,DOWN);
                    }
                    else
                    {
                        sprintf(to_bubble, "Arrived! from: %d, NOT FORWARDING IM V", pk->getSrcAddr());
//                        getParentModule()->bubble(to_bubble);
                    }
                }
            }
            else if(goLeft)
                        {
                            // Algorithm 1 Satellite LPVS East BB Routing
                            // Active Satellite Received Data.
                            /*
                             * 3) When Reading Msg from downISL:
                                Decrease T T L, send on upISL.
                                4) When Reading Msg from downISL:
                                If indexonPlane! = u then decrease T T L, send
                                Msg on upISL.
                             */

                            // check if I already saw this messegeEV << "Active recieved: " << std::to_string(myAddress) << endl;
                            EV << "received packet " << pk->getName() << " after " << pk->getHopCount() << "hops" << endl;
                            std::string key = std::to_string(pk->getSrcAddr()) + "," + std::to_string(std::lround(pk->getCreationTime().dbl())) + ","+std::to_string(pk->getnumPktperMsg()); //new

                            // if I didnt saw this messege and its not mine msg then update statistics
                            if(this->activeTraffics[key]==false && this->myAddress != pk->getSrcAddr())
                            {   // update statistics
                                EV << "New data" << endl;
                                numPktReceived += 1;
                                emit(endToEndDelaySignal, simTime() - pk->getCreationTime());
                                emit(hopCountSignal, pk->getHopCount());
                                emit(sourceAddressSignal, pk->getSrcAddr());

                                // update GUI given the packet
//                                this->updateGUI(pk);
                                EV << key << ": " << std::boolalpha << this->activeTraffics[key] << endl;
                                this->activeTraffics[key]=true;
                                EV << key << ": " << std::boolalpha << this->activeTraffics[key] << endl;

                                // record to CSV
                                writeRecvCSV(pk->getCreationTime().dbl(), pk->getSrcAddr(), (simTime() - pk->getCreationTime()).dbl(),pk->getnumPktperMsg(),pk->getHopCount(),pk->getBitLength());

                            }
                            else // I havn't seen the data
                            {
                                EV << key << ": " << std::boolalpha << this->activeTraffics[key] << endl;
                                EV << "Already saw this data" << endl;
                            }
                            //  When Reading Msg from upISL: Decrease TTL, send on downISL.
                            if(this->neighborDirection(pk->getIntermediateSrcAddr()) == UP)   // Reading Msg from upISL
                            {
                                // Decrease TTL
                                pk->setTTL(pk->getTTL() - 1);
                                sendPacket(pk, DOWN);
                            }
                            else if(this->neighborDirection(pk->getIntermediateSrcAddr()) == DOWN)   // Reading Msg from downISL
                            {
                                //When Reading Msg from upISL: If indexonPlane! = u then decrease TTL, send Msg on downISL.
                                if(myOrensIndex != pk->getU())
                                {
                                    // Decrease TTL
                                    pk->setTTL(pk->getTTL() - 1);

                                    // send to Up ISL
                                   sendPacket(pk,UP);
                                }
                                else // I am U
                                {
                                    sprintf(to_bubble, "Arrived! from: %d, NOT FORWARDING IM U", pk->getSrcAddr());
//                                    getParentModule()->bubble(to_bubble);
                                }
                            }
                        }
        }
        else // not active
        {
            if(!goLeft){
                // not active satellite
                // Algorithm 3 EBB - Proxy Routing with failed inter-ISLs
                // Received Packet on proxy Satellite
                EV << "proxy received: " << std::to_string(myAddress) << endl;
                if(myOrensIndex != pk->getJ())
                {
                    EV << "my index:" << std::to_string(myOrensIndex) <<" is not j: " << std::to_string(pk->getJ()) << endl;
                    EV << "Received from: " << std::to_string(this->neighborDirection(pk->getIntermediateSrcAddr())) << endl;

                    if(pk->getTTL() <= 0)
                    {
                        EV << "TTL == 0" << endl;
                        sprintf(to_bubble, "TTL = 0, DISCARDING");
                        getParentModule()->bubble(to_bubble);
                        delete pk;
                        return;
                    }
                    // Decrease TTL
                    pk->setTTL(pk->getTTL() - 1);

                    // If Received from downISL :
                    if (this->neighborDirection(pk->getIntermediateSrcAddr()) == DOWN)
                    {
                        EV << "Received from downISL: " << std::to_string((this->neighborDirection(pk->getIntermediateSrcAddr()))) << endl;
                        // If reachedj = False; then send on upISL
                        if (pk->getReachedJ() == false)
                        {
                            EV << "packet didnt reach j" << endl;
                            sendPacket(pk, UP);
                        }
                        else // If reachedj and message from down;
                        {
                            EV << "packet did reach j" << endl;
                            if (pk->getEfailed()){
                                sendPacket(pk, EAST);
                            }
                            if (pk->getWfailed()){
                                sendPacket(pk, WEST);
                            }
                            if (pk->getWfailed()||pk->getEfailed()){
                                sendPacket(pk, UP);
                            }
                        }
                    }

                    else if(this->neighborDirection(pk->getIntermediateSrcAddr()) == EAST){
                                pk->setEfailed(false);
                                if(this->isDirectionFailed(WEST)){
                                    pk->setWfailed(true);
                                }
                                else{
                                    sendPacket(pk, WEST);
                                    }
                                sendPacket(pk, DOWN);
                                }
                    else if(this->neighborDirection(pk->getIntermediateSrcAddr()) == WEST){
                                pk->setWfailed(false);
                                if(this->isDirectionFailed(EAST)){
                                    pk->setEfailed(true);
                                }
                                else{
                                    sendPacket(pk, EAST);
                                    }
                                sendPacket(pk, DOWN);
                                sendPacket(pk, UP);//special
                                pk->setEfailed(false);//special
                                }
                    else if(this->neighborDirection(pk->getIntermediateSrcAddr()) == UP)
                    {
                       if (pk->getEfailed()){
                           sendPacket(pk, EAST);
                           }

                       if (pk->getWfailed()){
                           sendPacket(pk, WEST);
                           }

                       if (pk->getV() != this->myOrensIndex)
                       {
                           sendPacket(pk, DOWN);
                    }

                }
                }

                else //If indexOnPlane == j
                {
                    EV << "my index:" << std::to_string(myOrensIndex) <<" it is j: " << std::to_string(pk->getJ()) << endl;
                    if(pk->getTTL() <= 0)
                    {
                        EV << "TTL == 0" << endl;
                        sprintf(to_bubble, "TTL = 0, DISCARDING");
                        getParentModule()->bubble(to_bubble);
                        return;
                    }
                    // Decrease TTL
                    pk->setTTL(pk->getTTL() - 1);
                    if(this->myPlane == minPlane){
                        pk->setWfailed(false);
                    }
                    if(this->myPlane == maxPlane){
                        pk->setEfailed(false);
                    }
                    //set reached j == true
                    pk->setReachedJ(true);
                    EV << "received from:" << std::to_string((this->neighborDirection(pk->getIntermediateSrcAddr()))) << endl;
                    // If Received from down ISL :
                    if (this->neighborDirection(pk->getIntermediateSrcAddr()) == UP)
                    {
                        sendPacket(pk, DOWN);

                        if(pk->getEfailed()){
                            sendPacket(pk, EAST);
                        }
                        if(pk->getEfailed()||pk->getWfailed()){
                            sendPacket(pk, UP);
                        }

                    }


                    else if (this->neighborDirection(pk->getIntermediateSrcAddr()) == DOWN) // If Received from down ISL :
                    {
                        EV << "Received from DownISL: " << std::to_string((this->neighborDirection(pk->getIntermediateSrcAddr()))) << endl;
                        if(this->isDirectionFailed(EAST))
                        {
                            EV << "Sending on UpISL because direction EAST is failed" << endl;
                            // set EFailed, send on downISL.
                            pk->setEfailed(true);}
                        else
                        {
                            EV << "Sending on EastISL" << endl;
                            // send to east ISL
                            sendPacket(pk, EAST);
                        }
                        sendPacket(pk, UP);
                        }
                    else // not from up
                      {
                          EV << "Received from east ISL or received from downISL and Wfailed" << endl;
                          std::vector<int> APS = this->getAPS();
                          // clear Efailed
                          pk->setEfailed(false);
                          sendPacket(pk, UP);
                          sendPacket(pk, DOWN);
                    }
                }
            }

            if(goLeft){
                // not active satellite
                // Algorithm 3 EBB - Proxy Routing with failed inter-ISLs
                // Received Packet on proxy Satellite
                EV << "proxy recieved: " << std::to_string(myAddress) << endl;
                if(myOrensIndex != pk->getJ())
                {
                    EV << "my index:" << std::to_string(myOrensIndex) <<" is not j: " << std::to_string(pk->getJ()) << endl;
                    EV << "Received from: " << std::to_string(this->neighborDirection(pk->getIntermediateSrcAddr())) << endl;

                    if(pk->getTTL() <= 0)
                    {
                        EV << "TTL == 0" << endl;
                        sprintf(to_bubble, "TTL = 0, DISCARDING");
                        getParentModule()->bubble(to_bubble);
                        delete pk;
                        return;
                    }
                    // Decrease TTL
                    pk->setTTL(pk->getTTL() - 1);

                    // If Received from upISL :
                    if (this->neighborDirection(pk->getIntermediateSrcAddr()) == UP)
                    {
                        EV << "Received from upISL: " << std::to_string((this->neighborDirection(pk->getIntermediateSrcAddr()))) << endl;
                        // If reachedj = False; then send on downISL
                        if (pk->getReachedJ() == false)
                        {
                            EV << "packet didnt reach j" << endl;
                            sendPacket(pk, DOWN);
                        }
                        else // If reachedj and message from up;
                        {
                            EV << "packet did reach j" << endl;
                            if (pk->getEfailed()){
                                sendPacket(pk, EAST);
                            }
                            if (pk->getWfailed()){
                                sendPacket(pk, WEST);
                            }
                            if (pk->getWfailed()||pk->getEfailed()){
                                sendPacket(pk, DOWN);
                            }
                        }
                    }

                    else if(this->neighborDirection(pk->getIntermediateSrcAddr()) == EAST){
                                pk->setEfailed(false);
                                if(this->isDirectionFailed(WEST)){
                                    pk->setWfailed(true);
                                }
                                else{
                                    sendPacket(pk, WEST);
                                    }
                                sendPacket(pk, UP);
                                }
                    else if(this->neighborDirection(pk->getIntermediateSrcAddr()) == WEST){
                                pk->setWfailed(false);
                                if(this->isDirectionFailed(EAST)){
                                    pk->setEfailed(true);
                                }
                                else{
                                    sendPacket(pk, EAST);
                                    }
                                sendPacket(pk, UP);
                                sendPacket(pk, DOWN);//special
                                pk->setEfailed(false);//special

                                }
                    else if(this->neighborDirection(pk->getIntermediateSrcAddr()) == DOWN)
                    {
                       if (pk->getEfailed()){
                           sendPacket(pk, EAST);
                           }

                       if (pk->getWfailed()){
                           sendPacket(pk, WEST);
                           }

                       if (pk->getU() != this->myOrensIndex)
                       {
                           sendPacket(pk, UP);
                    }

                }
                }

                else //If indexOnPlane == j
                {
                    EV << "my index:" << std::to_string(myOrensIndex) <<" it is j: " << std::to_string(pk->getJ()) << endl;
                    if(pk->getTTL() <= 0)
                    {
                        EV << "TTL == 0" << endl;
                        sprintf(to_bubble, "TTL = 0, DISCARDING");
                        getParentModule()->bubble(to_bubble);
                        return;
                    }
                    // Decrease TTL
                    pk->setTTL(pk->getTTL() - 1);
                    if(this->myPlane == minPlane){
                                                pk->setWfailed(false);
                    }
                    if(this->myPlane == maxPlane){
                        pk->setEfailed(false);
                    }
                    //set reached j == true
                    pk->setReachedJ(true);
                    EV << "received from:" << std::to_string((this->neighborDirection(pk->getIntermediateSrcAddr()))) << endl;
                    // If Received from down ISL :
                    if (this->neighborDirection(pk->getIntermediateSrcAddr()) == DOWN)
                    {
                        sendPacket(pk, UP);

                        if(pk->getEfailed()){
                            sendPacket(pk, EAST);
                        }
                        if(pk->getEfailed()||pk->getWfailed()){
                            sendPacket(pk, DOWN);
                        }

                    }


                    else if (this->neighborDirection(pk->getIntermediateSrcAddr()) == UP) // If Received from up ISL :
                    {
                        EV << "Received from UpISL: " << std::to_string((this->neighborDirection(pk->getIntermediateSrcAddr()))) << endl;
                        if(this->isDirectionFailed(EAST))
                        {
                            EV << "Sending on DownISL because direction EAST is failed" << endl;
                            // set EFailed, send on downISL.
                            pk->setEfailed(true);}
                        else
                        {
                            EV << "Sending on EastISL" << endl;
                            // send to east ISL
                            sendPacket(pk, EAST);
                        }
                        sendPacket(pk, DOWN);
                        }
                    else // not from up
                      {
                          EV << "Received from east ISL or received from downISL and Wfailed" << endl;
                          std::vector<int> APS = this->getAPS();
                          // clear Efailed
                          pk->setEfailed(false);
                          sendPacket(pk, DOWN);
                          sendPacket(pk, UP);
                    }
                }
            }
        }
        delete pk;
    }
}


void App::getCSVdata()
{
    int scenerio_num = getParentModule()->getParentModule()->par("scenerio_num");;
    std::string traffic_gen = "sheets/scenerio" + std::to_string(scenerio_num) + "_traffic_gen.csv";
    std::ifstream file(traffic_gen);
//    std::ifstream file("sheets/scenerio1_traffic_gen.csv"); // open the file for reading
    std::string line; // variable to store each line of the file

    // skip the first line of the file
    getline(file, line);

    // loop through each line of the file
    while (getline(file, line))
    {
        std::stringstream ss(line); // create a stringstream from the line
        std::string cell; // variable to store each field of the line
        int col = 0, address = 0, time = 0, amount = 0;
        std::string north;

        // loop through each field of the line
        while (std::getline(ss, cell, ',')) {
                    ++col;
                    if (col == 1) {
                        address = std::stoi(cell);
                    } else if (col == 2) {
                        time = std::stoi(cell);
                    } else if (col == 3) {
                        north = cell;
                    } else if (col == 4) {
                        amount = std::stoi(cell);
                    }
                    cell.clear();
                }
        // if the 4rd field matches the condition, store the 1th, 2th, and 3th fields of the line as int types
        if(address == myAddress && amount > 0)
        {
            int is_val_north;
            (north == "True") ? (is_val_north = 1) : (is_val_north = 0);
            std::vector<int> combined{time,is_val_north,amount};
            traffic.push_back(combined);
        }
    }
    // remove redundant vectors, most of the time there is no change, so keep only the changes
    //this->removeRedundancy();
}

void App::removeRedundancy()
{
    // Sort the vector of vectors by time
    std::sort(traffic.begin(), traffic.end(), [](const std::vector<int>& a, const std::vector<int>& b) {
        // Compare the time field of each vector
        return a[0] < b[0];
    });

    // Loop through the vector of vectors and remove redundant entries
    int previousStatus = -1; // Initialize previousStatus to an invalid value
    for (auto it = traffic.begin(); it != traffic.end(); ) {
        // Check if the status field of the current vector is different from the previous one,
        // or if the traffic field of the current vector is positive
        if ((*it)[1] != previousStatus || (*it)[2] > 0) {
            // Update previousStatus to the status of the current vector
            previousStatus = (*it)[1];
            // Move on to the next vector
            ++it;
        } else {
            // Remove the current vector from the vector of vectors
            it = traffic.erase(it);
        }
    }
}

void App::extractTrafficOfActive()
{
    int maxBitsperMsg = getParentModule()->getParentModule()->par("maxBitsperMsg");;
    int scenerio_num = getParentModule()->getParentModule()->par("scenerio_num");;
    std::string traffic_gen = "sheets/scenerio" + std::to_string(scenerio_num) + "_traffic_gen.csv";
    std::ifstream file(traffic_gen); // open the file for reading
    std::string line; // variable to store each line of the file
    std::vector<std::string> fields; // vector to store the fields of the current line

    // skip the first line of the file
    getline(file, line);

    // loop through each line of the file
    while (getline(file, line))
    {
        std::stringstream ss(line); // create a stringstream from the line
        std::string field; // variable to store each field of the line
        int field_count = 0; // variable to keep track of the current field number
        bool match_condition = false; // variable to keep track of whether the 4th field matches the condition

        fields.clear(); // clear the vector of fields for the current line

        // loop through each field of the line
        while (getline(ss, field, ','))
        {
            field_count++; // increment the field count
            if (field_count == 4)
            {
                // check if the 4th field is bigger than 0
                if (std::stoi(field) > 0)
                {
                    match_condition = true;
                }
            }
            fields.push_back(field); // store the field in the vector of fields for the current line
        }

        if (match_condition)
        {
            int numBitsLeft = std::stoi(fields[3]);
            int currentPkt = 1;
            while (numBitsLeft > maxBitsperMsg)
            {
                std::string key = fields[0] + "," + fields[1] + "," + std::to_string(currentPkt);
                this->activeTraffics[key] = false;
                currentPkt++;
                numBitsLeft -= maxBitsperMsg;
            }
            std::string key = fields[0] + "," + fields[1] + "," + std::to_string(currentPkt);
            this->activeTraffics[key] = false;
        }
    }
}

void App::extractSatelliteTimes()
{
    int scenerio_num = getParentModule()->getParentModule()->par("scenerio_num");;
    std::string sat_times = "sheets/scenerio" + std::to_string(scenerio_num) + "_sat_times.csv";
    std::ifstream file(sat_times);
//    std::ifstream file("sheets/scenerio1_sat_times.csv"); // open the file for reading
    std::string line; // variable to store each line of the file
    std::vector<std::string> fields; // vector to store the fields of the current line

    getline(file, line); // skip header

    while (getline(file, line)) {
        std::stringstream ss(line);
        std::string status, satellite, time_sec;
        getline(ss, status, ',');
        getline(ss, satellite, ',');
        getline(ss, time_sec);

        int sat = stoi(satellite);
        int time = stoi(time_sec);

        if (status == "START") {
            activeAddressesTimes[sat].first = time;
        } else if (status == "STOP") {
            activeAddressesTimes[sat].second = time;
        }
    }
    file.close();
}

void App::updateIsValNorth(int change) // CHANGED
{
    // get the data from CSV, and change color and icon
//    if(change == 1){
    getParentModule()->getDisplayString().setTagArg("i2",0,"status/up");
    getParentModule()->getDisplayString().setTagArg("i2",1,"blue");
    getParentModule()->getDisplayString().setTagArg("t",2,"blue");
//    }
//    else
//    {
//        getParentModule()->getDisplayString().setTagArg("i2",0,"status/down");
//        getParentModule()->getDisplayString().setTagArg("i2",1,"red");
//        getParentModule()->getDisplayString().setTagArg("t",2,"red");
//    }
    this->is_val_north = 1;// change;

}

void App::updateGUI(Packet* pk)
{
    // given a packet which arrived to its final destination, update all the relevant GUI elements
    if (hasGUI())
    {
        double pk_generation_time = pk->getCreationTime().dbl();
        int src_address =  pk->getSrcAddr();
        // get the current worst delay from the canvas
        cCanvas *canvas = getParentModule()->getParentModule()->getCanvas();
        cTextFigure *textFigure = check_and_cast<cTextFigure*>(canvas->getFigure("worstDelay"));
        double curr_worst_delay = (atof(&textFigure->getText()[27]));
        EV << curr_worst_delay <<endl;
        // did this packet was worse than before, if so update the canvas
        if (curr_worst_delay >= 0 && (curr_worst_delay < simTime().dbl() - pk_generation_time))
        {
            char label[1024];
            sprintf(label, "Worst End-to-End Delay(s): %.2f", simTime().dbl() - pk_generation_time);
            textFigure->setText(label);
        }
    }
}

void App::changeActiveStatus(bool start)
{
    if(start)
    {
        // I'm an active satellite, make me bigger and green
        getParentModule()->getDisplayString().setTagArg("is",0,"small");
        getParentModule()->getDisplayString().setTagArg("i",1,"#32CD32");
        getParentModule()->getDisplayString().setTagArg("i",2,66);

        // flig the flag
        isActive = true;
    }
    else
    {
        // I'm not active satellite,
        getParentModule()->getDisplayString().setTagArg("is",0,"vs");
        getParentModule()->getDisplayString().setTagArg("i",1,"#BABDB6");
        getParentModule()->getDisplayString().setTagArg("i",2,1);

        // flig the flag
        isActive = false;
    }
}

void App::getNeighbors()
{
    // function that gets all the neighbors of the module
    // and save if the state of the connection and direction
    for (int i = 0; i < this->getParentModule()->gateSize("port$o"); i++)
    {
        cGate *gate = this->getParentModule()->gate("port$o", i);
        cModule *connectedModule = gate->getNextGate()->getOwnerModule();
        int neighborAdd = connectedModule->par("address").intValue();
        bool connected = true;
        int neighborPlane = (neighborAdd)/100;
        int direction;
        //EV << neighborPlane << "\n";
        if (myPlane == neighborPlane)
        {
            if(myAddress%100 == getParentModule()->getParentModule()->par("num_of_sat_per_plane").intValue() && neighborAdd%100 == 1)
            {
                direction = UP;
            }
            else if(neighborAdd%100 == getParentModule()->getParentModule()->par("num_of_sat_per_plane").intValue() && myAddress%100 == 1)
            {
                direction = DOWN;
            }
            else
            {
                if(neighborAdd > myAddress)
                {
                    direction = UP;
                }
                else
                {
                    direction = DOWN;
                }
            }
        }
        else
        {
            if ( myPlane == getParentModule()->getParentModule()->par("num_of_planes").intValue())
            {
                if ( neighborPlane == 1)
                {
                    direction = EAST;
                }
                else
                {
                    direction = WEST;
                }
            }
            else if ( neighborPlane == getParentModule()->getParentModule()->par("num_of_planes").intValue())
            {
                if ( myPlane == 1)
                {
                    direction = WEST;
                }
                else
                {
                    direction = EAST;
                }
            }
            else
            {
                if(neighborPlane < myPlane)
                {
                    direction = WEST;
                }
                else
                {
                    direction = EAST;
                }
            }
        }
        neighbors.insert(std::make_pair(neighborAdd,std::make_pair(connected,direction)));
    }
    EV << "{ ";
    for (auto it = neighbors.begin(); it != neighbors.end(); ++it) {
        const auto& key = it->first;
        const auto& value = it->second;
        EV << key << ": (" << value.first << ", " << value.second << ") ";
    }
    EV << "}\n";
}

void App::getConnections()
{
    int scenerio_num = getParentModule()->getParentModule()->par("scenerio_num");;
    std::string connetions = "sheets/scenerio" + std::to_string(scenerio_num) + "_connections.csv";
    std::ifstream file(connetions);
    // get the connection to connectionsEvents that hold the next reconnection and disconnections of the module
//    std::ifstream file("sheets/scenerio1_connections.csv");
    std::string line;

    // skip the first line of the file
    getline(file, line);

    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (std::getline(iss, token, ',')) {
            tokens.push_back(token);
        }

        double start = std::stod(tokens[0]);
        double stop = std::stod(tokens[1]);
        int to = std::stoi(tokens[2]);
        int from = std::stoi(tokens[3]);
        int isAscending = std::stoi(tokens[4]);

        if (to == myAddress)
        {
            EV<< "My Address: "<<myAddress<< " from " << from <<endl;
            connectionsEvents.push_back(std::make_tuple(start, stop, from, isAscending));
        }
        if (from == myAddress)
        {
            EV<< "My Address: "<<myAddress<< " to " << to <<endl;
            connectionsEvents.push_back(std::make_tuple(start, stop, to, isAscending));
        }
    }

    // sort the connections by start time
    std::sort(connectionsEvents.begin(), connectionsEvents.end());

    for (const auto& event : connectionsEvents) {
        EV << std::get<0>(event) << ", " << std::get<1>(event) << ", " << std::get<2>(event) << ", " << std::get<3>(event) << std::endl;
    }

}

void App::disconnectWith(int neighborAddress)
{
    // update that this connection is now dead
    neighbors[neighborAddress].first = false;
    EV << "disconnected "<<myAddress << " with "<<neighborAddress<<endl;
    // update GUI
    for (int i = 0; i < this->getParentModule()->gateSize("port$o"); i++)
    {
        cGate *gate = this->getParentModule()->gate("port$o", i);
        cModule *connectedModule = gate->getNextGate()->getOwnerModule();
        if (connectedModule->par("address").intValue() == neighborAddress)
        {
            cDisplayString& connDispStr = gate->getDisplayString();
            connDispStr.parse("ls=,0,");
        }
    }
}

void App::reconnectWith(int neighborAddress)
{
    // update that this connection is now alive
    neighbors[neighborAddress].first = true;

    // update GUI
    EV << "connected "<< myAddress << " with " << neighborAddress<<endl ;
    for (int i = 0; i < this->getParentModule()->gateSize("port$o"); i++)
    {
        cGate *gate = this->getParentModule()->gate("port$o", i);
        cModule *connectedModule = gate->getNextGate()->getOwnerModule();
        if (connectedModule->par("address").intValue() == neighborAddress)
        {
            cDisplayString& connDispStr = gate->getDisplayString();
            connDispStr.parse("ls=,1,");
        }
    }
}

bool App::isNorth(int satAddress)
{
    int scenerio_num = getParentModule()->getParentModule()->par("scenerio_num");;
    std::string traffic_gen = "sheets/scenerio" + std::to_string(scenerio_num) + "_traffic_gen.csv";
    std::ifstream file(traffic_gen);
    // function that will return true if the given satellite address is moving north
//    std::ifstream file("sheets/scenerio1_traffic_gen.csv"); // open the file for reading
    std::string line; // variable to store each line of the file
    std::vector<std::string> fields; // vector to store the fields of the current line

    std::vector<std::vector<int>> satState;
    int maxTime = -1;
    int timestamp = -1;
    bool mostRecent = false;

    // skip the first line of the file
    getline(file, line);

    // loop through each line of the file
    while (getline(file, line))
    {
        std::stringstream ss(line); // create a stringstream from the line
        std::string field; // variable to store each field of the line
        int field_count = 0; // variable to keep track of the current field number
        bool match_condition = false; // variable to keep track of whether the 3rd field matches the condition

        fields.clear(); // clear the vector of fields for the current line

        // loop through each field of the line
        while (getline(ss, field, ','))
        {
            field_count++; // increment the field count
            if (field_count == 1)
            {
                // check if this is the 3rd field
                if (std::stoi(field) == satAddress)
                {
                    match_condition = true;
                }
            }
            fields.push_back(field); // store the field in the vector of fields for the current line
        }
        // get most recent value of is_val_north, that isn't bigger than the current time, and is of the given address
        timestamp = std::stoi(fields[1]);
        if(match_condition && timestamp <= simTime().dbl() && timestamp >= maxTime)
        {
            (fields[2] == "True") ? (mostRecent = true) : (mostRecent = false);
        }
    }
    return mostRecent;
}

std::vector<int> App::getASorMAS(bool giveAS)
{
    std::vector<int> set;
    for (auto it = activeAddressesTimes.begin(); it != activeAddressesTimes.end(); ++it) {
        const auto& key = it->first;
        const auto& value = it->second;
        if(giveAS && this->isNorth(key) && simTime().dbl() >= value.first && simTime().dbl() < value.second)
        {
            set.push_back(key);
        }
        if(!giveAS && !(this->isNorth(key)) && simTime().dbl() >= value.first && simTime().dbl() < value.second)   //we want MAS, so check if south
        {
            set.push_back(key);
        }
    }
    if(giveAS)
    {
        EV << "AS: ";
    }
    else
    {
        EV << "MAS: ";
    }
    for (auto & sat : set) {
        EV << sat << " ";
    }
    EV << endl;
    /*******************
    // get AS or MAS depending on giveAS
    std::vector<int> set;
    for (auto & activeSat : activeAddresses) {
        if(giveAS && this->isNorth(activeSat)) // we want AS, so check if north
        {
            set.push_back(activeSat);
        }
        if(!giveAS && !(this->isNorth(activeSat)))   //we want MAS, so check if south
        {
            set.push_back(activeSat);
        }
    }*/
    return set;
}

std::vector<int> App::getAPorMAP(bool giveAP)
{
    // get AP or MAP depending of giveAP
    // from AS and MAP
    std::vector<int> set = this->getASorMAS(giveAP);
    std::vector<int> planeSet;

    // get only the planes for the set
    for (auto & sat : set) {
        planeSet.push_back((sat) / 100);
    }
    // remove repeated planes
    sort( planeSet.begin(), planeSet.end() );
    planeSet.erase( unique( planeSet.begin(), planeSet.end() ), planeSet.end() );
    return planeSet;
}

std::vector<int> App::getAPS()
{
    std::vector<int> AP = this->getAPorMAP(true);
    std::vector<int> MAP = this->getAPorMAP(false);
    std::vector<int> APS = AP;

    // append the values from MAP to the end of the APS vector
    APS.insert(APS.end(), MAP.begin(), MAP.end());

    // sort the vector to bring duplicate values together
    std::sort(APS.begin(), APS.end());

    // remove duplicates
    auto last = std::unique(APS.begin(), APS.end());
    APS.erase(last, APS.end());
    EV << "APS: ";
    for (auto & aps : APS) {
        EV << aps << " ";
    }
    EV<< endl;
    return APS;
}

std::vector<int> App::calculateb1b2()
{
    // calclualte b1 and b2 from the algo
    // get AP and MAPs
    std::vector<int> AP = this->getAPorMAP(true);
    EV << "AP: ";
    for (auto & ap : AP) {
        EV << ap << " ";
    }
    EV << endl;
    std::vector<int> MAP = this->getAPorMAP(false);
    EV << "MAP: ";
    for (auto & map : MAP) {
        EV << map << " ";
    }
    EV << endl;

    std::vector<int> APS = this->getAPS();
    int max_dist = -1;
    int number_of_planes = getParentModule()->getParentModule()->par("num_of_planes");;
    int b1 = -1;
    int b2 = -1;
    sort(APS.begin(),APS.end());
    // find the farthest planes
    for (int i=0;i<APS.size()-1;i++)
    {
        int currentGap= APS[i+1] - APS[i];
        if(currentGap>max_dist)
        {
            max_dist=currentGap;
            b1=APS[i];
            b2=APS[i+1];
        }
    }
    // Check if there is a gap between the last and first numbers in the cycle
    int gapBetweenEndAndStart = APS.front() + (number_of_planes - APS.back());
    if (gapBetweenEndAndStart > max_dist) {
        max_dist = gapBetweenEndAndStart;
        b1 = APS.back();
        b2 = APS.front();
    }
    /*
    int min_dist = getParentModule()->getParentModule()->par("num_of_planes"); // initialize to maximum possible distance
    int number_of_panes = min_dist;
    int b1 = -1;
    int b2 = -1;

    // find the closest edges
    for (auto & ap : AP)
    {
        for (auto & map : MAP)
        {
            int dist = std::min(abs(ap-map), number_of_panes-abs(ap-map));  // minimum distance between the two planes
            if (dist < min_dist)
            { // update minimum distance and closest edge elements if a smaller distance is found
                min_dist = dist;
                b1 = ap;
                b2 = map;
            }
        }
    }
    */
    // return findings
    std::vector<int> edges;
    edges.push_back(b1);edges.push_back(b2);
    EV << "b1: " << b1 << ", b2; " << b2 << endl;
    return edges;
}

int App::convertToOrensIndex(int address)
{
    // given an address calculate the respondings orens index
    //return ((address%100) - 3 + getParentModule()->getParentModule()->par("num_of_sat_per_plane").intValue())%getParentModule()->getParentModule()->par("num_of_sat_per_plane").intValue();

    while(address>200)
    {
        address -= 97;
    }
    int canidate = (address % 100);
    if (canidate <= getParentModule()->getParentModule()->par("num_of_sat_per_plane").intValue())
    {
        return canidate;
    }
    else
    {
        return (canidate % (getParentModule()->getParentModule()->par("num_of_sat_per_plane").intValue() + 1)) + 1;
    }
}

void App::getOrensMapping()
{
    // saves all the mapping of orens index to the regular indexes
    for(int plane=1; plane <= getParentModule()->getParentModule()->par("num_of_planes").intValue(); plane++)
    {
        for(int index=1; index <= getParentModule()->getParentModule()->par("num_of_sat_per_plane").intValue(); index++)
        {
            orensMapping.push_back(std::make_tuple( this->convertToOrensIndex(plane*100 + index) , plane*100 +index ));
        }
    }
}

std::vector<int> App::calculateVU()
{
    // return VU (west and eastmost) indexes for algorithm in oren's notation
    std::vector <int>  orens;
    for (auto it = activeAddressesTimes.begin(); it != activeAddressesTimes.end(); ++it) {
        const auto& key = it->first;
        const auto& value = it->second;
        if(simTime().dbl() >= value.first && simTime().dbl() < value.second)
        {
            orens.push_back(this->convertToOrensIndex(key));
        }
    }
    /*
    for (auto& address: activeAddresses)
    {
        orens.push_back(this->convertToOrensIndex(address));
    }
    */
    sort( orens.begin(), orens.end() );
    orens.erase( unique( orens.begin(), orens.end() ), orens.end() );

    int max_dist = -1; // initialize to minimum possible distance
    int num_of_sat_per_plane = getParentModule()->getParentModule()->par("num_of_sat_per_plane");
    int c1 = -1;
    int c2 = -1;

    // Iterate through the sorted numbers and find the biggest gap
    for (int i = 0; i < orens.size() - 1; i++) {
        int currentGap = orens[i + 1] - orens[i];
        if (currentGap > max_dist) {
            max_dist = currentGap;
            c1 = orens[i];
            c2 = orens[i + 1];
        }
    }

    // Check if there is a gap between the last and first numbers in the cycle
    int gapBetweenEndAndStart = orens.front() + (num_of_sat_per_plane - orens.back());
    if (gapBetweenEndAndStart > max_dist) {
        max_dist = gapBetweenEndAndStart;
        c1 = orens.back();
        c2 = orens.front();
    }

    // we have the edges, now which one is or west
    // c1 and c2 can be either east,west or west,east
    // lets assume c1<c2
    if (c1>c2)
    {
        int temp = c1;
        c1 = c2;
        c2 = temp;
    }

    // check if each address is between c1 and c2, i.e ...c1...address...c2..
    bool in_between = false;
    for (auto & oren : orens)
    {
        if (c1 < oren && oren < c2)
        {
            in_between = true;
            break;
        }
    }

    std::vector<int> vu;
    if (in_between)
    {
        // then c1 is west and c2 is east
        vu.push_back(c1);
        vu.push_back(c2);
    }
    else
    {
        // then c1 is east and c2 is west
        vu.push_back(c2);
        vu.push_back(c1);
    }
    EV << "v: " << vu[0] << ", u; " << vu[1] << endl;
    return vu;
}


bool App::doesHaveInterPlane(int address)
{
    int scenerio_num = getParentModule()->getParentModule()->par("scenerio_num");;
    // check if the address have inter plane available now
    std::string connections = "sheets/scenerio" + std::to_string(scenerio_num) + "_connections.csv";
    std::ifstream file(connections);
//    std::ifstream file("sheets/scenerio1_connections.csv");
    std::string line;
    bool haveInter = false;

    // skip the first line of the file
    getline(file, line);

    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (std::getline(iss, token, ',')) {
            tokens.push_back(token);
        }

        double start = std::stod(tokens[0]);
        double stop = std::stod(tokens[1]);
        int to = std::stoi(tokens[2]);
        int from = std::stoi(tokens[3]);

        if ((to == address || from == address))
        {
            if ((start <= simTime().dbl()) &&  (stop > simTime().dbl()))
            {
                haveInter = true;
            }

        }
    }
    return haveInter;
}

bool App::doesOrenIndexHaveInterPlane(int orenIndex)
{
    // from orenIndex will find if the this index have inter plane connection
    bool haveInterPlane = true;
    for (auto & tuple : orensMapping)
    {
        if (std::get<0>(tuple) == orenIndex)
        {
            if(this->doesHaveInterPlane(std::get<1>(tuple)))
            {
                continue;
            }
            else
            {
                EV << "orens index : ";
                EV << std::get<0>(tuple) << endl;
                EV << "address : ";
                EV << std::get<1>(tuple) << endl;
                EV << this->doesHaveInterPlane(std::get<1>(tuple)) << endl;
                haveInterPlane = false;
                break;
            }

        }
    }
    return haveInterPlane;
}

int App::calculateJ(int vu)
{
    bool goLeft = getParentModule()->getParentModule()->par("goLeft");;
    int scenerio_num = getParentModule()->getParentModule()->par("scenerio_num");;
        if (scenerio_num == 2){
            int minPlane = 1;
        }
    // given u or v in orens notation, finds the corresponding j in orens notation
    if(!goLeft){
        int jindex = (((int) vu / 2 + 1) * 2) % (getParentModule()->getParentModule()->par("num_of_sat_per_plane").intValue());
            EV << "j: "  << jindex << endl;
            return jindex;
    }
    else if(goLeft)
    {
        int jindex = (((int) vu - 1)) % (getParentModule()->getParentModule()->par("num_of_sat_per_plane").intValue());
                            EV << "j: "  << jindex << endl;
                            return jindex;
    }
    return true;
}


Packet* App::generateNewPacket(int collectedData, int numPktperMsg) //new
{
    bool goLeft = getParentModule()->getParentModule()->par("goLeft");;
    int scenerio_num = getParentModule()->getParentModule()->par("scenerio_num");;
        if (scenerio_num == 2){
            int minPlane = 1;
        }
    char pkname[40];
    sprintf(pkname,"pk-%d", myAddress);
    Packet *pk = new Packet(pkname);
    pk->setBitLength(collectedData);
    pk->setSrcAddr(myAddress);
    pk->setIntermediateSrcAddr(myAddress);

    pk->setTTL(myTTL);
    pk->setReachedJ(false);
    pk->setB1(this->calculateb1b2()[0]);
    pk->setB2(this->calculateb1b2()[1]);
    pk->setV(this->calculateVU()[0]);
    pk->setU(this->calculateVU()[1]);
    if(!goLeft)
    {
       pk->setJ(this->calculateJ(pk->getU()));
    }
    else if(goLeft)
        {
           pk->setJ(this->calculateJ(pk->getV()));
        }


    pk->setnumPktperMsg(numPktperMsg);

    pk->setWfailed(true);
    pk->setEfailed(true);

    return pk;
}

int App::getISL(int to)
{
    int search_result = -1;
    for (const auto& neighbor : neighbors)
    {
        if (neighbor.second.first && neighbor.second.second == to)
        {
            search_result = neighbor.first;
            break;
        }
    }
    if (search_result == -1)
    {
        EV << "{ ";
        for (auto it = activeAddressesTimes.begin(); it != activeAddressesTimes.end(); ++it) {
            const auto& key = it->first;
            const auto& value = it->second;
            EV << key << ": (" << value.first << ", " << value.second << ") ";
        }
        EV << "}\n";
        std::string error = "Error: getISL: satellite"+ std::to_string(myAddress) + " didn't find: " + std::to_string(to);
        throw std::runtime_error(error);
    }
    else
    {
        return search_result;
    }
}

int App::neighborDirection(int neighborAddr)
{
    // from neighbor address, return the direction he is relative to the node
    return neighbors[neighborAddr].second;
}

bool App::isDirectionFailed(int dir)
{
    // check if the give direction is down, if it is, returns true
    for (const auto& item : neighbors)
    {
        if (item.second.second == dir)
        {
            return !item.second.first;
        }
    }
    return false;
}

void App::writeRecvCSV(double time, int sender, double latency, int numpckPerMsg,int hopCount,int bitLength)
{
    std::string fileName = "outfiles/recv" + std::to_string(myAddress) + ".csv";
    std::ofstream outFile(fileName, std::ios::app);
    if (!outFile.is_open())
    {
        throw cRuntimeError("Error opening file in Recv!");
    }
    outFile << time << "," << sender << "," << latency << "," << numpckPerMsg <<  "," << hopCount << "," <<bitLength <<"\n";
    outFile.close();
}

std::string App::getActiveAddressesAsString()
{
    std::string activeAddresses;
    for (const auto& entry : activeAddressesTimes)
    {
        int startTime = entry.second.first;
        int endTime = entry.second.second;
        if (simTime() >= startTime && simTime() <= endTime)
        {
            if (!activeAddresses.empty())
            {
                activeAddresses += ",";
            }
           activeAddresses += std::to_string(entry.first);
        }
    }

    return activeAddresses;
}

void App::writeSentCSV()
{
    std::string activeAddresses = this->getActiveAddressesAsString();
    std::string fileName = "outfiles/sent" + std::to_string(myAddress) + ".csv";
    std::ofstream outFile(fileName, std::ios::app);
    if (!outFile.is_open())
    {
        throw cRuntimeError("Error opening file in Sent!");
    }
    outFile << simTime().dbl() << "," << activeAddresses << "\n";
    outFile.close();
}

void App::sendPacket(Packet *pk, int direction)
{
    if (!this->isDirectionFailed(direction)) {
        pk->setDestAddr(this->getISL(direction));
        pk->setIntermediateSrcAddr(this->myAddress);
        send(pk->dup(),"out");
        if (direction == EAST){
            pk->setEfailed(false);
        }
        if (direction == WEST){
                    pk->setWfailed(false);
                }
    }

}
