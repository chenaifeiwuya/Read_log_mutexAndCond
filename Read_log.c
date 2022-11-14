#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/fcntl.h>
#include<pthread.h>
#include<signal.h>
#include<sys/wait.h>
/*请在这里添加程序注释
该程序为互斥锁结合条件变量实例
具体题目清看：条件变量部分课程资料后留题目
*/

//本程序需要比较筛选出的字符串:E CamX  或者 E CHIUSECASE
char cmp1[7]="E CamX";
char cmp2[13]="E CHIUSECASE";



int flagsA = 1;  //为1时表示线程A还在
int flagsB = 1;
int flagsC = 1;

char source_filePath[50];
char target_filePath[50];

typedef struct{
	char Buf[1024];
	int FRONT; //FRONT为上一条语句后的'\n'后一位的下标
	int REAR;   
	int CUR;   //CUR为已占用空间
	int MAX;   
}Buffer;


pthread_mutex_t lockA;  //分别对应BufferA  B缓冲区
pthread_mutex_t lockB;  

pthread_cond_t A_Full;  //分别对应4个执行条件
pthread_cond_t A_Empty;
pthread_cond_t B_Full;
pthread_cond_t B_Empty;

void * thread_A(void * arg)   //arg传BufferA指针
{
	pthread_detach(pthread_self());
	FILE * fp;
//	int fd;  //用于测试的文件描述符
//	fd = open("2.log",O_WRONLY);
	Buffer* BufferA = (Buffer*)arg;
	if((fp = fopen(source_filePath,"r")) == NULL)
	{
		perror("open source file Failed!");
		flagsA = 0;
		pthread_exit(NULL);
	}
	char a[1024];

	while(1)
	{
		bzero(a,1024);
		if(fgets(a,1024,fp) ==NULL)//从fp中读取1行数据(换行符也会被读出)
		{
			//返回值为NULL表示在文件末尾了
			printf("Pthread_A EXIT!!...\n");
			pthread_cond_signal(&A_Empty);
			flagsA = 0;
			break;
		}
		int num=1023;
		while(a[num] == '\0')  //倒序遍历计算字符串长度(因为字符串中穿插的有空格，所以无法用库函数求长度，因为最后一行没有换行符，所以无法单独用换行符来判断长度)
		{
			num--;
			if(num < 0)
				break;
		}
	//	printf("num : %d  .........\n",num);
		if(a[num] != '\n')				//由于读取的文件最后一行数据没有换行符，为了方便后续管理，手动添加换行符
		{
			a[num+1] = '\n';
			num++;      //长度+1
		}
		num++;  //字符串长度等于数组下标+1
	//	write(fd,a,num);
	//	fseek(fp,num,SEEK_CUR);  //光标移动到下一行(不用移动)
	pthread_mutex_lock(&lockA);
	while((BufferA->MAX -  BufferA->CUR) < num  )  //如果缓冲区A中没有足够空间了
	{
		printf("pthreadA 0x%x 挂起,BufferA剩余空间为%d 需写入大小为 %d\n",(unsigned int)pthread_self(),(BufferA->MAX - BufferA->CUR),num);

		pthread_cond_wait(&A_Full,&lockA);
	}

	if((BufferA->MAX - BufferA->FRONT) >= num )  //如果不用拆分写
		{
			memcpy(&(BufferA->Buf[BufferA->FRONT]),a,num); 
		}
		else  //要拆分写
		{
			memcpy(&(BufferA->Buf[BufferA->FRONT]),a,BufferA->MAX - BufferA->FRONT);
			memcpy(&(BufferA->Buf[0]),a+(BufferA->MAX - BufferA->FRONT),num - (BufferA->MAX - BufferA->FRONT));
		}
		BufferA->FRONT = (BufferA->FRONT + num) % BufferA->MAX;
		BufferA->CUR = (BufferA->CUR) + num;
		printf("pthread_A BufferA->CUR= %d\n",BufferA->CUR);
		pthread_mutex_unlock(&lockA);
		pthread_cond_signal(&A_Empty);
		printf("pthreadA signal A_Empty\n");
	}
	flagsA = 0;
	fclose(fp);
//	close(fd);
	pthread_exit(NULL); 
}

void * thread_B(void * arg)//arg传BufferA B的数组指针
{
	pthread_detach(pthread_self());

	int isContinue = 1;   //用于判断是否继续当前循环：0表示继续， 1表示跳过本次循环
	Buffer* BufferA = (Buffer*)arg;
	Buffer* BufferB = ((Buffer*)arg) + 1;
//	int fd = open("3.c",O_WRONLY);
	char a[1024];
	while(1)
	{
	
		bzero(a,1024);  //清空
		pthread_mutex_lock(&lockA);
		while(BufferA->CUR <= 0)
		{
			if(flagsA == 0)  //线程A已退出且缓冲区没有数据说明数据读完了,退出线程
				{
					flagsB = 0;
//					close(fd);
					//退出前进行一次唤醒操作
					pthread_cond_signal(&B_Empty);
					printf("Pthread_B EXIT!!!...\n");
				pthread_exit(NULL);
				}
			printf("Pthread_B 0x%x 挂起， BufferA内数据长度为%d\n",(unsigned int)pthread_self(),BufferA->CUR);
			pthread_cond_wait(&A_Empty, &lockA);
		}
		int i = 0;
		while(BufferA->Buf[BufferA->REAR] != '\n')   //从BufferA读出数据
		{

			memcpy(a+i,&(BufferA->Buf[BufferA->REAR]),1);
			BufferA->Buf[BufferA->REAR] = '\0';
			if((++(BufferA->REAR))==BufferA->MAX)
				BufferA->REAR = 0;
			(BufferA->CUR)--;
			if(i>=1024)
				break;
			i++;
		}
		BufferA->Buf[BufferA->REAR] = '\0';
		(BufferA->CUR)--;
		if((++(BufferA->REAR))==BufferA->MAX)  //上述循环中并未算入换行符，此处+1，算上换行符
				BufferA->REAR = 0;
		
		//BufferA->Buf[BufferA->REAR] = '\0'; //REAR所在处是下一个字符串起点
		a[i] = '\n';  //添加换行符
		printf("pthreadB BufferA->CUR = %d \n",BufferA->CUR);
		//判断本次读取数据中是否包含目标字符串:E CamX  或者 E CHIUSECASE

		pthread_mutex_unlock(&lockA);
		pthread_cond_signal(&A_Full);
		i = 0;
		int cmp_num1=0;
		int cmp_num2=0;
		isContinue = 1;   //置位
		while(a[i] != '\n')
		{
			while(a[i+cmp_num1] == cmp1[cmp_num1] && cmp_num1 < 7)    
			{
					cmp_num1++;
			}
			while(a[i+cmp_num2] == cmp2[cmp_num2] && cmp_num2 <13 )
			{
				cmp_num2++;
			}

			if(cmp_num1 == 6 || cmp_num2 == 12 )//未匹配到目标字符串
			{
				isContinue = 0; //不跳过本次循环
			}

		 	 i++;	
		}
		if(isContinue == 1)
		{
			continue;
		}

		printf("pthreadB signal A_Full\n");
		pthread_mutex_lock(&lockB);
		while((BufferB->MAX - BufferB->CUR) < i)   //i是本次数据的长度
		{
			printf("Pthread_B 0x%x 挂起， BufferB剩余空间为%d\n",(unsigned int)pthread_self(),BufferB->MAX - BufferB->CUR);
			pthread_cond_wait(&B_Full,&lockB);
		}
		//开始向BufferB写入
		i=0;
		while(a[i]!='\n')
		{
			memcpy(&(BufferB->Buf[BufferB->FRONT]),a+i,1);
//			write(fd,a+i,1);
		
			if((++(BufferB->FRONT))==BufferB->MAX)
				BufferB->FRONT = 0;
			(BufferB->CUR)++;
			if(i>= 1024)
				break;
			i++;

		} 
		memcpy(&(BufferB->Buf[BufferB->FRONT]),a+i,1);   //将'\n'写入
//		write(fd,a+i,1);
		if((++(BufferB->FRONT))==BufferB->MAX)
			BufferB->FRONT = 0;
		(BufferB->CUR)++;

		printf("BufferB->CUR = %d \n",BufferB->CUR);
		pthread_mutex_unlock(&lockB);
		pthread_cond_signal(&B_Empty);
		printf("pthreadB signal B_Empty\n");
	}
		flagsB = 0;
		pthread_exit(NULL);
		
}

void * thread_C(void * arg)
{

	Buffer* BufferB = (Buffer *)arg;
	pthread_detach(pthread_self());
	int fd;
	if( (fd = open(target_filePath,O_WRONLY | O_CREAT,0664)) == -1 )
	{
		printf("open target File Failed...\n");
		flagsC = 0;
		close(fd);
		pthread_exit(NULL);
	}
	while(1)
	{
		pthread_mutex_lock(&lockB);
		while(BufferB->CUR == 0)   //之所以用while循环是因为当被唤醒之后还会进行一次判断
		{
			if(flagsB == 0)
			{
				flagsC = 0;
				printf("pthread_C EXIT!...\n");
				pthread_exit(NULL);  
			}
			printf("Pthread_C 0x%x 挂起， BufferB内数据长度间为%d\n",(unsigned int)pthread_self(),BufferB->CUR);
			pthread_cond_wait(&B_Empty,&lockB);
		}
	int i =0;
		while(BufferB->CUR != 0)  //从BufferB写入到文件中
		{
			write(fd,&(BufferB->Buf[BufferB->REAR]),1);
			BufferB->Buf[BufferB->REAR] = '\0';
			if((++(BufferB->REAR))==BufferB->MAX)
				BufferB->REAR = 0;
			(BufferB->CUR)--;
			if(i>=1024)
				break;
			i++;
		}
		pthread_mutex_unlock(&lockB);
		pthread_cond_signal(&B_Full);
		printf("pthreadC signal B_Full\n");
	}
	flagsC = 0;
	close(fd);
	pthread_exit(NULL);
}


int main(int argc, char ** argv)
{
	pthread_t tids[3];
	//初始化各种资源	
	if(argc != 3)
	{
		printf("参数错误，清重新输入:例：/app  .log   .txt\n");
		exit(0);
	}
	strcpy(source_filePath,argv[1]);
	strcpy(target_filePath,argv[2]);
//	printf("%s\n",target_filePath);
	//初始化BufferA 和BufferB：
	Buffer * BufferAB;
	if ((BufferAB = (Buffer*)malloc(2 * sizeof(Buffer))) ==NULL)  //AB[0]为BufferA ， AB[1]为BufferB
	{
		printf("内存空间申请失败！\n");
		exit(0);
	}
	bzero(BufferAB[0].Buf,1024);
	BufferAB[0].FRONT = 0;
	BufferAB[0].REAR = 0;
	BufferAB[0].CUR = 0;
	BufferAB[0].MAX = 1024;

	bzero(BufferAB[1].Buf,1024);
	BufferAB[1].FRONT = 0;
	BufferAB[1].REAR = 0;
	BufferAB[1].CUR = 0;
	BufferAB[1].MAX = 1024;
	//初始化锁和条件变量
	if(pthread_mutex_init(&lockA,NULL) != 0 || pthread_mutex_init(&lockB,NULL) != 0 || pthread_cond_init(&A_Full,NULL) != 0 || pthread_cond_init(&A_Empty,NULL) != 0 || pthread_cond_init(&B_Full,NULL) != 0 || pthread_cond_init(&B_Empty,NULL) !=0)
	{
		perror("init lock or cond Failed...\n");
		free(BufferAB);
		BufferAB = NULL;
		exit(0);
	}
	//创建线程
	int err;
	if( (err = pthread_create(&tids[0],NULL,thread_A,(void *)&BufferAB[0])) != 0)
	{
		printf("threadA create Failed: %s\n",strerror(err));
		free(BufferAB);  //malloc分配空间时会有一个额外的空间用来记录管理信息，其大小为内存管理块结构体的大小，free通过这个结构体的信息来释放空间
		BufferAB = NULL;

		pthread_mutex_destroy(&lockA);
		pthread_mutex_destroy(&lockB);
		pthread_cond_destroy(&A_Full);
		pthread_cond_destroy(&A_Empty);
		pthread_cond_destroy(&B_Full);
		pthread_cond_destroy(&B_Empty);
		exit(0);
	}

	if( (err = pthread_create(&tids[1],NULL,thread_B,(void *)BufferAB)) != 0)
	{
		printf("threadB create Failed: %s\n",strerror(err));
		free(BufferAB);  //malloc分配空间时会有一个额外的空间用来记录管理信息，其大小为内存管理块结构体的大小，free通过这个结构体的信息来释放空间
		BufferAB = NULL;
		pthread_mutex_destroy(&lockA);
		pthread_mutex_destroy(&lockB);
		pthread_cond_destroy(&A_Full);
		pthread_cond_destroy(&A_Empty);
		pthread_cond_destroy(&B_Full);
		pthread_cond_destroy(&B_Empty);
		exit(0);
	}
	if( (err = pthread_create(&tids[2],NULL,thread_C,(void *)&BufferAB[1])) != 0)
	{
		printf("threadC create Failed: %s\n",strerror(err));
		free(BufferAB);  //malloc分配空间时会有一个额外的空间用来记录管理信息，其大小为内存管理块结构体的大小，free通过这个结构体的信息来释放空间
		BufferAB = NULL;
		pthread_mutex_destroy(&lockA);
		pthread_mutex_destroy(&lockB);
		pthread_cond_destroy(&A_Full);
		pthread_cond_destroy(&A_Empty);
		pthread_cond_destroy(&B_Full);
		pthread_cond_destroy(&B_Empty);
		exit(0);
	}

	while(1)
	{
		sleep(1);
		if(flagsC == 0)
			{
				printf("flagsA = %d,  flagsB = %d, flagsC = %d\n",flagsA,flagsB,flagsC);
				break;
			}
	}
		free(BufferAB);  //malloc分配空间时会有一个额外的空间用来记录管理信息，其大小为内存管理块结构体的大小，free通过这个结构体的信息来释放空间
		BufferAB = NULL;
		pthread_mutex_destroy(&lockA);
		pthread_mutex_destroy(&lockB);
		pthread_cond_destroy(&A_Full);
		pthread_cond_destroy(&A_Empty);
		pthread_cond_destroy(&B_Full);
		pthread_cond_destroy(&B_Empty);

		printf("Job Over!...\n");
		return 0;


}
