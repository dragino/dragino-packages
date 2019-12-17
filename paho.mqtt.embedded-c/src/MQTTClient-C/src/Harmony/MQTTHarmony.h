/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp.
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
 *    Johan Stokking - convert to Microchip Harmony
 *******************************************************************************/

#if !defined(MQTTHarmony_H)
#define MQTTHarmony_H

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"
#include "system_definitions.h"

#include "../MQTTErrors.h"

typedef struct Timer {
    TickType_t xTicksToWait;
    TimeOut_t xTimeOut;
} Timer;

void TimerInit(Timer *);
char TimerIsExpired(Timer *);
void TimerCountdownMS(Timer *, unsigned int);
void TimerCountdown(Timer *, unsigned int);
int TimerLeftMS(Timer *);

typedef struct Mutex {
    SemaphoreHandle_t sem;
} Mutex;

void MutexInit(Mutex *);
int MutexLock(Mutex *);
int MutexUnlock(Mutex *);
int MutexDestroy(Mutex *);

typedef struct Network {
    TCP_SOCKET my_socket;
    int (*mqttread)(struct Network *, unsigned char *, int, int);
    int (*mqttwrite)(struct Network *, unsigned char *, int, int);
} Network;

void NetworkInit(Network *);
int NetworkConnect(Network *, char *, int);
void NetworkDisconnect(Network *);

typedef struct Queue {
    QueueHandle_t queue;
} Queue;

void QueueInit(Queue *);
int Enqueue(Queue *, unsigned short);
int Dequeue(Queue *, unsigned short *, Timer *);
int QueueDestroy(Queue *);

typedef struct Thread {
    TaskHandle_t task;
} Thread;

int ThreadStart(Thread *, void (*fn)(void *), void *arg);
int ThreadJoin(Thread *);
void ThreadExit();

#endif
