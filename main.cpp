// HDS OpenCL Miner
// Main Function
// Copyright 2020 The Hds Team
// Copyright 2020 Wilke Trei

#include "hdsStratum.h"
#include "clHost.h"
#include "hdsUtil.h"

// Defining global variables
const string StrVersionNumber = "v2.0";
const string StrVersionDate = "April 20th 2020";

inline vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while(getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


inline vector<string> split(const string &s, char delim) {
    vector<string> elems;
    return split(s, delim, elems);
}

uint32_t cmdParser(vector<string> args, string &host, string &port, string &apiCred, bool &debug, hdsMiner::solverType &forcedSolver, vector<int32_t> &devices) {
	bool hostSet = false;
	bool apiSet = false;
	
	for (uint32_t i=0; i<args.size(); i++) {
		if (args[i][0] == '-') {
			if ((args[i].compare("-h")  == 0) || (args[i].compare("--help")  == 0)) {
				return 0x4;
			}

			if (args[i].compare("--server")  == 0) {
				if (i+1 < args.size()) {
					vector<string> tmp = split(args[i+1], ':');
					if (tmp.size() == 2) {
						host = tmp[0];
						port = tmp[1];
						hostSet = true;	
						i++;
						continue;
					}
				}
			}

			if (args[i].compare("--key")  == 0) {
				if (i+1 < args.size()) {
					apiCred = args[i+1];
					apiSet = true;
					i++;
					continue;
				}
			}

			if (args[i].compare("--devices")  == 0) {
				if (i+1 < args.size()) {
					vector<string> tmp = split(args[i+1], ',');
					for (int j=0; j<tmp.size(); j++) {
						devices.push_back(stoi(tmp[j]));
					}
					continue;
				}
			}
			
			if (args[i].compare("--hdsHashI")  == 0) {
				forcedSolver = hdsMiner::HdsI;
			}

			if (args[i].compare("--hdsHashII")  == 0) {
				forcedSolver = hdsMiner::HdsII;
			}

			if (args[i].compare("--hdsHashIII")  == 0) {
				forcedSolver = hdsMiner::HdsIII;
			}

			if (args[i].compare("--debug")  == 0) {
				debug = true;
			}
	
			if (args[i].compare("--version")  == 0) {
				cout << StrVersionNumber << " for HDS main network " << "(" << StrVersionDate << ")" << endl;
				exit(0);
			}
		}
	}

	uint32_t result = 0;
	if (!hostSet) result += 1;
	if (!apiSet) result += 2;

	if (devices.size() == 0) devices.assign(1,-1);
	sort(devices.begin(), devices.end());
	
	return result;
}

int main(int argc, char* argv[]) {

	

	vector<string> cmdLineArgs(argv, argv+argc);
	string host;
	string port;
	string apiCred;
	bool debug = false;
	hdsMiner::solverType forcedSolver = hdsMiner::None;
	bool useTLS = true;
	vector<int32_t> devices;


	uint32_t parsing = cmdParser(cmdLineArgs, host, port, apiCred, debug, forcedSolver, devices);

	cout << "-====================================-" << endl;
	cout << "                                      " << endl;
	cout << "       HdsHash III OpenCL Miner      " << endl;
	cout << "       version:   " + StrVersionNumber << endl;
	cout << "       date:      " + StrVersionDate << endl;
	cout << "                                      " << endl;
	cout << "-====================================-" << endl;
	cout << "" << endl;	
	cout << "Parameters: " << endl;
	cout << " --server:      " << host << ":" << port << endl;	
	cout << " --key:         " << apiCred << endl;
	cout << " --debug:       " << std::boolalpha << debug << endl;

	if (parsing != 0) {
		if (parsing & 0x1) {
			cout << "Error: Parameter --server missing" << endl;
		}

		if (parsing & 0x2) {
			cout << "Error: Parameter --key missing" << endl;
		}

		cout << "Parameters: " << endl;
		cout << " --help / -h 			Showing this message" << endl;
		cout << " --server <server>:<port>	The HDS stratum server and port to connect to (required)" << endl;
		cout << " --key <key>			The HDS stratum server API key (required), on a Hds mining pool the user name / wallet addres" << endl;
		cout << " --devices <numbers>		A comma seperated list of devices that should be used for mining (default: all in system)" << endl; 
		cout << " --hdsHashI			Force mining Hds Hash I" << endl;
		cout << " --hdsHashII			Force mining Hds Hash II" << endl;
		cout << " --hdsHashIII			Force mining Hds Hash III" << endl;
		cout << " --debug			Enable debug mode - verbose stratum information will be displayed" << endl;
		cout << " --version			Prints the version number" << endl;
		exit(0);
	}

	hdsMiner::hdsStratum myStratum(host, port, apiCred, debug, forcedSolver);

	hdsMiner::clHost myClHost;
	
	cout << endl;
	cout << "Setup OpenCL devices:" << endl;
	cout << "=====================" << endl;
	
	myClHost.setup(&myStratum, devices);

	cout << endl;
	cout << "Waiting for work from stratum:" << endl;
	cout << "==============================" << endl;

	myStratum.startWorking();

	while (!myStratum.hasWork()) {
		this_thread::sleep_for(std::chrono::milliseconds(200));
	}

	cout << endl;
	cout << "Start mining:" << endl;
	cout << "=============" << endl;
	myClHost.startMining();
}

#if defined(_MSC_VER) && (_MSC_VER >= 1900)

FILE _iob[] = { *stdin, *stdout, *stderr };
extern "C" FILE * __cdecl __iob_func(void) { return _iob; }

#endif
