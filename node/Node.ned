//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2008 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//


package node;
import inet.mobility.contract.IMobility;
import inet.mobility.single.TurtleMobility;
import inet.mobility.static.StaticGridMobility;
//
// A "Node" consists of a Routing plus an App module which represents the
// aggregate traffic of the subnets connected to that router.
//
module Node
{
    parameters:
        double posX @unit(m);
        double posY @unit(m);
        int address;

        string appType;
        @display("p=$posX,$posY;i=misc/node,#BABDB6;is=vs;bgb=368,346");
    gates:
        inout port[];
    submodules:
        app: <appType> like IApp {
            parameters:
                address = address;
                @display("p=248,50");
        }
        routing: Routing {
            parameters:
                @display("p=230,179");
            gates:
                in[sizeof(port)];
                out[sizeof(port)];
        }
        queue[sizeof(port)]: L2Queue {
            parameters:
                @display("p=80,200,row");
        }
        //grid: StaticGridMobility {
        //    @display("p=45,233");
        //}
        //mobility: StaticGridMobility {
        //    @display("p=116,233");
        //}
    connections:
        routing.localOut --> app.in;
        routing.localIn <-- app.out;
        for i=0..sizeof(port)-1 {
            routing.out[i] --> queue[i].in;
            routing.in[i] <-- queue[i].out;
            queue[i].line <--> port[i];
        }
}


