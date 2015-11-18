//
//  main.c
//  IceCreamShop
//
//  Created by ronglei on 15/11/13.
//  Copyright © 2015年 ronglei. All rights reserved.
//

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "signal.h"

#define kCountOfCustomers       3   // 顾客数量
#define kMaxCreamsPerCustomer   3   // 每个顾客能够购买冰淇淋的最大数量

// 获取0-x的随机数
static inline int x_random(x)
{
    sleep(1);
    return rand()%(x+1);
}

// 同步管理员检查的结构体
typedef struct {
    bool     passed;    // 操作员制作的冰淇淋是否合格的反馈标识
    signal_t request;   // 操作员发起检验信号量
    signal_t finish;    // 管理员检验完成信号量
    signal_t lock;      // 管理员资源竞争信号量
}inspect_s, *inspect_p;

void inspectNew(inspect_p inspect)
{
    inspect->passed = false;
    inspect->request = malloc(sizeof(signal_s));
    inspect->finish = malloc(sizeof(signal_s));
    inspect->lock = malloc(sizeof(signal_s));
}

// 同步收银员收银的结构体
typedef struct{
    int      number;    // 当前结账顾客的队列编号
    signal_t request;   // 顾客发起结账信号量
    signal_t customers[kCountOfCustomers]; // 每一个顾客维持一个信号量保证顺序
    signal_t lock;      // 收银员竞争信号量
}payment_s, *payment_p;

void paymentNew(payment_p payment)
{
    payment->number = 0;
    payment->request = malloc(sizeof(signal_s));
    payment->lock = malloc(sizeof(signal_s));
    for (int i = 0; i < kCountOfCustomers; i++) {
        payment->customers[i] = malloc(sizeof(signal_s));
    }
}

// 传递给customer线程的参数
typedef struct {
    int customerIndex;          // 顾客编号
    int numberOfRequestCreams;  // 该顾客请求冰淇淋数量
}customer_s, *customer_p;

// 全局参数
payment_s payment;
inspect_s inspect;

int numberOfMakedCreams = 0;

// 管理员线程
void *managerTask(void *creams)
{
    int totalCreams = (*(int *)creams); // 冰淇淋总数
    printf("管理员需要检验合格%d个冰淇淋\n" ,totalCreams);
    int numRefused = 0;     // 不合格的数量
    int numApproved = 0;    // 合格的数量
    int numInspect = 0;     // 检验的总次数
    
    while (numApproved < totalCreams) {
        waitSignal(inspect.request);    // 等待操作员发起检验请求
        numInspect++;
        printf("管理员正在检验第%d个冰淇淋\n" ,numInspect);
        inspect.passed = x_random(1);   // 0-1随机数决定是否合格
        
        if (inspect.passed){
            numApproved++;
            printf("%d个冰淇淋通过了检验\n", numApproved);
        }else{
            numRefused ++;
            printf("%d个冰淇淋未通过检验\n", numRefused);
        }
        postSignal(inspect.finish);     // 告诉操作员检验完成
    }
    
    return NULL;
}

// 制作冰淇淋
void makeCream()
{
    numberOfMakedCreams ++;
    printf("操作员正在制作第%d冰淇淋\n", numberOfMakedCreams);
}

// 操作员线程
void *clerkTask(void *sem)
{
    bool passed = false;    // 检验是否合格的标识
    while (!passed) {       // 知道合格为止
        makeCream();
        waitSignal(inspect.lock);   // 竞争管理员资源

        postSignal(inspect.request);// 发起检验请求
        waitSignal(inspect.finish); // 等待检验结束
        passed = inspect.passed;    // 记录检验结果
        
        postSignal(inspect.lock);   // 释放管理员资源
    }
    
    postSignal(sem);                // 将合格的冰淇淋交给顾客
    
    return NULL;
}

// 去付款
void walkToCashier(customer_s customer)
{
    printf("第%d号顾客的%d个冰淇淋已经做好了,可以去找收银员付款了\n", customer.customerIndex, customer.numberOfRequestCreams);
}

// 顾客线程
void *customerTask(void *customer)
{
    customer_s customerInfo = *(customer_p)customer;// 接受管理员信息
    int numberOfNeedCreams = customerInfo.numberOfRequestCreams;
    signal_s    clerkDone[numberOfNeedCreams];      // 创建信号量数组等待合格的冰淇淋
    pthread_t   clerk[numberOfNeedCreams];          // 创建操作员线程数组去制作冰淇淋
    for (int i = 0; i < numberOfNeedCreams; i ++) {
        char *name = malloc(sizeof(char *));        // 信号量命名
        sprintf(name, "clerk_done_%d", i);
        initSignal(&clerkDone[i], name, 0);
        
        // 执行操作员线程并将信号量传递过去 等操作员获取到合格的冰淇淋后 通过该信号量通知顾客
        pthread_create(&clerk[i], NULL, clerkTask, &clerkDone[i]);
        free(name);
    }
   
    // 等待所有的操作员都拿到合格的冰淇淋
    for (int i = 0; i < numberOfNeedCreams; i ++) {
        waitSignal(&clerkDone[i]);
    }
    
    walkToCashier(customerInfo);    // 去排队付款
    waitSignal(payment.lock);       // 竞争收银员资源
    int place = payment.number ++;  // 记录当前正在付款的顾客队列好
    
    postSignal(payment.request);    // 请求付款
    waitSignal(payment.customers[place]); // 等待收银员找钱
    
    postSignal(payment.lock);       // 释放收银员资源
    
    // 释放内存
    free(customer);
    for (int i = 0; i < numberOfNeedCreams; i ++) {
        pthread_join(clerk[i], NULL);
    }
    
    return NULL;
}

// 收银员收款
void collectMoney(int customerIndex)
{
    printf("第%d号顾客付款完毕\n", customerIndex);
}

// 收银员线程
void *cashierTask()
{
    for (int i = 0; i < kCountOfCustomers; i ++) {
        waitSignal(payment.request);      // 等待付款请求
        collectMoney(i);                  // 找钱
        postSignal(payment.customers[i]); // 结账结束通知顾客
    }
    
    return NULL;
}

int main(int argc, const char * argv[])
{
    srand((unsigned) time(0));  // 随机因子
    
    // 初始化全局数据
    inspectNew(&inspect);
    initSignal(inspect.request, "clerk_request", 0);
    initSignal(inspect.finish, "clerk_finish", 0);
    initSignal(inspect.lock, "clerk_lock", 1);
    paymentNew(&payment);
    initSignal(payment.request, "customer_request", 0);
    for (int i = 0; i < kCountOfCustomers; i ++) {
        char *name = malloc(sizeof(char *));
        sprintf(name, "customer_%d", i);
        initSignal(payment.customers[i] ,name, 0);
        free(name);
    }
    initSignal(payment.lock ,"customer_lock", 1);
    
    // 调起顾客线程
    int totalCreams = 0;
    pthread_t customer[kCountOfCustomers];
    
    for (int i = 0; i < kCountOfCustomers; i ++) {
        customer_p customerInfo = malloc(sizeof(customer_s));
        customerInfo->customerIndex = i;
        customerInfo->numberOfRequestCreams = x_random(kMaxCreamsPerCustomer) + 1;
        
        printf("第%d号顾客请求了%d个冰淇淋\n", i, customerInfo->numberOfRequestCreams);
        pthread_create(&customer[i], NULL, customerTask, customerInfo);
        
        totalCreams += customerInfo->numberOfRequestCreams;
    }
    
    // 调起管理员和收银员线程
    pthread_t manager;
    pthread_t cashier;
    
    pthread_create(&manager, NULL, managerTask, &totalCreams);
    pthread_create(&cashier, NULL, cashierTask, NULL);
    
    // 等待线程结束回收内存
    for (int i = 0; i < kCountOfCustomers; i ++) {
        pthread_join(customer[i], NULL);
    }
    pthread_join(manager, NULL);
    pthread_join(cashier, NULL);
    
    return 0;
}
