//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2008 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

//
// Represents a packet in the network.
//
packet Packet
{
    int srcAddr;
    int intermediateSrcAddr;
    int destAddr;	// to be changed every hop
    
    // algos parameters
    int TTL;
    bool reachedJ;
    int b1;
    int b2;
    int u;
    int v;
    int j;
    bool Wfailed;
    bool Efailed;
    
    // statistics
    int hopCount;
}

message Control
{
    double time;
	int address;
	int IsAsending;
}
