#ifndef SOCKST_H
#define SOCKST_H
#include "qtts.h"
//#include "msp_cmn.h"
#include "msp_errors.h"
#include <assert.h>
#include<pthread.h>
/*
 * *线程池里所有运行和等待的任务都是一个CThread_worker
 * *由于所有任务都在链表里，所以是一个链表结构
 * */
typedef struct worker
{
    /*回调函数，任务运行时会调用此函数，注意也可声明成其它形式*/
    void *(*process) (void *arg);
    void *arg;/*回调函数的参数*/
    struct worker *next;
 
} CThread_worker;
 
 
 
/*线程池结构*/
typedef struct
{
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_ready;
 
    /*链表结构，线程池中所有等待任务*/
    CThread_worker *queue_head;
 
    /*是否销毁线程池*/
    int shutdown;
    pthread_t *threadid;
    /*线程池中允许的活动线程数目*/
    int max_thread_num;
    /*当前等待队列的任务数目*/
    int cur_queue_size;
 
} CThread_pool;


int pool_add_worker (void *(*process) (void *arg), void *arg);
void *thread_routine (void *arg);

/* wav音频头部格式 */
typedef struct _wave_pcm_hdr
{
        char            riff[4];                // = "RIFF"
        int             size_8;                 // = FileSize - 8
        char            wave[4];                // = "WAVE"
        char            fmt[4];                 // = "fmt "
        int             fmt_size;               // = 下一个结构体的大小 : 16

        short int       format_tag;             // = PCM : 1
        short int       channels;               // = 通道数 : 1
        int             samples_per_sec;        // = 采样率 : 8000 | 6000 | 11025 | 16000
        int             avg_bytes_per_sec;      // = 每秒字节数 : samples_per_sec * bits_per_sample / 8
        short int       block_align;            // = 每采样点字节数 : wBitsPerSample / 8
        short int       bits_per_sample;        // = 量化比特数: 8 | 16

        char            data[4];                // = "data";
        int             data_size;              // = 纯数据长度 : FileSize - 44
} wave_pcm_hdr;

/* 默认wav音频头部数据 */
wave_pcm_hdr default_wav_hdr =
{
        { 'R', 'I', 'F', 'F' },
        0,
        {'W', 'A', 'V', 'E'},
        {'f', 'm', 't', ' '},
        16,
        1,
        1,
        16000,
        32000,
        2,
        16,
        {'d', 'a', 't', 'a'},
        0
};
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
typedef struct car_mes
{
	char car_pic_route[128];
	char car_name[128];
	char car_time[128];

}car_mes;

#endif
