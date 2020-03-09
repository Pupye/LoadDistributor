#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define BUSY 7
#define EMPTY 5
#define FILLED 3

struct Frames
{
    int frameId;
    int frameData[10]; // in the end it should be 1200*800 one dimensional array
    //also need checksum
    //isFinished? Do we need such variable??
};

struct CamData
{
    int camID;
    struct Frames frames[10];
};

//think about round robin when it comes into data
struct SharedInts
{
    //two semaphores we need
    sem_t produced; // 0-10
    sem_t consumed; // 0-10

    sem_t cIndexLock;           // 0-1
    int consumedUpTo;           // index of consumed up to (cumulative)
    sem_t pIndexLock;           // 0-1 to access producedUpTo
    int producedUpTo;           // index of produced up to (cumulative)
    struct CamData camdata[10]; //should be 60 frames of particular camera
};

int getEmptyIndex(int *index, int length)
{
    for (size_t i = 0; i < length; i++)
    {
        if (index[i] == EMPTY)
        {
            return i;
        }
    }
    return -1;
}

void produceCamData(struct CamData *camdata)
{
    for (size_t i = 0; i < 10; i++)
    {
        for (size_t j = 0; j < 10; j++)
        {
            camdata->frames[i].frameData[j] = rand(); //here we should read from random
        }
    }
}

int main(int argc, char *argv[])
{
    key_t ShmKEY;
    int ShmID;
    struct SharedInts *ShmPTR;
    pid_t pid;
    /*to get unigue identifier*/
    ShmKEY = ftok("..", 'x');
    //allocating shared block
    ShmID = shmget(ShmKEY, sizeof(struct SharedInts), IPC_CREAT | 0666);
    if (ShmID < 0)
    {
        printf("*** shmget error ***\n");
        exit(1);
    }
    /*attaching to the memory with pointer*/
    ShmPTR = (struct SharedInts *)shmat(ShmID, NULL, 0);
    if ((int)ShmPTR == -1)
    {
        printf("*** shmat error ***\n");
        exit(1);
    }

    //**initialization of semaphores
    sem_init(&(ShmPTR->consumed), 2, 10);
    sem_init(&(ShmPTR->produced), 2, 0);
    sem_init(&(ShmPTR->pIndexLock), 2, 1);
    sem_init(&(ShmPTR->cIndexLock), 2, 1);
    //** dealing with indexes
    ShmPTR->producedUpTo = 0;
    ShmPTR->consumedUpTo = 0;
    /*******/
    for (size_t j = 0; j < 5; j++)
    {
        pid = fork();
        if (pid == 0)
        {
            break;
        }
    }

    if (pid == 0)
    {
        int dataIndex = 0;
        int producedIndex = 0;
        for (;;)
        {
            sem_wait(&ShmPTR->consumed);
            sem_wait(&ShmPTR->pIndexLock);
            //get produced index and mode it with 10
            producedIndex = ShmPTR->producedUpTo;
            ShmPTR->producedUpTo = (producedIndex + 1) % 10;
            sem_post(&ShmPTR->pIndexLock);
            produceCamData(&(ShmPTR->camdata[producedIndex]));
            sleep(5);
            printf("produced data with index %i\n", dataIndex);
            fflush(stdout);

            sem_post(&ShmPTR->produced);
            dataIndex++;
        }
    }
    else
    {
        while (pid = waitpid(-1, NULL, 0))
        {
            if (errno == ECHILD)
            {
                break;
            }
        }
    }
}
