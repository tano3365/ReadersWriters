#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <unistd.h>

using namespace std;

mutex writerMutex;
mutex priorityMutex;
mutex fileMutex;
mutex counterMutex;
mutex firstReaderMutex;
condition_variable writersVariable;
condition_variable counterVariable;
condition_variable firstReaderVariable;
int size;
int readerCounter = 0;
bool noReaders = false;
string output;

void useSharedResource(char type, int id, int time){
	this_thread::sleep_for(chrono::milliseconds(time));
	fileMutex.lock();
	output += string(1,type) + " " + to_string(id) + " " + to_string(time) + '\n';
	fileMutex.unlock();
}

int getMax(int* priority){
	int max = -1;
	for(int i = 0; i < size; ++i) if(max < priority[i]) max = priority[i];
	return max;
}

bool waitForTurn(int id, int* priority){
	int max = getMax(priority);
	if(max >= 3 && max != priority[id-1]){
		return false;
	}
	return true;
}

void addOneActive(int* priority){
	for(int i = 0; i < size; ++i) if(priority[i] != 0) ++priority[i];
}

void writer(int id, int time, int* priority){
	priorityMutex.lock();
	priority[id-1] = 1;
	priorityMutex.unlock();

	unique_lock<mutex> writerLock(writerMutex);
	bool firstTime = true;
	while(!waitForTurn(id,priority)){
		if(!firstTime) writersVariable.notify_one();
		writersVariable.wait(writerLock);
		firstTime = false;
	}
	priorityMutex.lock();
	priority[id-1] = 0;
	addOneActive(priority);
	priorityMutex.unlock();

	useSharedResource('E',id,time);

	writersVariable.notify_one();
	writerLock.unlock();
}


void reader(int id, int time, int* priority){

	priorityMutex.lock();
	priority[id-1] = 1;
	priorityMutex.unlock();


	unique_lock<mutex> counterLock(counterMutex);
	bool firstTime = true;
	while(!waitForTurn(id,priority)){
		if(!firstTime) writersVariable.notify_one();
		counterVariable.wait(counterLock);
		firstTime = false;
	}

	++readerCounter;
	unique_lock<mutex> writerLock(writerMutex, defer_lock);
	bool firstReader = false;
	if(readerCounter == 1) {
		writerLock.lock();
		bool firstTime = true;
		firstReader = true;
		while(!waitForTurn(id,priority)){
			if(!firstTime) writersVariable.notify_one();
			writersVariable.wait(writerLock);
			firstTime = false;
		}
	}

	priorityMutex.lock();
	priority[id-1] = 0;
	addOneActive(priority);
	priorityMutex.unlock();
	counterVariable.notify_one();
	counterLock.unlock();
	useSharedResource('L',id,time);
	counterLock.lock();
	--readerCounter;


	if(readerCounter == 0) {
		if(firstReader) {
			counterVariable.notify_one();
			counterLock.unlock();
			writersVariable.notify_one();
			writerLock.unlock();
		}
		else{
			firstReaderMutex.lock();
			noReaders = false;
			firstReaderVariable.notify_one();
			firstReaderMutex.unlock();
			counterVariable.notify_one();
			counterLock.unlock();
		}
	}
	else {
		if(firstReader) {
			unique_lock<mutex> firstReaderLock(firstReaderMutex);
			noReaders = true;
			counterVariable.notify_one();
			counterLock.unlock();
			while(noReaders) {
				firstReaderVariable.wait(firstReaderLock);
			}
			writersVariable.notify_one();
			writerLock.unlock();
		}
		else{
			counterVariable.notify_one();
			counterLock.unlock();
		}
	}
}

int main() {
	size = 7;
	int* priority = new int [size];
	for(int i = 0 ; i < size; ++i) priority[i] = 0;
	thread t1 (reader,1,15,priority);

	thread t2 (reader,2,10,priority);

	thread t3 (reader,3,5,priority);
	thread t4 (writer,4,5,priority);
	thread t5 (writer,5,12,priority);
	thread t6 (reader,6,10,priority);
	thread t7 (reader,7,2,priority);

	t1.join();
	t2.join();
	t3.join();
	t4.join();
	t5.join();
	t6.join();
	t7.join();
	cout << output;
	return 0;
}