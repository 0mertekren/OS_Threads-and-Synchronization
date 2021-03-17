#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <pthread.h>
#include <semaphore.h>
#include <unordered_map>
#include <string>
#include <sstream>



using namespace std;
long int num_of_clients;
unsigned char max_capacity;
ofstream out_file;
ifstream file;
unsigned long int hall_idx;
unsigned long int num_of_served_clients=0;


unordered_map<string, unsigned char> hall_map = // @suppress("Invalid arguments") -- eclipse warning
{
		{ "OdaTiyatrosu", 60 },
		{ "UskudarStudyoSahne", 80 },
		{ "KucukSahne", 200 }
};

const char* teller_names[] =
{
		"Teller A",
		"Teller B",
		"Teller C"
};

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t status = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t served_clients = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t i_o = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_read = PTHREAD_MUTEX_INITIALIZER;
sem_t empty;
sem_t full;
sem_t door;
bool *seats;

void input_parse();
void *a_client(void *param);
void *a_teller(void *param);
void init_tellers();
bool reserve_this_ticket(struct client &a_client, unsigned char teller_id);
void print_reservation(struct client a_client, unsigned char teller_id);

struct client{
	string name;
	unsigned long int arrival_time;
	unsigned long int service_time;
	unsigned long int seat_requested;
	unsigned long int seat_id_assigned;
};

struct teller{
	bool teller_is_busy;
	unsigned char teller_id;
};

enum{
	TELLER_A,
	TELLER_B,
	TELLER_C,
	NUM_OF_TELLERS
};

struct teller teller_table[NUM_OF_TELLERS];
struct client *client_info_table;
struct client next_client;

int main(int argc, char **argv)
{
	if(argc != 3)
	{
		cout << "false input count";
		return -1;
	}
	file.open(argv[1]);
	out_file.open(argv[2]);
	if(file.is_open() == false)
		return -1;
	out_file << "Welcome to the Sync-Ticket!" << endl;
	input_parse();
	init_tellers();

	sem_init(&full, 0, 0);
	sem_init(&door, 0, NUM_OF_TELLERS);
	sem_init(&empty, 0, max_capacity);

	pthread_mutex_lock(&global_read);
	pthread_create(&teller_table[TELLER_A].pth_teller, NULL, &a_teller,(void*)&teller_table[TELLER_A].teller_id);
	out_file << "Teller A has arrived." << endl;
	pthread_mutex_unlock(&global_read);
	pthread_mutex_lock(&global_read);
	pthread_create(&teller_table[TELLER_B].pth_teller, NULL, &a_teller, (void*)&teller_table[TELLER_B].teller_id);
	out_file << "Teller B has arrived." << endl;
	pthread_mutex_unlock(&global_read);
	pthread_mutex_lock(&global_read);
	pthread_create(&teller_table[TELLER_C].pth_teller, NULL, &a_teller, (void*)&teller_table[TELLER_C].teller_id);
	out_file << "Teller C has arrived." << endl;
	pthread_mutex_unlock(&global_read);

	//create client threads
	for(auto i = 0; i< num_of_clients; i++){
		pthread_create(&client_info_table[i].pthread_id, NULL, &a_client,(void*)&client_info_table[i]);
	}

	//pthread_create for teller
	//pthread_create for clients
	// client index argument olarak verilecek

	// array_of seats each index represents seats and if someone
	// is occupying the seat.
	pthread_join(teller_table[TELLER_A].pth_teller, NULL);
	pthread_join(teller_table[TELLER_B].pth_teller, NULL);
	pthread_join(teller_table[TELLER_C].pth_teller, NULL);
	for(auto i = 0; i< num_of_clients; i++){
		pthread_join(client_info_table[i].pthread_id, NULL);
	}

	return 1;
}

void input_parse()
{
	int z;
	string the_hall;
	string str;
	file >>	the_hall;
	file >> num_of_clients;
	client_info_table = new struct client[num_of_clients];
	std::getline(file, str);
	for(auto i=0; i< num_of_clients; i++)
	{
		std::getline(file, str);
		//cout << "str is " << str << endl;
		stringstream ss(str);
		z = 0;
		while (ss.good()) {

			string substr;
			getline(ss, substr, ',');
			if(z == 0)
			{
				client_info_table[i].name.assign(substr);
				//cout << "subsstr is " << substr << endl;
			}
			else if(z == 1)
			{
				client_info_table[i].arrival_time = stoi(substr);
				//cout << "arriv is " << client_info_table[i].arrival_time << endl;
			}
			else if(z == 2)
				client_info_table[i].service_time = stoi(substr);
			else if(z == 3)
				client_info_table[i].seat_requested = stoi(substr);
			z++;
		}
	}
	max_capacity = hall_map[the_hall];
	seats = new bool[max_capacity+1];
	return;
}

void init_tellers()
{
	teller_table[TELLER_A].teller_id = TELLER_A;
	teller_table[TELLER_A].teller_is_busy = false;
	teller_table[TELLER_B].teller_id = TELLER_B;
	teller_table[TELLER_B].teller_is_busy = false;
	teller_table[TELLER_C].teller_id = TELLER_C;
	teller_table[TELLER_C].teller_is_busy = false;
}

void *a_client(void *param){
	struct client a_client = *((struct client *)param);
	a_client.pthread_id = pthread_self();

	//out_file << "I am " << a_client.name << " and my arrival time is " << a_client.arrival_time << "" << endl;
	sleep(a_client.arrival_time);

	pthread_mutex_lock(&mutex);
	next_client = a_client;
	sem_post(&full);
	//out_file << "1.Here is next client name " << next_client.name << "" << endl;
	return NULL;

}
void *a_teller(void *param)
{
	unsigned char teller_id = *((unsigned char *)param);
	struct client a_client;
	unsigned char i;
	unsigned char assigned_teller = (unsigned char)-1;
	while(1)
	{
		pthread_mutex_lock(&served_clients);
		if(num_of_served_clients >= num_of_clients)
		{
			out_file << "All clients received service." << endl;
			out_file.close();
			exit(0);
		}
		pthread_mutex_unlock(&served_clients);
		sem_wait(&full);
		a_client = next_client;
		pthread_mutex_lock(&status);
		for(i=0; i< NUM_OF_TELLERS; i++)
		{
			if(teller_table[i].teller_is_busy == false)
			{
				assigned_teller = i;
				teller_table[i].teller_is_busy = true;
				if(reserve_this_ticket(a_client, assigned_teller) == false)
				{
					//TODO:
				}
				break;
			}
		}
		//out_file << "2.Here is next client name " << next_client.name << "" << endl;
		pthread_mutex_unlock(&status);

		pthread_mutex_unlock(&mutex);
		sleep(a_client.service_time);
		//serve the client
		if(a_client.seat_requested >= max_capacity)
			return NULL;
		pthread_mutex_lock(&i_o);
		print_reservation(a_client, assigned_teller);
		pthread_mutex_unlock(&i_o);

		pthread_mutex_lock(&status);
		teller_table[assigned_teller].teller_is_busy = false;
		pthread_mutex_unlock(&status);
		pthread_mutex_lock(&served_clients);
		num_of_served_clients++;
		pthread_mutex_unlock(&served_clients);
	}
}

bool reserve_this_ticket(struct client &a_client, unsigned char teller_id)
{
	if(seats[a_client.seat_requested] == false)
	{
		seats[a_client.seat_requested] = true;
		a_client.seat_id_assigned = a_client.seat_requested;
		return true;
	}
	else
	{
		//first empty id
		for(auto i=1; i<=max_capacity;i++)
		{
			if(seats[i] == false)
			{
				seats[i] = true;
				a_client.seat_id_assigned = i;
				return true;
			}
		}
		return false;
	}
}

void print_reservation(struct client a_client, unsigned char teller_id)
{

	out_file << a_client.name << " requests seat " << a_client.seat_requested <<
			", reserves seat "<< a_client.seat_id_assigned << ". Signed by " <<
			teller_names[teller_id] << "." << endl;
}
