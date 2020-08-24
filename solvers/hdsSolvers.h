// HDS OpenCL Miner
// Solver Interface & Header
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

#include "hdsUtil.h"

using namespace std;

#ifndef hdsSolver_h
#define hdsSolver_h

namespace hdsMiner {

class hdsSolver {
	protected:
	// OpenCL 
	vector< vector<cl::Buffer> > buffers;
	vector< vector<cl::Kernel> > kernels;

	vector< uint32_t* > results;

	// Functions
	virtual void loadAndCompileKernel(cl::Context &, cl::Device &, uint32_t) = 0;
	
	public:
	
	// Setup function for the PoW scheme
	void setup(vector<cl::Device> &devices, vector<cl::Context> &contexts, vector<uint32_t> &contextMap) {
		buffers.resize(devices.size());
		kernels.resize(devices.size());
		results.resize(devices.size());

		for (uint32_t i=0; i<devices.size(); i++) {
			loadAndCompileKernel(contexts[contextMap[i]], devices[i], i);
		}
	}

	// Destructor to free memory
	void stop(uint32_t gpu) {
		buffers[gpu].clear();
		kernels[gpu].clear();
	}

	uint32_t * getResults(uint32_t gpu) {
		return results[gpu];
	}

	virtual void createBuffers(cl::Context &, cl::Device &, uint32_t) = 0;
	virtual void queueKernels(cl::CommandQueue *, uint32_t,  cl::Event *, clCallbackData *) = 0;
	
	void unmapResult(cl::CommandQueue * queue, uint32_t gpu) {
		queue->enqueueUnmapMemObject(buffers[gpu][buffers[gpu].size() - 1], results[gpu], NULL, NULL);
	}
};

class hdsHashI_S : public hdsSolver {
	private:
	void loadAndCompileKernel(cl::Context &, cl::Device &, uint32_t);

	public:
	void createBuffers(cl::Context &, cl::Device &, uint32_t);
	void queueKernels(cl::CommandQueue *, uint32_t,  cl::Event *, clCallbackData *);
	void unmapResult(uint32_t);
};

class hdsHashII_S : public hdsSolver {
	private:
	void loadAndCompileKernel(cl::Context &, cl::Device &, uint32_t);

	public:
	void createBuffers(cl::Context &, cl::Device &, uint32_t);
	void queueKernels(cl::CommandQueue *, uint32_t,  cl::Event *, clCallbackData *);
	void unmapResult(uint32_t);
};

class hdsHashIII_S : public hdsSolver {
	private:
	void loadAndCompileKernel(cl::Context &, cl::Device &, uint32_t);

	public:
	void createBuffers(cl::Context &, cl::Device &, uint32_t);
	void queueKernels(cl::CommandQueue *, uint32_t,  cl::Event *, clCallbackData *);
	void unmapResult(uint32_t);
};



}

#endif
