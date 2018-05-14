/* 
 * file:        homework.c
 * description: CS 5600 Homework 2
 *
 * Akshdeep Rungta, Hongxiang Wang
 * $Id: homework.c 530 2012-01-31 19:55:02Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include "hw2.h"

void print_barber_sleep(){
    printf("DEBUG: %f barber goes to sleep\n", timestamp());
}

void print_barber_wakes_up(){
    printf("DEBUG: %f barber wakes up\n", timestamp());
}

void print_customer_enters_shop(int customer){
    printf("DEBUG: %f customer %d enters shop\n", timestamp(), customer);
}

void print_customer_starts_haircut(int customer){
    printf("DEBUG: %f customer %d starts haircut\n", timestamp(), customer);
}

void print_customer_leaves_shop(int customer){
    printf("DEBUG: %f customer %d leaves shop\n", timestamp(), customer);
}

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;              // Mutex for the monitor
pthread_cond_t c_barber_chair = PTHREAD_COND_INITIALIZER;   // Condition for sleeping and waking up barber 
pthread_cond_t c_waiting_chair = PTHREAD_COND_INITIALIZER;  // Condition for customer waiting for haicut
pthread_cond_t c_haircut = PTHREAD_COND_INITIALIZER;        // Condition for customer to wait for haircut to be done
pthread_cond_t c_ready_haircut = PTHREAD_COND_INITIALIZER;  // Condition for barber to wait for customer to come to barber chair
pthread_cond_t c_leaves = PTHREAD_COND_INITIALIZER;         // Condition for barber wait for customer to leave

#define MAX 4 // The maximum number of waiting chairs

int waiting_chair_occupied = 0; //Number of waiting chairs occupied
int barber_sleep = 0;           //Flag to check if barber is asleep
int barber_chair = 0;           //Flag to check if barber chair is occupied

void * customer_counter;        // Counter for average number of customer in shop

int count_cust_enter = 0;       // Counter for number of customer entering the shop
int count_cust_leave = 0;       // Counter for number of customer leaving the shop without haircut
int count_cust_empty = 0;       // Counter for number of customer enter in empty shop

void *customer_haircut_timer;   // Timer for average time customer spend in shop with haircut
void *barber_chair_empty_timer; // Timer for average time of customer in barber chair entering when shop is empty
void *barber_chair_full_timer;  // Timer for average time of customer in barber chair entering when shop is not empty

/* the barber method
 */
void barber(void)
{
    pthread_mutex_lock(&m);
    while (1) {
        // When there are no customers waiting 
        while (waiting_chair_occupied == 0) {
            print_barber_sleep();
            barber_sleep = 0;
            pthread_cond_wait(&c_barber_chair, &m);             
            stat_timer_start(barber_chair_empty_timer);             
            sleep_exp(1.2, &m);
            pthread_cond_signal(&c_haircut);
            stat_timer_stop(barber_chair_empty_timer);
            pthread_cond_wait(&c_leaves, &m);        
        }      

        // When there are customers waiting
        pthread_cond_signal(&c_waiting_chair);    
        waiting_chair_occupied -= 1;   
        barber_chair = 1;
        stat_timer_start(barber_chair_full_timer);
        pthread_cond_wait(&c_ready_haircut, &m);
        sleep_exp(1.2, &m);
        pthread_cond_signal(&c_haircut);
        stat_timer_stop(barber_chair_full_timer);
        pthread_cond_wait(&c_leaves, &m);        
    }
    pthread_mutex_unlock(&m);
}

/* the customer method
 */

void customer(int customer_num)
{
    pthread_mutex_lock(&m);
    stat_count_incr(customer_counter);
    count_cust_enter++;
    print_customer_enters_shop(customer_num);
    // Check if shop is full
    if ((waiting_chair_occupied == MAX && barber_chair == 1) || waiting_chair_occupied == MAX + 1){
        print_customer_leaves_shop(customer_num);
        count_cust_leave++;        
        stat_count_decr(customer_counter);
        pthread_mutex_unlock(&m);        
        return;
    }

    stat_timer_start(customer_haircut_timer); 

    //Check if barber is asleep
    if (barber_sleep == 0) {
        pthread_cond_signal(&c_barber_chair);
        barber_sleep =1;
        print_barber_wakes_up(); 
        barber_chair = 1;
        count_cust_empty++;       
    } 

    //Else wait on waiting chair
    else {
        waiting_chair_occupied += 1;
        pthread_cond_wait(&c_waiting_chair, &m);  
        pthread_cond_signal(&c_ready_haircut);
        
    }
    
    // Start haircut
    print_customer_starts_haircut(customer_num);
    pthread_cond_wait(&c_haircut, &m);   
    pthread_cond_signal(&c_leaves);
    barber_chair = 0;
    print_customer_leaves_shop(customer_num);
    stat_timer_stop(customer_haircut_timer);
    stat_count_decr(customer_counter);
    pthread_mutex_unlock(&m);
   
    return;
}

/* Threads which call these methods. Note that the pthread create
 * function allows you to pass a single void* pointer value to each
 * thread you create; we actually pass an integer (the customer number)
 * as that argument instead, using a "cast" to pretend it's a pointer.
 */

/* the customer thread function - create 10 threads, each of which calls
 * this function with its customer number 0..9
 */
void *customer_thread(void *context) 
{
    int customer_num = (int)context; 
    while (1) {
        sleep_exp(10, NULL);
        customer(customer_num);
    }   
    
    return 0;
}

/*  barber thread
 */
void *barber_thread(void *context)
{
  
    barber(); /* never returns */
    return 0;
}

void q2(void)
{    
    // Create barber thread
    pthread_t barb; 
    pthread_create(&barb, NULL, barber_thread, NULL);

    // Create 10 customer threads
    pthread_t cust[10]; 
    for (int i = 0; i < 10; i++) {
        pthread_create(&cust[i], NULL, customer_thread, (int *)i);
    }
    wait_until_done();
}


void q3(void)
{
    // Initialize timers and counter
    customer_haircut_timer = stat_timer();
    barber_chair_empty_timer = stat_timer();
    barber_chair_full_timer = stat_timer();
    customer_counter = stat_counter();

    // Create barber thread
    pthread_t barb; 
    pthread_create(&barb, NULL, barber_thread, NULL);

    // Create 10 customer threads
    pthread_t cust[10]; 
    for (int i = 0; i < 10; i++) {
        pthread_create(&cust[i], NULL, customer_thread, (int *)i);
    }
    wait_until_done();

    // Fraction of customer visits that result in turning away due to a full shop       
    printf("Number of customers entered: %d\n", count_cust_enter); 
    printf("Number of customers left without haircut: %d\n", count_cust_leave);
    printf("Fraction of customer visits result that in turning away due to a full shop: %f\n",
          (double)count_cust_leave/(double)count_cust_enter);

    //Average time spent in the shop (including haircut) by a customer who does not find a full shop
    double customer_haircut_timer_mean = stat_timer_mean(customer_haircut_timer);
    printf("Average time spent in the shop (including haircut) by a customer who does not find a full shop: %f\n", 
        customer_haircut_timer_mean);
    
    //Average number of customer in shop
    double customer_counter_mean = stat_count_mean(customer_counter);
    printf("Average number of customer in shop: %f\n", customer_counter_mean);

    //Fraction of time someone is sitting in the barber's chair
    double barber_chair_empty_timer_mean = stat_timer_mean(barber_chair_empty_timer);
    double barber_chair_full_timer_mean = stat_timer_mean(barber_chair_full_timer);
    double total_time_empty = barber_chair_empty_timer_mean * count_cust_empty;
    double total_time_full = barber_chair_full_timer_mean * (count_cust_enter - count_cust_leave - count_cust_empty);
    double total = customer_haircut_timer_mean *  (count_cust_enter - count_cust_leave);
    double fraction_haircut = (total_time_empty + total_time_full)/total;
    printf("Fraction of time someone is sitting in the barber's chair: %f\n", fraction_haircut);
}
