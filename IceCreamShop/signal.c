//
//  signal.c
//  IceCreamShop
//
//  Created by ronglei on 15/11/14.
//  Copyright © 2015年 ronglei. All rights reserved.
//

#include "signal.h"
#include <stdio.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void initSignal(signal_t signal, char *name, int value)
{
    assert(name != NULL);
    sem_unlink(name);
    
    if ((signal->sem = sem_open(name, O_CREAT, 0666, value)) == (void *)(void*)SEM_FAILED) {
        printf("%s signal open error\n", name);
        exit(-1);
    }
    signal->name = name;
    signal->value = value;
}

void waitSignal(signal_t signal)
{
    int result = 0;
    if ((result = sem_wait(signal->sem) != 0)) {
        printf("%s signal wait error\n", signal->name);
        exit(-1);
    }
    signal->value--;
}

void postSignal(signal_t signal)
{
    int result = 0;
    if ((result = sem_post(signal->sem) != 0)) {
        printf("%s signal post error\n", signal->name);
        exit(-1);
    }
    signal->value++;
}