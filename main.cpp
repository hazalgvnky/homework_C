#include <iostream>
extern "C" {
#include "hw2_output.h"
}
#include <pthread.h>
#include <unistd.h>
#include <vector>
#include <sys/time.h>


using namespace std;

class privates{
public:
    int gid;
    int priv_x, priv_y;
    int ms;
    int ng;
    int **areas;
    privates()= default;
};

class orders{
public:
    int time;
    string type;
    int usage;
    orders()= default;
};

class sneakys{
public:
    int sid;
    int ms;
    int ng;
    int **areas;
    int *num_cig;
    sneakys()= default;
};

int **grid;

int grid_x,grid_y;

int **bool_grid;

int **bool_sneaky_grid;

int num_privates;

int num_orders;

int num_sneaky;

string state = "continue";

static pthread_cond_t **func_cond;

static pthread_cond_t stateCond;

static pthread_mutex_t func_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t commander_mutex = PTHREAD_MUTEX_INITIALIZER;

privates *all_priv;

sneakys *all_sneaky;

orders *all_order;

int my_sleep(int timeInMs)
{
    struct timespec timeout;
    struct timeval now;
    long future_us;
    int r;

    gettimeofday(&now,NULL);

    future_us = now.tv_usec + timeInMs * 1000;
    timeout.tv_nsec = (future_us % 1000000) * 1000;
    timeout.tv_sec = now.tv_sec + future_us / 1000000;
    
    pthread_mutex_lock(&commander_mutex);

    r = pthread_cond_timedwait(&stateCond, &commander_mutex, &timeout);
    
    pthread_mutex_unlock(&commander_mutex);
    return r;
}

int lock_cell(privates my_private, int n){
    vector<pair<int,int>> delete_array;
    for(int i = 0;i<my_private.priv_x;i++) {

        for (int j = 0; j < my_private.priv_y; j++) {

             if(bool_grid[my_private.areas[n][0]+i][my_private.areas[n][1]+j] == 0){
                //lock the cell
                bool_grid[my_private.areas[n][0]+i][my_private.areas[n][1]+j] = 1;
                //can be needed later
                delete_array.push_back(pair<int,int> (my_private.areas[n][0]+i,my_private.areas[n][1]+j));
            }
            else{
                //it is not free to lock
                //unlock the prev cells
                for(auto a : delete_array){
                    bool_grid[a.first][a.second] = 0;
                }

                pthread_cond_wait(&func_cond[my_private.areas[n][0]+i][my_private.areas[n][1]+j], &func_mutex);
                pthread_mutex_lock(&commander_mutex);
                 if(state == "break"){

                     hw2_notify(PROPER_PRIVATE_TOOK_BREAK, my_private.gid, 0, 0);
                     //unlock cells and signal cells

                     //wait until continue
                     pthread_cond_wait(&stateCond, &commander_mutex);

                     if(state == "stop"){
                         //hw2_notify(PROPER_PRIVATE_STOPPED, my_private.gid, 0, 0);
                         pthread_mutex_unlock(&commander_mutex);
                         return -1;
                     }

                     hw2_notify(PROPER_PRIVATE_CONTINUED, my_private.gid, 0, 0);

                 }

                 if(state == "stop"){
                     //hw2_notify(PROPER_PRIVATE_STOPPED, my_private.gid, 0, 0);
                     pthread_mutex_unlock(&commander_mutex);
                     return -1;
                 }
                 pthread_mutex_unlock(&commander_mutex);
                return lock_cell(my_private, n);
            }

        }
    }
}

int lock_cell_sneaky(sneakys my_sneaky, int n){
    vector<pair<int,int>> delete_array;
    if(bool_sneaky_grid[my_sneaky.areas[n][0]][my_sneaky.areas[n][1]] == 0) {

        for (int i = 0; i < 3; i++) {

            for (int j = 0; j < 3; j++) {

                if (bool_grid[my_sneaky.areas[n][0] - 1 + i][my_sneaky.areas[n][1] - 1 + j] == 0) {
                    //lock the cell
                    pthread_mutex_lock(&commander_mutex);

                    if(state == "stop"){
                        //hw2_notify(SNEAKY_SMOKER_STOPPED, my_sneaky.sid, 0, 0);
                        pthread_mutex_unlock(&commander_mutex);
                        return -1;
                    }

                    pthread_mutex_unlock(&commander_mutex);
                    bool_grid[my_sneaky.areas[n][0] - 1 + i][my_sneaky.areas[n][1] - 1 + j] = 2;
                    //push the x,y incase they are needed
                    delete_array.push_back({my_sneaky.areas[n][0] - 1 + i, my_sneaky.areas[n][1] - 1 + j});
                }
                else {
                    //it is not free to lock
                    //unlock the prev cells
                    for (auto a : delete_array) {
                        bool_grid[a.first][a.second] = 0;
                    }
                    pthread_cond_wait(&func_cond[my_sneaky.areas[n][0]-1+i][my_sneaky.areas[n][1]-1+j], &func_mutex);

                    pthread_mutex_lock(&commander_mutex);

                    if(state == "stop"){
                        //hw2_notify(SNEAKY_SMOKER_STOPPED, my_sneaky.sid, 0, 0);
                        pthread_mutex_unlock(&commander_mutex);
                        return -1;
                    }

                    pthread_mutex_unlock(&commander_mutex);

                    return lock_cell_sneaky(my_sneaky, n);
                }
            }
        }
    }
    else{
        pthread_cond_wait(&func_cond[my_sneaky.areas[n][0]][my_sneaky.areas[n][1]], &func_mutex);
        return lock_cell_sneaky(my_sneaky, n);
    }
}

int litter_cell(sneakys my_sneaky, int n){

    for(int c = 0; c<my_sneaky.num_cig[n]; ){

        //cell#1
        if(c == my_sneaky.num_cig[n]){
            break;
        }
        my_sleep(my_sneaky.ms);

        pthread_mutex_lock(&commander_mutex);

        if(state == "stop"){
            pthread_mutex_unlock(&commander_mutex);
            return -1;
        }

        grid[my_sneaky.areas[n][0]-1][my_sneaky.areas[n][1]-1]++;

        hw2_notify(SNEAKY_SMOKER_FLICKED, my_sneaky.sid, my_sneaky.areas[n][0]-1, my_sneaky.areas[n][1]-1);

        c++;

        pthread_mutex_unlock(&commander_mutex);


        //cell#2
        if(c == my_sneaky.num_cig[n]){
            break;
        }
        my_sleep(my_sneaky.ms);

        pthread_mutex_lock(&commander_mutex);

        if(state == "stop"){
            pthread_mutex_unlock(&commander_mutex);
            return -1;
        }

        grid[my_sneaky.areas[n][0]-1][my_sneaky.areas[n][1]-1+1]++;

        hw2_notify(SNEAKY_SMOKER_FLICKED, my_sneaky.sid, my_sneaky.areas[n][0]-1, my_sneaky.areas[n][1]-1+1);

        c++;

        pthread_mutex_unlock(&commander_mutex);

        //cell#3
        if(c == my_sneaky.num_cig[n]){
            break;
        }
        my_sleep(my_sneaky.ms);

        pthread_mutex_lock(&commander_mutex);

        if(state == "stop"){
            pthread_mutex_unlock(&commander_mutex);
            return -1;
        }

        grid[my_sneaky.areas[n][0]-1][my_sneaky.areas[n][1]-1+2]++;

        hw2_notify(SNEAKY_SMOKER_FLICKED, my_sneaky.sid, my_sneaky.areas[n][0]-1, my_sneaky.areas[n][1]-1+2);

        c++;

        pthread_mutex_unlock(&commander_mutex);

        //cell#4
        if(c == my_sneaky.num_cig[n]){
            break;
        }
        my_sleep(my_sneaky.ms);

        pthread_mutex_lock(&commander_mutex);

        if(state == "stop"){
            pthread_mutex_unlock(&commander_mutex);
            return -1;
        }

        grid[my_sneaky.areas[n][0]-1+1][my_sneaky.areas[n][1]-1+2]++;

        hw2_notify(SNEAKY_SMOKER_FLICKED, my_sneaky.sid, my_sneaky.areas[n][0]-1+1, my_sneaky.areas[n][1]-1+2);

        c++;

        pthread_mutex_unlock(&commander_mutex);

        //cell#5
        if(c == my_sneaky.num_cig[n]){
            break;
        }
        my_sleep(my_sneaky.ms);

        pthread_mutex_lock(&commander_mutex);

        if(state == "stop"){
            pthread_mutex_unlock(&commander_mutex);
            return -1;
        }

        grid[my_sneaky.areas[n][0]-1+2][my_sneaky.areas[n][1]-1+2]++;

        hw2_notify(SNEAKY_SMOKER_FLICKED, my_sneaky.sid, my_sneaky.areas[n][0]-1+2, my_sneaky.areas[n][1]-1+2);

        c++;

        pthread_mutex_unlock(&commander_mutex);

        //cell#6
        if(c == my_sneaky.num_cig[n]){
            break;
        }
        my_sleep(my_sneaky.ms);

        pthread_mutex_lock(&commander_mutex);

        if(state == "stop"){
            pthread_mutex_unlock(&commander_mutex);
            return -1;
        }

        grid[my_sneaky.areas[n][0]-1+2][my_sneaky.areas[n][1]-1+1]++;

        hw2_notify(SNEAKY_SMOKER_FLICKED, my_sneaky.sid, my_sneaky.areas[n][0]-1+2, my_sneaky.areas[n][1]-1+1);

        c++;

        pthread_mutex_unlock(&commander_mutex);

        //cell#7
        if(c == my_sneaky.num_cig[n]){
            break;
        }
        my_sleep(my_sneaky.ms);

        pthread_mutex_lock(&commander_mutex);

        if(state == "stop"){
            pthread_mutex_unlock(&commander_mutex);
            return -1;
        }

        grid[my_sneaky.areas[n][0]-1+2][my_sneaky.areas[n][1]-1]++;

        hw2_notify(SNEAKY_SMOKER_FLICKED, my_sneaky.sid, my_sneaky.areas[n][0]-1+2, my_sneaky.areas[n][1]-1);

        c++;

        pthread_mutex_unlock(&commander_mutex);

        //cell#8
        if(c == my_sneaky.num_cig[n]){
            break;
        }
        my_sleep(my_sneaky.ms);

        pthread_mutex_lock(&commander_mutex);

        if(state == "stop"){
            pthread_mutex_unlock(&commander_mutex);
            return -1;
        }

        grid[my_sneaky.areas[n][0]-1+1][my_sneaky.areas[n][1]-1]++;

        hw2_notify(SNEAKY_SMOKER_FLICKED, my_sneaky.sid, my_sneaky.areas[n][0]-1+1, my_sneaky.areas[n][1]-1);

        c++;

        pthread_mutex_unlock(&commander_mutex);

    }
    return 0;
}

int clean_cell(privates my_private, int n){

    for(int i=0;i<my_private.priv_x;i++){

        for(int j=0;j<my_private.priv_y;j++){

            int cig_num = grid[my_private.areas[n][0]+i][my_private.areas[n][1]+j];
            for(int h = 0; h<cig_num; h++){

                my_sleep(my_private.ms);
                pthread_mutex_lock(&commander_mutex);
                if(state == "break"){
                    hw2_notify(PROPER_PRIVATE_TOOK_BREAK, my_private.gid, 0, 0);
                    //unlock cells and signal cells

                    //wait until continue
                    pthread_cond_wait(&stateCond, &commander_mutex);

                    if(state == "stop"){
                        pthread_mutex_unlock(&commander_mutex);
                        return -1;
                    }
                    pthread_mutex_unlock(&commander_mutex);
                    return 1;

                }
                if(state == "stop"){
                    pthread_mutex_unlock(&commander_mutex);
                    return -1;
                }


                grid[my_private.areas[n][0]+i][my_private.areas[n][1]+j]--;
                hw2_notify(PROPER_PRIVATE_GATHERED, my_private.gid, my_private.areas[n][0]+i, my_private.areas[n][1]+j);
                pthread_mutex_unlock(&commander_mutex);

            }
        }
    }
    return 0;
}

void* routine(void* arg){
    privates my_private;
    my_private = *(privates *) arg;

    hw2_notify(PROPER_PRIVATE_CREATED, my_private.gid, 0, 0);

    for(int n=0;n<my_private.ng;n++){

        pthread_mutex_lock(&func_mutex);

        int lockres = lock_cell(my_private, n);
        pthread_mutex_unlock(&func_mutex);
        if (lockres == -1){
            hw2_notify(PROPER_PRIVATE_STOPPED, my_private.gid, 0, 0);
            return nullptr;
        }

        pthread_mutex_lock(&commander_mutex);
        if(state == "break"){

            hw2_notify(PROPER_PRIVATE_TOOK_BREAK, my_private.gid, 0, 0);
            //unlock cells and signal cells

            //wait until continue
            pthread_cond_wait(&stateCond, &commander_mutex);

            if(state == "stop"){
                hw2_notify(PROPER_PRIVATE_STOPPED, my_private.gid, 0, 0);
                pthread_mutex_unlock(&commander_mutex);
                return nullptr;
            }

            hw2_notify(PROPER_PRIVATE_CONTINUED, my_private.gid, 0, 0);

        }
        if(state == "stop"){
            hw2_notify(PROPER_PRIVATE_STOPPED, my_private.gid, 0, 0);
            pthread_mutex_unlock(&commander_mutex);
            return nullptr;
        }
        pthread_mutex_unlock(&commander_mutex);
        hw2_notify(PROPER_PRIVATE_ARRIVED, my_private.gid, my_private.areas[n][0], my_private.areas[n][1]);

        int res = clean_cell(my_private, n);
        switch (res) {
            case 1: --n; hw2_notify(PROPER_PRIVATE_CONTINUED, my_private.gid, 0, 0); continue;
            case -1: hw2_notify(PROPER_PRIVATE_STOPPED, my_private.gid, 0, 0); return nullptr;
            case 0: break;
        }


        pthread_mutex_lock(&commander_mutex);
        if(state == "break"){

            hw2_notify(PROPER_PRIVATE_TOOK_BREAK, my_private.gid, 0, 0);
            //unlock cells and signal cells

            //wait until continue
            pthread_cond_wait(&stateCond, &commander_mutex);

            if(state == "stop"){
                hw2_notify(PROPER_PRIVATE_STOPPED, my_private.gid, 0, 0);
                pthread_mutex_unlock(&commander_mutex);
                return nullptr;
            }

            hw2_notify(PROPER_PRIVATE_CONTINUED, my_private.gid, 0, 0);

        }

        if(state == "stop"){
            hw2_notify(PROPER_PRIVATE_STOPPED, my_private.gid, 0, 0);
            pthread_mutex_unlock(&commander_mutex);
            return nullptr;
        }
        pthread_mutex_unlock(&commander_mutex);

        hw2_notify(PROPER_PRIVATE_CLEARED, my_private.gid, 0, 0);

        for(int i=0;i<my_private.priv_x;i++){

            for(int j=0;j<my_private.priv_y;j++){

                bool_grid[my_private.areas[n][0]+i][my_private.areas[n][1]+j] = 0;

                pthread_cond_signal(&func_cond[my_private.areas[n][0]+i][my_private.areas[n][1]+j]);

            }
        }
    }

    hw2_notify(PROPER_PRIVATE_EXITED, my_private.gid, 0, 0);

    return nullptr;
}

void* routine2(void* arg){
    sneakys my_sneaky;
    my_sneaky = *(sneakys *) arg;

    hw2_notify(SNEAKY_SMOKER_CREATED, my_sneaky.sid, 0, 0);

    for(int n = 0; n<my_sneaky.ng; n++){

        pthread_mutex_lock(&func_mutex);

        int lockres = lock_cell_sneaky(my_sneaky, n);

        pthread_mutex_unlock(&func_mutex);

        if(lockres == -1){
            hw2_notify(SNEAKY_SMOKER_STOPPED, my_sneaky.sid, 0, 0);
            return nullptr;
        }

        pthread_mutex_lock(&commander_mutex);

        if(state == "stop"){
            hw2_notify(SNEAKY_SMOKER_STOPPED, my_sneaky.sid, 0, 0);
            pthread_mutex_unlock(&commander_mutex);
            return nullptr;
        }


        hw2_notify(SNEAKY_SMOKER_ARRIVED, my_sneaky.sid, my_sneaky.areas[n][0], my_sneaky.areas[n][1]);
        pthread_mutex_unlock(&commander_mutex);


        int res = litter_cell(my_sneaky, n);

        switch (res) {
            case -1: hw2_notify(SNEAKY_SMOKER_STOPPED, my_sneaky.sid, 0, 0); return nullptr;
            case 0: break;
        }

        pthread_mutex_lock(&commander_mutex);

        if(state == "stop"){
            hw2_notify(SNEAKY_SMOKER_STOPPED, my_sneaky.sid, 0, 0);
            pthread_mutex_unlock(&commander_mutex);
            return nullptr;
        }

        pthread_mutex_unlock(&commander_mutex);

        hw2_notify(SNEAKY_SMOKER_LEFT, my_sneaky.sid, 0, 0);

        bool_sneaky_grid[my_sneaky.areas[n][0]][my_sneaky.areas[n][1]] = 0;

        for(int i=0;i<3;i++){

            for(int j=0;j<3;j++){

                bool_grid[my_sneaky.areas[n][0]-1+i][my_sneaky.areas[n][1]-1+j] = 0;

                pthread_cond_signal(&func_cond[my_sneaky.areas[n][0]-1+i][my_sneaky.areas[n][1]-1+j]);

            }
        }
    }

    hw2_notify(SNEAKY_SMOKER_EXITED, my_sneaky.sid, 0, 0);

    return nullptr;
}


void get_input(){
    //grids
    cin>>grid_x;
    cin>>grid_y;

    grid = new int*[grid_x];
    bool_grid = new int*[grid_x];
    bool_sneaky_grid = new int*[grid_x];

    for(int i = 0 ; i < grid_x; i++){
        grid[i] = new int[grid_y];
        bool_grid[i] = new int[grid_y];
        bool_sneaky_grid[i] = new int[grid_y];
    }

    for(int i = 0; i<grid_x; i++){
        for(int j = 0; j<grid_y; j++){
            cin>>grid[i][j];
            bool_grid[i][j] = 0;
            bool_sneaky_grid[i][j] = 0;
        }
    }

    //all privates
    cin>>num_privates;
    all_priv = new privates[num_privates];

    for(int i = 0; i<num_privates; i++){
        privates new_private;
        cin>>new_private.gid;
        cin>>new_private.priv_x;
        cin>>new_private.priv_y;
        cin>>new_private.ms;
        cin>>new_private.ng;
        new_private.areas = new int*[new_private.ng];
        for(int j = 0; j<new_private.ng; j++){
            new_private.areas[j] = new int[2];
            cin>>new_private.areas[j][0];
            cin>>new_private.areas[j][1];
        }
        all_priv[i] = new_private;
    }

    //all orders
    cin>>num_orders;
    all_order = new orders[num_orders];

    //check break-continue relationship
    int break_continue = 0;
    int stop_check = 0;

    for(int i = 0; i<num_orders; i++){
        orders new_order;
        cin>>new_order.time;
        cin>>new_order.type;
        new_order.usage = 0;

        if(new_order.type == "break"){

            if(break_continue == 1 || stop_check == 1){
                new_order.usage = 1;
            }
            else{
                break_continue = 1;
            }

        }

        else if(new_order.type == "continue"){

            if(break_continue == 0 || stop_check == 1){
                new_order.usage = 1;
            }
            else{
                break_continue = 0;
            }
        }

        else if(new_order.type == "stop"){
            if(stop_check == 0){
                stop_check = 1;
            }
            else{
                new_order.usage = 1;
            }
        }

        all_order[i] = new_order;
    }

    //all sneaky smokers
    cin>>num_sneaky;
    all_sneaky = new sneakys[num_sneaky];

    for(int i = 0; i<num_sneaky; i++){
        sneakys new_sneaky;
        cin>>new_sneaky.sid;
        cin>>new_sneaky.ms;
        cin>>new_sneaky.ng;
        new_sneaky.areas = new int*[new_sneaky.ng];
        new_sneaky.num_cig = new int[new_sneaky.ng];
        for(int j = 0; j<new_sneaky.ng; j++){
            new_sneaky.areas[j] = new int[2];
            cin>>new_sneaky.areas[j][0];
            cin>>new_sneaky.areas[j][1];
            cin>>new_sneaky.num_cig[j];
        }
        all_sneaky[i] = new_sneaky;
    }

}

void my_thread(){
    //threads
    pthread_t threads[num_privates];
    pthread_t s_threads[num_sneaky];

    //cond
    func_cond = new pthread_cond_t*[grid_x];
    for(int i=0;i<grid_x;i++){
        func_cond[i] = new pthread_cond_t[grid_y];
        for(int j=0;j<grid_y;j++){
            pthread_cond_init(&func_cond[i][j], NULL);
        }
    }
    pthread_cond_init(&stateCond, NULL);

    //mutex
    pthread_mutex_init(&func_mutex, NULL);
    pthread_mutex_init(&commander_mutex, NULL);

    //create
    for(int i=0;i<num_privates;i++){
        if(pthread_create(&threads[i], NULL, &routine, &all_priv[i]) != 0){
            return;
        }
    }

    for(int i=0;i<num_sneaky;i++){
        if(pthread_create(&s_threads[i], NULL, &routine2, &all_sneaky[i]) != 0){
            return;
        }
    }

    //part2 orders
    for(int i=0; i<num_orders; i++){
        if(all_order[i].type == "break"){
            usleep(all_order[i].time*1000);

            hw2_notify(ORDER_BREAK, 0, 0, 0);

            if(all_order[i].usage == 0){
                state = "break";

                for (int j = 0; j < grid_x; ++j) {
                    for (int k = 0; k < grid_y; ++k) {
                        if (bool_grid[j][k] == 1 && bool_sneaky_grid[j][k] != 1){
                            bool_grid[j][k] = 0;
                            pthread_cond_broadcast(&func_cond[j][k]);
                        }
                    }
                }


                pthread_cond_broadcast(&stateCond);
            }
        }
        else if(all_order[i].type == "continue"){
            usleep(all_order[i].time*1000);

            hw2_notify(ORDER_CONTINUE, 0, 0, 0);

            if(all_order[i].usage == 0){
                state = "continue";
                pthread_cond_broadcast(&stateCond);
            }
        }
        else if(all_order[i].type == "stop"){
            usleep(all_order[i].time*1000);

            hw2_notify(ORDER_STOP, 0, 0, 0);

            if(all_order[i].usage == 0){
                state = "stop";

                for (int j = 0; j < grid_x; ++j) {
                    for (int k = 0; k < grid_y; ++k) {
                        if (bool_grid[j][k] == 1 && bool_sneaky_grid[j][k] != 1){
                            bool_grid[j][k] = 0;
                            pthread_cond_broadcast(&func_cond[j][k]);
                        }
                    }
                }

                pthread_cond_broadcast(&stateCond);

                //join
                for(int i=0;i<num_privates;i++){
                    if(pthread_join(threads[i], NULL) != 0){
                        return;
                    }
                }

                for(int i=0;i<num_sneaky;i++){
                    if(pthread_join(s_threads[i], NULL) != 0){
                        return;
                    }
                }

                i++;

                while(i<num_orders){
                    if(all_order[i].type == "break"){
                        usleep(all_order[i].time*1000);

                        hw2_notify(ORDER_BREAK, 0, 0, 0);
                    }
                    else if(all_order[i].type == "continue"){
                        usleep(all_order[i].time*1000);

                        hw2_notify(ORDER_CONTINUE, 0, 0, 0);
                    }
                    else if(all_order[i].type == "stop"){
                        usleep(all_order[i].time*1000);

                        hw2_notify(ORDER_STOP, 0, 0, 0);
                    }
                    i++;
                }

                //destroy
                pthread_mutex_destroy(&func_mutex);
                pthread_mutex_destroy(&commander_mutex);

                for(int i=0;i<grid_x;i++){
                    for(int j=0;j<grid_y;j++){
                        pthread_cond_destroy(&func_cond[i][j]);
                    }
                }
                pthread_cond_destroy(&stateCond);

                return;
            }
        }
    }

    //join
    for(int i=0;i<num_privates;i++){
        if(pthread_join(threads[i], NULL) != 0){
            return;
        }
    }

    for(int i=0;i<num_sneaky;i++){
        if(pthread_join(s_threads[i], NULL) != 0){
            return;
        }
    }

    //destroy
    pthread_mutex_destroy(&func_mutex);
    pthread_mutex_destroy(&commander_mutex);

    for(int i=0;i<grid_x;i++){
        for(int j=0;j<grid_y;j++){
            pthread_cond_destroy(&func_cond[i][j]);
        }
    }
    pthread_cond_destroy(&stateCond);
}

int main() {
    hw2_init_notifier();
    get_input();
    my_thread();
    return 0;
}