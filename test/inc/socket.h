#ifndef SOCKST_H
#define SOCKST_H

typedef struct client_packet
{
	int pic_size;
	int client_num;
}client_packet;


typedef struct server_packet
{
	char car_id[8];
	int voice_size;
	int command;
}server_packet;

#endif

