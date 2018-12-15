#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <condition_variable>
#include <unistd.h>

using namespace std;

string FILENAME = "entrada.txt";
string DEFAULT_OUTPUT_NAME = "salida.txt";
mutex writerMutex;
mutex priorityMutex;
mutex fileMutex;
mutex counterMutex;
mutex firstReaderMutex;
condition_variable writersVariable;
condition_variable counterVariable;
condition_variable firstReaderVariable;
thread* threads;
int size;
int* priority;
int readerCounter = 0;
bool noReaders = false;
ofstream oFile;//archivo de salida
string oFileName;//nombre definido por usuario

void useSharedResource(char type, int id, int time){
	this_thread::sleep_for(chrono::milliseconds(time));
	fileMutex.lock();
	oFile << string(1,type) + " " + to_string(id) + '\n';
	fileMutex.unlock();
}

int getMax(){
	int max = -2;
	for(int i = 0; i < size; ++i) if(max < priority[i]) max = priority[i];
	return max;
}

bool waitForTurn(int id){
	priorityMutex.lock();
	int max = getMax();
	if(max >= 3 && max != priority[id-1]){
		priorityMutex.unlock();
		return true;
	}
	priorityMutex.unlock();
	return false;
}

void addOneActive(){
	for(int i = 0; i < size; ++i) if(priority[i] != -1) ++priority[i];
}

void writer(int id, int time){
	priorityMutex.lock();
	priority[id-1] = 0;
	priorityMutex.unlock();

	unique_lock<mutex> writerLock(writerMutex);
	bool firstTime = true;
	while(waitForTurn(id)){
		if(!firstTime) writersVariable.notify_one();
		writersVariable.wait(writerLock);
		firstTime = false;
	}
	priorityMutex.lock();
	priority[id-1] = -1;
	addOneActive();
	priorityMutex.unlock();

	useSharedResource('E',id,time);

	counterVariable.notify_one();
	writersVariable.notify_one();
	writerLock.unlock();
}


void reader(int id, int time){

	priorityMutex.lock();
	priority[id-1] = 0;
	priorityMutex.unlock();


	unique_lock<mutex> counterLock(counterMutex);
	bool firstTime = true;
	while(waitForTurn(id)){
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
		while(waitForTurn(id)){
			if(!firstTime) writersVariable.notify_one();
			writersVariable.wait(writerLock);
			firstTime = false;
		}
	}

	priorityMutex.lock();
	priority[id-1] = -1;
	addOneActive();
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
			noReaders = true;
			firstReaderVariable.notify_one();
			firstReaderMutex.unlock();
			counterVariable.notify_one();
			counterLock.unlock();
		}
	}
	else {
		if(firstReader) {
			unique_lock<mutex> firstReaderLock(firstReaderMutex);
			noReaders = false;
			counterVariable.notify_one();
			counterLock.unlock();
			while(!noReaders) {
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

void createOutput(){
    ofstream ofile;
    ofile.open(DEFAULT_OUTPUT_NAME, ios::out);
    char * buffer = new char[output.size()];
    strcpy(buffer,output.c_str());
    ofile.write(buffer, buffer.size()/5);
    ofile.close();
    delete buffer;
}

void readFile() {
	ifstream ifile;
	string line;
	int numberOfLines;
	string typePerson;
	int identifier;
	int miliseconds;
	ifile.open(FILENAME,ios::in);

	if (ifile.fail()){
		cout << "No se pudo abrir el archivo!";
		exit(1);
	}

    while(ifile >> numberOfLines){
        threads = new thread[numberOfLines];
        //cout << numberOfLines << endl;
        getline(ifile, line);
        for(int i = 0; i < numberOfLines; ++i){
            typePerson = line.substr(0);
            identifier = stoi(line.substr(2));
            miliseconds = stoi(line.substr(4,5));
            for (int j = 0; j < line.length(); ++j) {
                if(typePerson == "L"){
                    threads[i] = threads(reader, identifier, miliseconds);
                } else if(typePerson == "E"){
                    threads[i] = threads(writer, identifier, miliseconds);
                }
            }
        }
        //cout << threads << endl;
    }
}

int main() {
	readFile();

	for(int i = 0; i < size; ++i){
		threads[i].join();
	}
	oFile.close();
	delete[] priority;
	delete[] threads;
	return 0;
}
