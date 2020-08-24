// HDS OpenCL Miner
// OpenCL Host Interface
// Copyright 2020 The Hds Team
// Copyright 2020 Wilke Trei

#include <CL/cl.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <cstdlib>
#include <climits>

#include "hdsSolvers.h"
#include "hdsStratum.h"

#ifndef hdsMiner_H
#define hdsMiner_H

namespace hdsMiner {


class clHost {
	private:
	// OpenCL 
	vector<cl::Platform> platforms;  
	vector<cl::Context> contexts;
	vector<cl::Device> devices;
	vector<cl::Event> events;
	vector<cl::CommandQueue> queues;
	vector< uint32_t > deviceContext;

	// Statistics
	vector<int> solutionCnt;

	// To check if a mining thread stoped and we must resume it
	vector<bool> paused;

	// Callback data
	vector<clCallbackData> currentWork;
	bool restart = true;

	// Functions
	void detectPlatFormDevices(vector<int32_t>, bool);
	
	// The connector
	hdsStratum* stratum;

	hdsHashI_S   HdsHashI;
	hdsHashII_S  HdsHashII;
	hdsHashIII_S HdsHashIII;

	public:
	
	void setup(hdsStratum*, vector<int32_t>);
	void startMining();	
	void callbackFunc(cl_int, void*);
};

}

#endif
