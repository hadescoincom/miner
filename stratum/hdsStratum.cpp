// HDS OpenCL Miner
// Stratum interface class
// Copyright 2020 The Hds Team
// Copyright 2020 Wilke Trei


#include "hdsStratum.h"
#include "crypto/sha256.c"
#include "crypto/blake2b.h"

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe32(x) OSSwapHostToBigInt32(x)
#endif

namespace hdsMiner {

// This one ensures that the calling thread can work on immediately
void hdsStratum::queueDataSend(string data) {
	io_service.post(boost::bind(&hdsStratum::syncSend,this, data));
}

// Function to add a string into the socket write queue
void hdsStratum::syncSend(string data) {
	writeRequests.push_back(data);
	activateWrite();
}


// Got granted we can write to our connection, lets do so	
void hdsStratum::activateWrite() {
	if (!activeWrite && writeRequests.size() > 0) {
		activeWrite = true;

		string json = writeRequests.front();
		writeRequests.pop_front();

		std::ostream os(&requestBuffer);
		os << json;
		if (debug) cout << "Write to connection: " << json;

		boost::asio::async_write(*socket, requestBuffer, boost::bind(&hdsStratum::writeHandler,this, boost::asio::placeholders::error));
	}
}
	

// Once written check if there is more to write
void hdsStratum::writeHandler(const boost::system::error_code& err) {
	activeWrite = false;
	activateWrite(); 
	if (err) {
		if (debug) cout << "Write to stratum failed: " << err.message() << endl;
	} 
}


// Called by main() function, starts the stratum client thread
void hdsStratum::startWorking(){
	t_start = time(NULL);
	std::thread (&hdsStratum::connect,this).detach();
}

// This function will be used to establish a connection to the API server
void hdsStratum::connect() {
	while (true) {
		tcp::resolver::query q(host, port); 

		cout << "Connecting to " << host << ":" << port << endl;
		try {
	    		tcp::resolver::iterator endpoint_iterator = res.resolve(q);
			tcp::endpoint endpoint = *endpoint_iterator;
			socket.reset(new boost::asio::ssl::stream<tcp::socket>(io_service, context));

			socket->set_verify_mode(boost::asio::ssl::verify_none);
    			socket->set_verify_callback(boost::bind(&hdsStratum::verifyCertificate, this, _1, _2));

			socket->lowest_layer().async_connect(endpoint,
			boost::bind(&hdsStratum::handleConnect, this, boost::asio::placeholders::error, ++endpoint_iterator));

			io_service.run();
		} catch (std::exception const& _e) {
			 cout << "Stratum error: " <<  _e.what() << endl;
		}

		workId = -1;
		io_service.reset();
		socket->lowest_layer().close();

		cout << "Lost connection to HDS stratum server" << endl;
		cout << "Trying to connect in 5 seconds"<< endl;

		std::this_thread::sleep_for(std::chrono::seconds(5));
	}		
}


// Once the physical connection is there start a TLS handshake
void hdsStratum::handleConnect(const boost::system::error_code& err, tcp::resolver::iterator endpoint_iterator) {
	if (!err) {
	cout << "Node connection: ok" << endl;

      	// The connection was successful. Do the TLS handshake
	socket->async_handshake(boost::asio::ssl::stream_base::client,boost::bind(&hdsStratum::handleHandshake, this, boost::asio::placeholders::error));
	
    	} else if (err != boost::asio::error::operation_aborted) {
		if (endpoint_iterator != tcp::resolver::iterator()) {
			// The endpoint did not work, but we can try the next one
			tcp::endpoint endpoint = *endpoint_iterator;

			socket->lowest_layer().async_connect(endpoint,
			boost::bind(&hdsStratum::handleConnect, this, boost::asio::placeholders::error, ++endpoint_iterator));
		} 
	} 	
}


// Dummy function: we will not verify if the endpoint is verified at the moment,
// still there is a TLS handshake, so connection is encrypted
bool hdsStratum::verifyCertificate(bool preverified, boost::asio::ssl::verify_context& ctx){
	return true;
}


void hdsStratum::handleHandshake(const boost::system::error_code& error) {
	if (!error) {
		// Listen to receive stratum input
		boost::asio::async_read_until(*socket, responseBuffer, "\n",
		boost::bind(&hdsStratum::readStratum, this, boost::asio::placeholders::error));

		cout << "TLS Handshake:   ok" << endl;
		
		// The connection was successful. Send the login request
		std::stringstream json;
		json << "{\"method\":\"login\", \"api_key\":\"" << apiKey << "\", \"id\":\"login\",\"jsonrpc\":\"2.0\"} \n";
		queueDataSend(json.str());	
	} else {
		cout << "Handshake failed: " << error.message() << "\n";
	}
}


// Simple helper function that casts a hex string into byte array
vector<uint8_t> parseHex (string input) {
	vector<uint8_t> result ;
	result.reserve(input.length() / 2);
	for (uint32_t i = 0; i < input.length(); i += 2){
		uint32_t byte;
		std::istringstream hex_byte(input.substr(i, 2));
		hex_byte >> std::hex >> byte;
		result.push_back(static_cast<unsigned char>(byte));
	}
	return result;
}


// Main stratum read function, will be called on every received data
void hdsStratum::readStratum(const boost::system::error_code& err) {
	if (!err) {
		// We just read something without problem.
		std::istream is(&responseBuffer);
		std::string response;
		getline(is, response);

		if (debug) cout << "Incomming Stratum: " << response << endl;

		// Parse the input to a property tree
		pt::iptree jsonTree;
		try {
			istringstream jsonStream(response);
			pt::read_json(jsonStream,jsonTree);

			// This should be for any valid stratum
			if (jsonTree.count("method") > 0) {	
				string method = jsonTree.get<string>("method");
			
				// Result to a node request
				if (method.compare("result") == 0) {
					// A login reply
					if (jsonTree.get<string>("id").compare("login") == 0) {
						int32_t code = jsonTree.get<int32_t>("code");
						if (code >= 0) {
							cout << "Login at node accepted \n" << endl;
							if (jsonTree.count("nonceprefix") > 0) {
								string poolNonceStr = jsonTree.get<string>("nonceprefix");
								poolNonce = parseHex(poolNonceStr);
							} else {
								poolNonce.clear();
							}

							if (jsonTree.count("forkheight") > 0) {
								forkHeight = jsonTree.get<uint64_t>("forkheight");
							}

							if (jsonTree.count("forkheight2") > 0) {
								forkHeight2 = jsonTree.get<uint64_t>("forkheight2");
							}
						} else {
							cout << "Error: Login at node not accepted. Closing miner." << endl;
							exit(0);
						}	
					} else {	// A share reply
						int32_t code = jsonTree.get<int32_t>("code");
						if (code == 1) {
							cout << "Solution for work id " << jsonTree.get<string>("id") << " accepted" << endl;
							sharesAcc++;
						} else {
							cout << "Warning: Solution for work id " << jsonTree.get<string>("id") << " rejected" << endl;
							sharesRej++;
						}
					}
				}

				// A new job decription;
				if (method.compare("job") == 0) {
					updateMutex.lock();
					// Get new work load
					string work = jsonTree.get<string>("input");
					serverWork = parseHex(work);

					// Get jobId of new job
					workId =  jsonTree.get<uint64_t>("id");	
					
					// Get the target difficulty
					uint32_t stratDiff =  jsonTree.get<uint32_t>("difficulty");
					powDiff = hds::Difficulty(stratDiff);

					// Nicehash support
					if (jsonTree.count("nonceprefix") > 0) {
						string poolNonceStr = jsonTree.get<string>("nonceprefix");
						poolNonce = parseHex(poolNonceStr);
					}

					// Block Height for fork detection
					if (jsonTree.count("height") > 0) {
						blockHeight = jsonTree.get<uint64_t>("height");
						if ((blockHeight == forkHeight) || (blockHeight == forkHeight2)) cout << endl << "-= PoW fork height reached. Switching algorithm =-" << endl << endl;
					}


					updateMutex.unlock();	

					cout << "New job: " << workId << "  Difficulty: " << std::fixed << std::setprecision(0) << powDiff.ToFloat() << endl;
					cout << "Solutions (Accepted/Rejected): " << sharesAcc << " / " << sharesRej << " Uptime: " << (int)(t_current-t_start) << " sec" << endl; 	
				}

				// Cancel a running job
				if (method.compare("cancel") == 0) {
					updateMutex.lock();
					// Get jobId of canceled job
					uint64_t id =  jsonTree.get<uint64_t>("id");
					// Set it to an unlikely value;
					if (id == workId) workId = -1;
					updateMutex.unlock();
				}
				t_current = time(NULL);
			}

			

		} catch(const pt::ptree_error &e) {
			cout << "Json parse error when reading Stratum node: " << e.what() << endl; 
		}

		// Prepare to continue reading
		boost::asio::async_read_until(*socket, responseBuffer, "\n",
        	boost::bind(&hdsStratum::readStratum, this, boost::asio::placeholders::error));
	}
}


// Checking if we have valid work, else the GPUs will pause
bool hdsStratum::hasWork() {
	return (workId >= 0);
}


void hdsStratum::Blake2B_HdsIII(WorkDescription * wd) {
	blake2bInstance blakeInst;
	blakeInst.init(32,448,5, "Hds--PoW");

	uint8_t msg[128] = {0};
	memcpy(&msg[0], (uint8_t *) &wd->work, 32);
	memcpy(&msg[32], (uint8_t *) &wd->nonce, 8);
	memcpy(&msg[40], &extraNonce[0], 4);

	blakeInst.update(&msg[0], 44, 1);	
	blakeInst.ret_final((uint8_t*) &wd->work, 32);		
}


// function the clHost class uses to fetch new work
void hdsStratum::getWork(WorkDescription& wd, solverType * solver) {

	// nonce is atomic, so every time we call this will get a nonce increased by one
	uint64_t cliNonce = nonce.fetch_add(1);

	uint8_t* noncePoint = (uint8_t*) &wd.nonce;

	uint32_t poolNonceBytes = min<uint32_t>(poolNonce.size(), 6); 	// Need some range left for miner
	wd.nonce = (cliNonce << 8*poolNonceBytes);

	for (uint32_t i=0; i<poolNonceBytes; i++) {			// Prefix pool nonce
		noncePoint[i] = poolNonce[i];
	}
	
	updateMutex.lock();

	wd.workId = workId;
	wd.powDiff = powDiff;

	uint64_t limit = numeric_limits<uint64_t>::max();
	//wd.forceHdsHashI = (blockHeight < limit) && (forkHeight < limit) && (blockHeight < forkHeight);
	
	solverType thisSolver;
	if (forcedSolver != None) {
		thisSolver = forcedSolver;
	} else if ((blockHeight < limit) && (forkHeight < limit) && (blockHeight < forkHeight)) {
		thisSolver = HdsI;
	} else if ((blockHeight < limit) && (forkHeight < limit) && (blockHeight >= forkHeight) && (blockHeight < forkHeight2)) {
		thisSolver = HdsII;
	} else {
		thisSolver = HdsIII;
	}
		
	*solver = thisSolver;
	wd.solver = thisSolver;
	
	memcpy(wd.work, serverWork.data(), 32);

	if (*solver == HdsIII) {
		Blake2B_HdsIII(&wd);
	}

	updateMutex.unlock();
}


void CompressArray(const unsigned char* in, size_t in_len,
                   unsigned char* out, size_t out_len,
                   size_t bit_len, size_t byte_pad) {
	assert(bit_len >= 8);
	assert(8*sizeof(uint32_t) >= bit_len);

	size_t in_width { (bit_len+7)/8 + byte_pad };
	assert(out_len == (bit_len*in_len/in_width + 7)/8);

	uint32_t bit_len_mask { ((uint32_t)1 << bit_len) - 1 };

	// The acc_bits least-significant bits of acc_value represent a bit sequence
	// in big-endian order.
	size_t acc_bits = 0;
	uint32_t acc_value = 0;

	size_t j = 0;
	for (size_t i = 0; i < out_len; i++) {
		// When we have fewer than 8 bits left in the accumulator, read the next
		// input element.
		if (acc_bits < 8) {
			if (j < in_len) {
				acc_value = acc_value << bit_len;
				for (size_t x = byte_pad; x < in_width; x++) {
					acc_value = acc_value | (
					(
					// Apply bit_len_mask across byte boundaries
					in[j + x] & ((bit_len_mask >> (8 * (in_width - x - 1))) & 0xFF)
					) << (8 * (in_width - x - 1))); // Big-endian
				}
				j += in_width;
				acc_bits += bit_len;
			}
			else {
				acc_value <<= 8 - acc_bits;
				acc_bits += 8 - acc_bits;;
			}
		}

		acc_bits -= 8;
		out[i] = (acc_value >> acc_bits) & 0xFF;
	}
}

#ifdef WIN32

inline uint32_t htobe32(uint32_t x)
{
    return (((x & 0xff000000U) >> 24) | ((x & 0x00ff0000U) >> 8) |
        ((x & 0x0000ff00U) << 8) | ((x & 0x000000ffU) << 24));
}


#endif // WIN32

// Big-endian so that lexicographic array comparison is equivalent to integer comparison
void EhIndexToArray(const uint32_t i, unsigned char* array) {
	static_assert(sizeof(uint32_t) == 4, "");
	uint32_t bei = htobe32(i);
	memcpy(array, &bei, sizeof(uint32_t));
}


// Helper function that compresses the solution from 32 unsigned integers (128 bytes) to 104 bytes
std::vector<unsigned char> GetMinimalFromIndices(std::vector<uint32_t> indices, size_t cBitLen) {
	assert(((cBitLen+1)+7)/8 <= sizeof(uint32_t));
	size_t lenIndices { indices.size()*sizeof(uint32_t) };
	size_t minLen { (cBitLen+1)*lenIndices/(8*sizeof(uint32_t)) };
	size_t bytePad { sizeof(uint32_t) - ((cBitLen+1)+7)/8 };
	std::vector<unsigned char> array(lenIndices);
	for (size_t i = 0; i < indices.size(); i++) {
		EhIndexToArray(indices[i], array.data()+(i*sizeof(uint32_t)));
	}
	std::vector<unsigned char> ret(minLen);
	CompressArray(array.data(), lenIndices, ret.data(), minLen, cBitLen+1, bytePad);
	return ret;
}

std::vector<uint8_t> hdsStratum::packHdsIII(std::vector<uint32_t> &solverOutput) {
	std::bitset<800> inStream;
	std::bitset<800> mask(0xFF);

	inStream.reset();
	inStream |= (solverOutput[28] & 0xFFFF);

	for (int32_t i = 27; i>=16; i--) {
		inStream = (inStream << 32);
		inStream |= solverOutput[i];
	}

	inStream = (inStream << 16);
	inStream |= (solverOutput[12] & 0xFFFF);

	for (int32_t i = 11; i>=0; i--) {
		inStream = (inStream << 32);
		inStream |= solverOutput[i];
	}


	std::vector<uint8_t> res;
	for (uint32_t i=0; i<100; i++) {
		res.push_back((uint8_t) (inStream & mask).to_ulong() );
		inStream = (inStream >> 8);
	}


	// Adding the extra nonce
	for (uint32_t i=0; i<4; i++) res.push_back(extraNonce[i]);

	return res;
	
}


std::vector<uint32_t> GetIndicesFromMinimal(std::vector<uint8_t> soln) {
	std::bitset<800> inStream;
	std::bitset<800> mask((1 << (24+1))-1);

	inStream.reset();
	for (int32_t i = 99; i>=0; i--) {
		inStream = (inStream << 8);
		inStream |= (uint64_t) soln[i];
	}

	std::vector<uint32_t> res;
	for (uint32_t i=0; i<32; i++) {
		res.push_back((uint32_t) (inStream & mask).to_ulong() );
		inStream = (inStream >> (24+1));
	}

	return res;
}


bool hdsStratum::testSolution(const hds::Difficulty& diff, const vector<uint32_t>& indices, vector<uint8_t>& compressed) {

	// get the compressed representation of the solution and check against target

	hds::uintBig_t<32> hv;
	Sha256_Onestep(compressed.data(), compressed.size(), hv.m_pData);

	return diff.IsTargetReached(hv);
}

void hdsStratum::submitSolution(int64_t wId, uint64_t nonceIn, const std::vector<uint8_t>& compressed) {

	// The solutions target is low enough, lets submit it
	vector<uint8_t> nonceBytes;
			
	nonceBytes.assign(8,0);
	*((uint64_t*) nonceBytes.data()) = nonceIn;

	stringstream nonceHex;
	for (int c=0; c<nonceBytes.size(); c++) {
		nonceHex << std::setfill('0') << std::setw(2) << std::hex << (unsigned) nonceBytes[c];
	}

	stringstream solutionHex;
	for (int c=0; c<compressed.size(); c++) {
		solutionHex << std::setfill('0') << std::setw(2) << std::hex << (unsigned) compressed[c];
	}	
			
	// Line the stratum msg up
	std::stringstream json;
	json << "{\"method\" : \"solution\", \"id\": \"" << wId << "\", \"nonce\": \"" << nonceHex.str() 
			<< "\", \"output\": \"" << solutionHex.str() << "\", \"jsonrpc\":\"2.0\" } \n";

	queueDataSend(json.str());	

	cout << "Submitting solution to job " << wId << " with nonce " <<  nonceHex.str() << endl;
}


// Will be called by clHost class for check & submit
void hdsStratum::handleSolution(const WorkDescription& wd, vector<uint32_t> &indices) {

	std::vector<uint8_t> compressed;

	if (wd.solver == HdsIII) {
		compressed = packHdsIII(indices);
	} else {
		compressed = GetMinimalFromIndices(indices,25);
	}

	if (testSolution(wd.powDiff, indices, compressed))
		std::thread (&hdsStratum::submitSolution,this,wd.workId,wd.nonce,std::move(compressed)).detach();
}


hdsStratum::hdsStratum(string hostIn, string portIn, string apiKeyIn, bool debugIn, solverType forcedIn) : res(io_service), context(boost::asio::ssl::context::tlsv12)  {

	context.set_options(	  boost::asio::ssl::context::default_workarounds
				| boost::asio::ssl::context::no_sslv2
                		| boost::asio::ssl::context::no_sslv3
				| boost::asio::ssl::context::no_tlsv1
				| boost::asio::ssl::context::single_dh_use);

	host = hostIn;
	port = portIn;
	apiKey = apiKeyIn;
	debug = debugIn;

	forcedSolver = forcedIn;

	// Assign the work field and nonce
	serverWork.assign(32,(uint8_t) 0);

	random_device rd;
	default_random_engine generator(rd());
	uniform_int_distribution<uint64_t> distribution(0,0xFFFFFFFFFFFFFFFF);

	// We pick a random start nonce
	nonce = distribution(generator);

	// No work in the beginning
	workId = -1;
}

} // End namespace hdsMiner

