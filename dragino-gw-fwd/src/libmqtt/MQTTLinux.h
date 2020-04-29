/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander - initial API and implementation and/or initial documentation
 *******************************************************************************/

#if !defined(__MQTT_LINUX_)
#define __MQTT_LINUX_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "MQTTErrors.h"

typedef struct Timer
{
   struct timeval end_time;
} Timer;

void TimerInit(Timer *);
char TimerIsExpired(Timer *);
void TimerCountdownMS(Timer *, unsigned int);
void TimerCountdown(Timer *, unsigned int);
int TimerLeftMS(Timer *);

typedef struct Network
{
   int my_socket;
   int (*mqttread)(struct Network *, unsigned char *, int, int);
   int (*mqttwrite)(struct Network *, unsigned char *, int, int);
} Network;

int linux_read(Network *, unsigned char *, int, int);
int linux_write(Network *, unsigned char *, int, int);

void NetworkInit(Network *);
int NetworkConnect(Network *, char *, int);
void NetworkDisconnect(Network *);
int NetworkCheckConnected(Network *);
int NetworkIsConnected(Network *);

typedef struct Mutex
{
   pthread_mutex_t m;
} Mutex;

void MutexInit(Mutex *);
int MutexLock(Mutex *);
int MutexUnlock(Mutex *);
int MutexDestroy(Mutex *);

typedef struct Semaphore
{
   sem_t s;
} Semaphore;

void SemaphoreInit(Semaphore *);
int SemaphoreWait(Semaphore *);
int SemaphoreTimedWait(Semaphore *, Timer *);
int SemaphoreSignal(Semaphore *);
int SemaphoreDestroy(Semaphore *);

typedef struct Queue
{
   unsigned short item;
   Semaphore s;
   Mutex m;
} Queue;

void QueueInit(Queue *);
int Enqueue(Queue *, unsigned short);
int Dequeue(Queue *, unsigned short *, Timer *);
int QueueDestroy(Queue *);

typedef struct Thread
{
   pthread_t t;
   int started;
} Thread;

int ThreadStart(Thread *, void (*fn)(void *), void *arg);
int ThreadStarted(Thread *);
int ThreadJoin(Thread *);
void ThreadExit();

#endif
