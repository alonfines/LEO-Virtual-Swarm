//
// This file is part of an OMNeT++/OMNEST simulation example.
//
// Copyright (C) 1992-2008 Andras Varga
//
// This file is distributed WITHOUT ANY WARRANTY. See the file
// `license' for details on this and other legal matters.
//
package networks;

import node.Node;
import ned.DatarateChannel;

//
// A network topology commonly known as the "NTT backbone".
//
network NetLEO
{
    parameters:
        // Plane settings
        int num_of_planes;
        int num_of_sat_per_plane;
        int num_of_hosts = num_of_planes*num_of_sat_per_plane;
        int phase_deg;
        int intra_plane_bias;
		int inter_plane_bias;
        string active_sats;	// String that hold the active satellites 
        volatile double hoptime @unit(ms) = default(50ms);
        double bandwidthMbps @unit(Mbps) = default(10Mbps);//10Mbps
		int scenerio_num;
		bool goLeft;
		int maxBitsperMsg;
		
		// Grid settings
		double distance_between_sats @unit(m);
        double maxY @unit(m) = (num_of_sat_per_plane+1)*(tan(phase_deg*3.1415/180))*distance_between_sats;
        double maxX @unit(m) = (num_of_sat_per_plane + 3*(num_of_planes-1))*distance_between_sats;
        double minX @unit(m) = 0m;
        double minY @unit(m) = 0m;
        
		@figure[worstDelay](type=text; pos=500,2000; font=1000,1000,bold; color="RED"; text="");

        @display("bgb=$maxX,$maxY;bgu=m");
    types:
        channel C extends DatarateChannel
        {
            parameters:
                delay = hoptime;
                datarate = bandwidthMbps;
                @display("ls=,1");
        }
        channel Circular extends DatarateChannel
        {
            parameters:
                delay = hoptime;
                datarate = bandwidthMbps;
                @display("ls=#729FCF,3,da");
        }
    submodules:
        rte[num_of_hosts]: Node {
            parameters:
            	
                address = inter_plane_bias*100 + int (index / num_of_sat_per_plane)*100 + ((index+intra_plane_bias) % num_of_sat_per_plane)+1;

				posX = distance_between_sats / 2 + (index%num_of_sat_per_plane)*distance_between_sats + int(index / num_of_sat_per_plane)*distance_between_sats*3;
				posY = maxY -distance_between_sats / 2 - (index%num_of_sat_per_plane)*tan(phase_deg*3.1415/180)*distance_between_sats ;
        }
    connections allowunconnected:
        for j=0..num_of_planes-1, for i=0..num_of_sat_per_plane-1 {
            // inter plane
            rte[num_of_sat_per_plane*j+i].port++ <--> C <--> rte[num_of_sat_per_plane*j+i+1].port++ if (i!=num_of_sat_per_plane-1);
            // lop last in plane to first in plain
            rte[num_of_sat_per_plane*j+i].port++ <--> Circular <--> rte[num_of_sat_per_plane*j].port++ if (i == (num_of_sat_per_plane -1));
                       
            // intra plane
            // each non last-plane, to the next plane, minus 2
            rte[num_of_sat_per_plane*j+i].port++ <--> C <--> rte[num_of_sat_per_plane*(j+1) + i - 2].port++ if  (j!=int(num_of_planes)-1 && i >= 2);
            //rte[num_of_sat_per_plane*j+i].port++ <--> C <--> rte[num_of_sat_per_plane*(j+1)].port++ if  (j!=int(num_of_planes)-1 && i == 2);
            // if im the first, I dont have "minus 2", loop from the back
            rte[num_of_sat_per_plane*j+i].port++ <--> C <--> rte[num_of_sat_per_plane*(j+1) + num_of_sat_per_plane - 2 + i].port++ if  (j!=int(num_of_planes)-1 &&  i < 2);
			
			// edge cases - intra plane
			// last plane, connect to first plane minus 2
			rte[num_of_sat_per_plane*j+i].port++ <--> C <--> rte[i - 2].port++ if  (j==int(num_of_planes)-1 && i >= 2 );
			// we are connecting to the first plane minus 2, but if the index is already smaller then 2, loop from the back again
			rte[num_of_sat_per_plane*j+i].port++ <--> C <--> rte[num_of_sat_per_plane + i - 2].port++ if  (j==int(num_of_planes)-1 &&  i < 2 );	
        }
}

