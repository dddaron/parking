#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "socket.h"
#include <pthread.h>
#include <signal.h>
#include<mysql/mysql.h>
#include<sys/ioctl.h>

#include <errno.h>

#include <jerror.h>
#include<sys/select.h>
#include<sys/time.h>

int image_id = 1;
int mutex_id = 0;
//pthread_mutex_t mutex;
pthread_mutex_t mutex1;
pthread_mutex_t mutex2;

int client_id[4];

/***************************************************
	int *workingnum = (int *) malloc (sizeof (int) * 10);
	workingnum[i] = i;
    pool_add_worker (myprocess, &workingnum[i]);


****************************************************/


//share resource
static CThread_pool *pool = NULL;
void
pool_init (int max_thread_num)
{
    pool = (CThread_pool *) malloc (sizeof (CThread_pool));
 
    pthread_mutex_init (&(pool->queue_lock), NULL);
    pthread_cond_init (&(pool->queue_ready), NULL);
 
    pool->queue_head = NULL;
 
    pool->max_thread_num = max_thread_num;
    pool->cur_queue_size = 0;
 
    pool->shutdown = 0;
 
    pool->threadid = (pthread_t *) malloc (max_thread_num * sizeof (pthread_t));
    int i = 0;
    for (i = 0; i < max_thread_num; i++)
    { 
        pthread_create (&(pool->threadid[i]), NULL, thread_routine,NULL);
    }
}
 
 
 
/*向线程池中加入任务*/
int
pool_add_worker (void *(*process) (void *arg), void *arg)
{
    /*构造一个新任务*/
    CThread_worker *newworker = (CThread_worker *) malloc (sizeof (CThread_worker));
    newworker->process = process;
    newworker->arg = arg;
    newworker->next = NULL;/*别忘置空*/
 
    pthread_mutex_lock (&(pool->queue_lock));
    /*将任务加入到等待队列中*/
    CThread_worker *member = pool->queue_head;
    if (member != NULL)
    {
        while (member->next != NULL)
            member = member->next;
        member->next = newworker;
    }
    else
    {
        pool->queue_head = newworker;
    }
 
    assert (pool->queue_head != NULL);
 
    pool->cur_queue_size++;
    pthread_mutex_unlock (&(pool->queue_lock));
    /*好了，等待队列中有任务了，唤醒一个等待线程；
    注意如果所有线程都在忙碌，这句没有任何作用*/
    pthread_cond_signal (&(pool->queue_ready));
    return 0;
}
 
 
 
/*销毁线程池，等待队列中的任务不会再被执行，但是正在运行的线程会一直
把任务运行完后再退出*/
int
pool_destroy ()
{
    if (pool->shutdown)
        return -1;/*防止两次调用*/
    pool->shutdown = 1;
 
    /*唤醒所有等待线程，线程池要销毁了*/
    pthread_cond_broadcast (&(pool->queue_ready));
 
    /*阻塞等待线程退出，否则就成僵尸了*/
    int i;
    for (i = 0; i < pool->max_thread_num; i++)
        pthread_join (pool->threadid[i], NULL);
    free (pool->threadid);
 
    /*销毁等待队列*/
    CThread_worker *head = NULL;
    while (pool->queue_head != NULL)
    {
        head = pool->queue_head;
        pool->queue_head = pool->queue_head->next;
        free (head);
    }
    /*条件变量和互斥量也别忘了销毁*/
    pthread_mutex_destroy(&(pool->queue_lock));
    pthread_cond_destroy(&(pool->queue_ready));
    
    free (pool);
    /*销毁后指针置空是个好习惯*/
    pool=NULL;
    return 0;
}
 
 
 
void *
thread_routine (void *arg)
{
   // printf ("starting thread 0x%x\n", pthread_self ());
    while (1)
    {
        pthread_mutex_lock (&(pool->queue_lock));
        /*如果等待队列为0并且不销毁线程池，则处于阻塞状态; 注意
        pthread_cond_wait是一个原子操作，等待前会解锁，唤醒后会加锁*/
        while (pool->cur_queue_size == 0 && !pool->shutdown)
        {
         //   printf ("thread 0x%x is waiting\n", pthread_self ());
            pthread_cond_wait (&(pool->queue_ready), &(pool->queue_lock));
        }
 
        /*线程池要销毁了*/
        if (pool->shutdown)
        {
            /*遇到break,continue,return等跳转语句，千万不要忘记先解锁*/
            pthread_mutex_unlock (&(pool->queue_lock));
           // printf ("thread 0x%x will exit\n", pthread_self ());
            pthread_exit (NULL);
        }
 
       // printf ("thread 0x%x is starting to work\n", pthread_self ());
 
        /*assert是调试的好帮手*/
        assert (pool->cur_queue_size != 0);
        assert (pool->queue_head != NULL);
        
        /*等待队列长度减去1，并取出链表中的头元素*/
        pool->cur_queue_size--;
        CThread_worker *worker = pool->queue_head;
        pool->queue_head = worker->next;
        pthread_mutex_unlock (&(pool->queue_lock));
 
        /*调用回调函数，执行任务*/
        (*(worker->process)) (worker->arg);
        free (worker);
        worker = NULL;
    }
    /*这一句应该是不可达的*/
    pthread_exit (NULL);
}


//获得系统时间
void time_get(char* car_time)
{
	time_t t;		//将t声明为时间变量
    struct tm *p;	//struct tm是一个结构体，声明一个结构体指针
    time(&t);
    p=localtime(&t);//获得当地的时间
   // printf("%d-%d-%d %d:%d:%d",1900+p->tm_year,1+p->tm_mon,p->tm_mday,p->tm_hour,p->tm_min,p->tm_sec);
	sprintf(car_time,"%d-%d-%d %d:%d",1900+p->tm_year,1+p->tm_mon,p->tm_mday,p->tm_hour,p->tm_min);
}

/* 入口插入数据库*/
void mysql_in_cmd(char* car_pic_route,char* car_name,char* car_time)
{
	MYSQL mysql;
	mysql_init(&mysql);

	if(!mysql_real_connect(&mysql,"127.0.0.1","root","nUjVhADy","park_project",0,NULL,0))
	{
		printf("mysql_connect fail!\n");
		return;
	}else
	{
		printf("connectd MYSQL successs!\n");
	}
	char sql_cmd[1024];
	//sql_cmd = "insert into car() value()";
	//sprintf(sql_cmd,"%s%s%s%s%s%s%s%s%s","insert into car (car_pic,car_id,entry_time,time) value (",car_pic_route,",",car_name,",",car_time,",",car_time,");");
	sprintf(sql_cmd,"%s%c%s%c%s%c%s%c%s%c%s%c%s","insert into car (car_pic,car_id,entry_time) value (",'"',car_pic_route,'"',",",'"',car_name,'"',",",'"',car_time,'"',");");
	printf("sql_cmd:%s\n",sql_cmd);
	mysql_query(&mysql, "set names 'utf8'");
	int ret = mysql_query(&mysql,sql_cmd); //执行sql语句
	if(ret != 0)
	{
		printf("mysql_query():%s\n",mysql_error(&mysql));
		return;
	}
	mysql_close(&mysql);
	
}
void mysql_out_cmd(char *car_id,char *car_time)
{

	MYSQL mysql;
	mysql_init(&mysql);
	
	if(!mysql_real_connect(&mysql,"127.0.0.1","root","nUjVhADy","park_project",0,NULL,0))
	{
		printf("mysql_connect fail!\n");
		return;
	}else
	{
		printf("connectd MYSQL successs!\n");
	}
	char sql_cmd[512];
	char update[512];
	char update_fee[512];
	//time_get(car_time);
	
	sprintf(sql_cmd,"%s%c%s%c%s%c%s%c%c"," update car set exit_time = ",'"',car_time,'"'," where car_id = ",'"',car_id,'"',';');
	printf("sql_cmd:%s\n",sql_cmd);
	sprintf(update,"%s%c%s%c%c","update car set time = (UNIX_TIMESTAMP(exit_time) - UNIX_TIMESTAMP(entry_time))/60 where car_id = ",'"',car_id,'"',';');
	sprintf(update_fee,"%s%c%s%c%c","update car set fee = (UNIX_TIMESTAMP(exit_time) - UNIX_TIMESTAMP(entry_time))/360 where car_id = ",'"',car_id,'"',';');
	printf("%s\n",update_fee);
	mysql_query(&mysql, "set names 'utf8'");
	int ret = mysql_query(&mysql,sql_cmd); //执行sql语句
	if(ret != 0)
	{
		printf("mysql_query():%s\n",mysql_error(&mysql));
		return;
	}

	mysql_query(&mysql,update);
	mysql_query(&mysql,update_fee);
	mysql_close(&mysql);

}

/* 文本合成 */
int text_to_speech(const char* src_text, const char* des_path, const char* params)
{
        int          ret          = -1;
        FILE*        fp           = NULL;
        const char*  sessionID    = NULL;
        unsigned int audio_len    = 0;
        wave_pcm_hdr wav_hdr      = default_wav_hdr;
        int          synth_status = MSP_TTS_FLAG_STILL_HAVE_DATA;

        if (NULL == src_text || NULL == des_path)
        {
                printf("params is error!\n");
                return ret;
        }
        fp = fopen(des_path, "wb");
        if (NULL == fp)
        {
                printf("open %s error.\n", des_path);
                return ret;
        }
        /* 开始合成 */
        sessionID = QTTSSessionBegin(params, &ret);
        if (MSP_SUCCESS != ret)
        {
                printf("QTTSSessionBegin failed, error code: %d.\n", ret);
                fclose(fp);
                return ret;
        }
        ret = QTTSTextPut(sessionID, src_text, (unsigned int)strlen(src_text), NULL);
        if (MSP_SUCCESS != ret)
        {
                printf("QTTSTextPut failed, error code: %d.\n",ret);
                QTTSSessionEnd(sessionID, "TextPutError");
                fclose(fp);
                return ret;
        }
       
        fwrite(&wav_hdr, sizeof(wav_hdr) ,1, fp); //添加wav音频头，使用采样率为16000
        while (1)
        {
                 /* 获取合成音频 */
                const void* data = QTTSAudioGet(sessionID, &audio_len, &synth_status, &ret);
                if (MSP_SUCCESS != ret)
                        break;
                if (NULL != data)
                {
                        fwrite(data, audio_len, 1, fp);
                    wav_hdr.data_size += audio_len; //计算data_size大小
                }
                if (MSP_TTS_FLAG_DATA_END == synth_status)
                        break;
                printf(">");
            usleep(150*1000); //防止频繁占用CPU
        }
        printf("\n");
        if (MSP_SUCCESS != ret)
        {
                printf("QTTSAudioGet failed, error code: %d.\n",ret);
                QTTSSessionEnd(sessionID, "AudioGetError");
                fclose(fp);
                return ret;
        }
        /* 修正wav文件头数据的大小 */
        wav_hdr.size_8 += wav_hdr.data_size + (sizeof(wav_hdr) - 8);

         /* 将修正过的数据写回文件头部,音频文件为wav格式 */
        fseek(fp, 4, 0);
        fwrite(&wav_hdr.size_8,sizeof(wav_hdr.size_8), 1, fp);  //写入size_8的值
        fseek(fp, 40, 0); //将文件指针偏移到存储data_size值的位置
        fwrite(&wav_hdr.data_size,sizeof(wav_hdr.data_size), 1, fp); //写入data_size的值
        fclose(fp);
        fp = NULL;
         /* 合成完毕 */
        ret = QTTSSessionEnd(sessionID, "Normal");
        if (MSP_SUCCESS != ret)
        {
                printf("QTTSSessionEnd failed, error code: %d.\n",ret);
        }

        return ret;
}


//接收照片
void rdphoto(int client_sd)
{
		
		int fd;
		client_packet head;
		recv(client_sd,&head,sizeof(client_packet),0);
		
		pthread_mutex_unlock(&mutex1);
		
		char image_name[128]={0};

		sprintf(image_name,"%s%d%s","./image/",image_id,".jpg");
		printf("image_name:%s\n",image_name);
		image_id++;
		fd = open(image_name,O_RDWR|O_CREAT|O_TRUNC,0666);	
		if(fd < 0)
		{
			perror("open file fail\n");
			exit(1);
		}
		int cnt,writenum = 0;
		char r_buf[1024];
		writenum = head.pic_size;
		printf("writenum is:%d\n",writenum);
		while(writenum > 0)
		{
			memset(r_buf,0,sizeof(r_buf));
			if(writenum >= 1024)
				cnt = recv(client_sd,r_buf,sizeof(r_buf),0);
			else
				cnt = recv(client_sd,r_buf,writenum,0);
			if(cnt == 0)
			{
				printf("data is complete\n");
				close(client_sd);
				break;
			}
			write(fd,r_buf,cnt);
			writenum -= cnt;
		}	
		
		close(fd);
		memset(&head,0,sizeof(client_packet));
		memset(r_buf,0,sizeof(r_buf));
		strcpy(r_buf,"rdphotoed");
		send(client_sd,r_buf,strlen(r_buf),0);
		pthread_mutex_unlock(&mutex1);
}



//发送语音
void sdvoice(int client_sd)
{
	printf("sending voice\n");
	int fd,cnt;
	server_packet head;
	struct stat buf;
	char tmpfile[128] = {0},r_buf[1024]={0};

	sprintf(tmpfile,"%s%s","car",".wav");
	fd = open(tmpfile,O_RDWR);	
	if(fd < 0)
	{
		perror("open wav fail\n");
		exit(2);
	}
	stat(tmpfile,&buf);
	head.voice_size = buf.st_size;
	head.command = 1;
	
	send(client_sd,&head,sizeof(server_packet),0);	
	
	memset(r_buf,0,sizeof(r_buf));
	while((cnt = read(fd,r_buf,1024))!=0)
	{
		send(client_sd,r_buf,cnt,0);
		memset(r_buf,0,sizeof(r_buf));
	}
	memset(&head,0,sizeof(client_packet));
				

	printf("sdvoice is over\n");
	memset(r_buf,0,sizeof(r_buf));
	

	//recv(client_sd,r_buf,7,0);

	//printf("video_r_buf:%s\n",r_buf);
}

//发送文本
void sdtxt(int client_sd)
{
	
	int fd,cnt;
	server_packet head;
	struct stat buf;
	char tmpfile[128] = {0},r_buf[1024]={0};

	sprintf(tmpfile,"%s%s","car",".txt");
	fd = open(tmpfile,O_RDWR);	
	if(fd < 0)
	{
		perror("open wav fail\n");
		exit(2);
	}
	stat(tmpfile,&buf);
	head.voice_size = buf.st_size;
	head.command = 1;
	
	send(client_sd,&head,sizeof(server_packet),0);	
	
	memset(r_buf,0,sizeof(r_buf));
	while((cnt = read(fd,r_buf,1024))!=0)
	{
		send(client_sd,r_buf,cnt,0);
		memset(r_buf,0,sizeof(r_buf));
	}
	memset(&head,0,sizeof(client_packet));
				


	printf("sdtxt is over\n");
	memset(r_buf,0,sizeof(r_buf));
	

//	recv(client_sd,r_buf,5,0);
	
	
		
//	printf("txt_r_buf:%s\n",r_buf);
}

void *TCP_Analyzer(void *pPara)
{

	int *a;car_mes car;

	a = (int *)pPara;

//回应客户端收到数据包
	
	char r_buf[128] = {0};
	sleep(1);					
	
	
				
	rdphoto(*a);

	pthread_mutex_lock(&mutex2);

	
	char image_jpg[512]={0};
	sprintf(image_jpg,"%s%s%d%s","./demo ","./image/",image_id-1,".jpg");
	sprintf(car.car_pic_route,"%s%d%s","/home/project/EasyPR/image/",image_id-1,".jpg");
	

	system(image_jpg);
	int fd;
	fd = open("car.txt",O_RDWR);
	read(fd,r_buf,128);
	char *rr_buf = r_buf;	
	
	close(fd);

	
	strcpy(car.car_name,rr_buf+7);
	time_get(car.car_time); //获得时间
	
	if(client_id[0] == 1|| client_id[1] ==1){
		mysql_in_cmd(car.car_pic_route,rr_buf+7,car.car_time);	//入口插入数据库	
		client_id[0] = 0;
		client_id[1] = 0;
		}
	else if(client_id[2] == 1|| client_id[3] ==1)
	{
		mysql_out_cmd(rr_buf+7,car.car_time);
		client_id[2] = 0;
		client_id[3] = 0;
	}
	//time_get(car.car_time); //获得时间
//	mysql_out_cmd(rr_buf+7,"100",car.car_time);
	char car_wav_name[128];
	sprintf(car_wav_name,"%s%s%s","车牌:",rr_buf+7,",欢迎光临");
	
    const char* session_begin_params = "voice_name = xiaoyan, text_encoding = utf8, sample_rate = 16000, speed = 50, volume = 50, pitch = 50, rdn = 2";
	static const char* filename             = "/home/project/EasyPR/car.wav"; //合成的语音文件名称
    const char* text                 = car_wav_name;
	

 	sleep(mutex_id);
	mutex_id%=2;  //让线程先后进行语音合成
	mutex_id++;
	 /* 文本合成 */
	text_to_speech(text, filename, session_begin_params);
     /*   if (MSP_SUCCESS != rets)
        {
                printf("text_to_speech failed, error code: %d.\n", rets);
        }*/
	
	pthread_mutex_unlock(&mutex2);

//exit:
   	//MSPLogout(); //退出登录	
	sdvoice(*a);
	sdtxt(*a);
	
	
	printf("write is over\n");
	close(*a);
	//pthread_exit(NULL);
}



int main()
{
	
 	int *pConnectfd = NULL;
	int listen_sd;
	int client_fd;
	pool_init (3);/*线程池中最多三个活动线程*/
	pthread_mutex_init(&mutex1,NULL);
	pthread_mutex_init(&mutex2,NULL);
	listen_sd  = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);

	int   ret                  = MSP_SUCCESS;
    const char* login_params         = "appid = 5d5230fd, work_dir = .";//登录参数,appid与msc库绑定,请勿随意改动
	/* 用户登录 */
	ret = MSPLogin(NULL, NULL, login_params);
	if (MSP_SUCCESS != ret)
        {
                printf("MSPLogin failed, error code: %d.\n", ret);
  //              goto exit ;
        }

	
	if(listen_sd < 0)
	{
		perror("socket");
		exit(1);
	}
	
	 int opt = 1;
    setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	printf("socket create success,server socket fd:%d\n",listen_sd);

	pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8090);//little to big
	server_addr.sin_addr.s_addr = htons(INADDR_ANY);	

	bind(listen_sd,(struct sockaddr *)&server_addr,sizeof(server_addr));
	
	listen(listen_sd,100);
	

	fd_set fds,rfds;
	FD_ZERO(&fds);
	FD_SET(listen_sd,&fds);
	
	
	while(1)
	{
		printf("listening\n");
		rfds = fds;
		
		select(listen_sd+1, &rfds, NULL, NULL, 0);
		 
		if(FD_ISSET(listen_sd,&rfds))		 /* 说明有新的客户端链接请求 */
		{
		struct sockaddr_in client_addr;
		socklen_t addrlen;
		addrlen = sizeof(client_addr);
	
		client_fd = accept(listen_sd,(struct sockaddr *)&client_addr,&addrlen); /* Accept 不会阻塞 */
            	
		
		
		FD_SET(client_fd,&fds);
		
				
				printf("client_fd=%d\n",client_fd);
				
				if(client_fd != -1)
				{
			//		sleep(2);
					client_packet client_1;
		
					recv(client_fd,&client_1,sizeof(client_packet),0);

					client_id[client_1.client_num] = 1;
					printf("client_id = %d\n",client_1.client_num);
					send(client_fd,"rec",3,0);
				 		pConnectfd = (int *)malloc(sizeof(int)); //线程并发的重点
                        if (NULL == pConnectfd)
                        {
                            printf("TCPServer: pConnectfd malloc Failed.\n");
                            break;
                        }

                        memset(pConnectfd,0,sizeof(int));
                        *pConnectfd = client_fd ;
						printf("TCPServer:the connect fd is %d\n",client_fd );
						
        				pool_add_worker(TCP_Analyzer, (void *)pConnectfd);
                       
					
                       //rets = pthread_create(&pid, &attr, TCP_Analyzer, (void *)&client_fd);
                        
				}
		}

	}
	close(listen_sd);		
	return 0;
}
