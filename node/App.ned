//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2008 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//

package node;

//
// Application model to generate traffic for the network.
//
simple App like IApp
{
    parameters:
        int address;  // local node address
        int ttl;
        @display("i=block/browser");
        @signal[endToEndDelay](type="simtime_t");
        @signal[hopCount](type="int");
        @signal[sourceAddress](type="int");
        @statistic[endToEndDelay](title="end-to-end delay of arrived packets";unit=s;record=vector,histogram,mean,max;interpolationmode=none);
        @statistic[hopCount](title="hop count of arrived packets";interpolationmode=none;record=histogram,mean,max);
        @statistic[sourceAddress](title="source address of arrived packets";interpolationmode=none;record=histogram);
    gates:
        input in;
        output out;
}


