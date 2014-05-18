#include <mpi.h>
#include <iostream>
#include <windows.h>
#include "matrix.h"

#define TAG_MATRIX_PARTITION	0x4560

// Struct of info sending data
struct SendingInfo
{
	unsigned int recepient;		// number of process we send message to
	unsigned int freq;			// delay
	unsigned int size;			// message size
	unsigned int countRun;		// run counter

	SendingInfo(int rec, int f, int s, int c_run) 
		: recepient(rec), freq(f), size(s), countRun(c_run) {}
};

struct Info_For_DoSend
{
	SendingInfo *info;
	std::vector<double> *time_spent;
	std::vector<double> *local_time;
};

// object of synchronization
CRITICAL_SECTION result_flush_lock;
CRITICAL_SECTION time_buf_lock;

// variable of control count of running child threads
int threadCount = 0;
double experiment_start = 0.0;

DWORD WINAPI Thread_DoSend(Info_For_DoSend *info);
DWORD WINAPI Thread_Sender(SendingInfo* data);


int main(int argc, char *argv[])
{
	int processor_rank  = 0;
	int processor_count = 0;
	int ifFilesOpened = 0;
	Matrix freqMatrix;
	Matrix lengthMatrix;
	Matrix countMatrix;
	int i = 0;

	ifFilesOpened |= freqMatrix.fillFromFile("frequency.txt");
	ifFilesOpened |= lengthMatrix.fillFromFile("length.txt");
	ifFilesOpened |= countMatrix.fillFromFile("count.txt");

	if ( 0 != ifFilesOpened )
	{
		std::cout << "Cannot open setup file\n";
		return 1;
	}

	// Start mpi area
	MPI_Init(&argc,&argv);
	MPI_Comm_size (MPI_COMM_WORLD, &processor_count);
	MPI_Comm_rank (MPI_COMM_WORLD, &processor_rank );
	MPI_Status   status;	

	MPI_Barrier(MPI_COMM_WORLD); /* IMPORTANT */

	experiment_start = MPI_Wtime()*1000;

	InitializeCriticalSection(&result_flush_lock);
	InitializeCriticalSection(&time_buf_lock);

	std::cout << "<results_total>\n";
	std::cout << "\t<rank>" << processor_rank << "</rank>\n";

	for(i = 0; i < freqMatrix.size(); i++)
	{
		if(i != processor_rank)
		{
			//Fill data structure for sending and run in separate thread
			SendingInfo *send_info = new SendingInfo(i, freqMatrix.get(processor_rank, i), 
													lengthMatrix.get(processor_rank, i), countMatrix.get(0,0));
			CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE)Thread_Sender,
				send_info,
				0, 0);
			threadCount++;
		}
	}

	//What for do we need this????
	for(i = 0; i < freqMatrix.size(); i++)
	{
		if(i != processor_rank)
		{
			MPI_Status recv_status;
			int countRecvByte = 0;
			//MPI_Probe will return control after it recieve data into system buffer
			MPI_Probe(i, TAG_MATRIX_PARTITION, MPI_COMM_WORLD, &recv_status);
			//From recv_status mentioned above we get number of bytes that is required for recieve
			MPI_Get_count(&recv_status, MPI_BYTE, &countRecvByte);
			//Allocate buffer for recieve, get data and free memory
			double *recvData = new double[countRecvByte];
			MPI_Recv(recvData, countRecvByte, MPI_BYTE, i, TAG_MATRIX_PARTITION, MPI_COMM_WORLD, &status);
			delete[] recvData;
		}
	}
	
	//Wait all senders threads to exit
	while(true)
	{
		if(threadCount != 0)
			Sleep(1000);
		else 
			break;
	}
	
	//Wait all processes to exit
    MPI_Barrier(MPI_COMM_WORLD);

	//Free resources
	MPI_Finalize();
	DeleteCriticalSection(&result_flush_lock);
	DeleteCriticalSection(&time_buf_lock);

	std::cout << "</results_total>\n";

	return 0;
}

DWORD WINAPI Thread_Sender(SendingInfo* data)
{
	if ( NULL == data ) 
		return 1;

	double t_start = MPI_Wtime();

	//Maybe it was magic workaround, leave it here for now
	//SendingInfo *info = (SendingInfo*)data;
	
	double total_send_time = 0.0;
	std::vector<double> time_mas;
	std::vector<double> local_time;

	Info_For_DoSend sendInfo = { data, &time_mas, &local_time};

	//cout << "Start sending, will do " << info->countRun << "sends\n"; 
	for(int i = 0; i < data->countRun; i++)
	{
		Sleep(data->freq);
		CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)Thread_DoSend,
			&sendInfo,
			0, 0);
	}

	EnterCriticalSection(&result_flush_lock);

	std::cout << "\t<result>\n";
	std::cout << "\t\t<to>" << data->recepient << "</to>\n";
	std::cout << "\t\t<send_results>\n";
	for(int j = 0; j < data->countRun; j++)
	{
		std::cout << "\t\t\t<send_result>\n";
		if ( j < time_mas.size() )
			std::cout << "\t\t\t\t<time_send>" << time_mas[j] << "</time_send>\n"; 
		std::cout << "\t\t\t\t<bytes>" << data->size * sizeof(double) << "</bytes>\n";
		std::cout << "\t\t\t\t<frequency>" << data->freq * sizeof(int) << "</frequency>\n";
		std::cout << "\t\t\t\t<local_counter>" << j << "</local_counter>\n";
		if ( j < local_time.size() )
			std::cout << "\t\t\t\t<local_time>" << local_time[j] << "</local_time>\n";
		std::cout << "\t\t\t</send_result>\n";
		if ( j < time_mas.size() )
			total_send_time += time_mas[j];
	}
	std::cout << "\t\t</send_results>\n";
	std::cout << "\t\t<time_total>" << MPI_Wtime() - t_start + total_send_time << "</time_total>\n";
	std::cout << "\t</result>\n";
	
	threadCount--;
	LeaveCriticalSection(&result_flush_lock);

	if ( NULL != data )
		delete data;

	return 0;
}

DWORD WINAPI Thread_DoSend(Info_For_DoSend *info)
{
	if ( NULL == info )
		return 1;

	double tsend_start = MPI_Wtime() * 1000;
	double *message = new double[info->info->size];
	
	MPI_Send(message, info->info->size * sizeof(double), MPI_BYTE, info->info->recepient, TAG_MATRIX_PARTITION, MPI_COMM_WORLD);
	delete [] message;

	//1000 should be replaced with macros
	double tsend_end = MPI_Wtime() * 1000;

	EnterCriticalSection(&time_buf_lock);

	info->local_time->push_back(experiment_start - tsend_start);
	info->time_spent->push_back(tsend_end - tsend_start);

	LeaveCriticalSection(&time_buf_lock);

	return 0;
}