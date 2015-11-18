//
//  signal.h
//  IceCreamShop
//
//  Created by ronglei on 15/11/14.
//  Copyright © 2015年 ronglei. All rights reserved.
//

#ifndef signal_h
#define signal_h

#include <semaphore.h>

typedef struct{
    sem_t *sem;     // 信号量
    char *name;     // 名称标识
    int value;      // 当前信号量的值
}signal_s, *signal_t;

void initSignal(signal_t signal, char *name, int value); // open_sem
void waitSignal(signal_t signal);   // wait_sem
void postSignal(signal_t signal);   // post_sem

#endif /* signal_h */
