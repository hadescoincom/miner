

#include "core/difficulty.h"

#ifndef hdsUtil_h
#define hdsUtil_h

namespace hdsMiner {

	enum solverType {All, None, HdsI, HdsII, HdsIII};

	struct WorkDescription 	{
		solverType solver;
		int64_t workId;
		uint64_t nonce;
		uint64_t work[4];
		hds::Difficulty powDiff;
	};

	struct clCallbackData {
		void* host;
		uint32_t gpuIndex;
		solverType currentSolver=None;
		WorkDescription wd;
	};

}


#endif
