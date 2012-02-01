// realtime ranking deamon
// read from port 6667, param is(int32_t type, ...)
// add from port 6666, param is (int32_t type, int32_t group_id, int32_t player_id, int32_t add_point), type is 1
// compile with gcc server.c -lpthread

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<pthread.h>

#define MAXLINE 64
#define MAX_GROUP 20
#define MAX_PLAYER 1000000
#define MAX_RANK 200000
#define MAX_POINT 60
#define MAX_PAGENUM 100

#define ADD 1
#define SEARCH_1 2
#define SEARCH_2 3
#define SUM 4
#define CLEAN 5

typedef struct _PlayerNode
{
    int32_t rank_id[MAX_GROUP];
} PlayerNode;

typedef struct _RankNode
{
    int32_t player_id;
    int32_t point;
    int32_t rank;
} RankNode;

PlayerNode* player;
RankNode** rank;
int32_t rank_end;
int32_t total_point[MAX_GROUP] = {0};

pthread_mutex_t *mut_read, *mut_write;

int32_t get(char* buf)
{
    return ((int32_t*)buf)[0];
}

void process_write(char* buf)
{
    pthread_mutex_lock (mut_read);
    pthread_mutex_lock (mut_write);
    pthread_mutex_unlock (mut_read);
    int32_t type = get(buf);
    if (type == ADD) {
        int32_t group_id = get(buf+4);
        int32_t player_id = get(buf+8);
        int32_t point = get(buf+12);
        if(player_id<0 || player_id>=MAX_PLAYER) return;
        if(group_id<0 || group_id>=MAX_GROUP) return;
        if(point <= 0 || point > MAX_POINT) return;
        total_point[group_id] += point;
        if(player[player_id].rank_id[group_id] == -1) {
            rank_end++;
            int32_t rank_temp = rank_end;
            while(1) {
                rank_temp--;
                if(rank_temp == 0) {
                    rank[group_id][rank_temp].point = point;
                    rank[group_id][rank_temp].player_id = player_id;
                    rank[group_id][rank_temp].rank = rank_temp+1;
                    player[player_id].rank_id[group_id] = rank_temp;
                    break;
                }
                if(rank[group_id][rank_temp-1].point<point) {
                    rank[group_id][rank_temp].point = rank[group_id][rank_temp-1].point;
                    rank[group_id][rank_temp].player_id = rank[group_id][rank_temp-1].player_id;
                    rank[group_id][rank_temp].rank = rank_temp+1;
                    player[rank[group_id][rank_temp].player_id].rank_id[group_id] = rank_temp;
                } else {
                    rank[group_id][rank_temp].point = point;
                    rank[group_id][rank_temp].player_id = player_id;
                    rank[group_id][rank_temp].rank = rank_temp+1;
                    player[player_id].rank_id[group_id] = rank_temp;
                    break;
                }
            }
        } else {
            point += rank[group_id][player[player_id].rank_id[group_id]].point;
            int32_t rank_temp = player[player_id].rank_id[group_id];
            while(1) {
                if(rank_temp == 0) {
                    rank[group_id][rank_temp].point = point;
                    rank[group_id][rank_temp].player_id = player_id;
                    rank[group_id][rank_temp].rank = rank_temp+1;
                    player[player_id].rank_id[group_id] = rank_temp;
                    break;
                }
                rank_temp--;
                if(rank[group_id][rank_temp].point<point) {
                    rank[group_id][rank_temp+1].point = rank[group_id][rank_temp].point;
                    rank[group_id][rank_temp+1].player_id = rank[group_id][rank_temp].player_id;
                    rank[group_id][rank_temp+1].rank = rank_temp+2;
                    player[rank[group_id][rank_temp+1].player_id].rank_id[group_id] = rank_temp+1;
                } else {
                    rank[group_id][rank_temp+1].point = point;
                    rank[group_id][rank_temp+1].player_id = player_id;
                    rank[group_id][rank_temp+1].rank = rank_temp+2;
                    player[player_id].rank_id[group_id] = rank_temp+1;
                    break;
                }
            }
        }
    } else if(type == CLEAN) {
        int j;
        for(j=0;j<MAX_GROUP;j++) {
            memset(rank[j], 0, MAX_RANK*sizeof(RankNode));
        }
        memset(player, 255, MAX_PLAYER*sizeof(PlayerNode));
        printf("ranking cleaned\n");
    }
    pthread_mutex_unlock (mut_write);
}

void process_read(char* buf, int32_t connfd)
{
    pthread_mutex_lock (mut_read);
    pthread_mutex_lock (mut_write);
    int32_t type = get(buf);
    if (type == SUM) {
        int32_t group_id = get(buf+4);
        write(connfd, &total_point[group_id], 4);
    } else if (type == SEARCH_1 ) {
        int32_t group_id = get(buf+4);
        int32_t player_id = get(buf+8);
        int32_t page_num = get(buf+12);
        if(player_id<0 || player_id>=MAX_PLAYER) return;
        if(group_id<0 || group_id>=MAX_GROUP) return;
        if(page_num<1 || page_num>MAX_PAGENUM) return;
        int32_t page = (int32_t)(player[player_id].rank_id[group_id]/page_num)+1;
        write(connfd, &rank[group_id][page*page_num-page_num], page_num*sizeof(RankNode));
    } else if (type == SEARCH_2 ) {
        int32_t group_id = get(buf+4);
        int32_t page = get(buf+8);
        int32_t page_num = get(buf+12);
        if(group_id<0 || group_id>=MAX_GROUP) return;
        if(page_num<1 || page_num>MAX_PAGENUM) return;
        if(page*page_num >= rank_end) return;
        write(connfd, &rank[group_id][page*page_num-page_num], page_num*sizeof(RankNode));
    } else {
        return;
    }
    pthread_mutex_unlock (mut_write);
    pthread_mutex_unlock (mut_read);
}

void *write_thread(void *args) {
    int32_t listen_write = *((int32_t*)args);
    printf("write process start\n");
    while(1){
        int32_t conn_write;
        if((conn_write = accept(listen_write, (struct sockaddr*)NULL, NULL)) == -1){
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            continue;
        }
        printf("write +1\n");
        char write_buff[64];
        recv(conn_write, write_buff, MAXLINE, 0);

        process_write(write_buff);

        close(conn_write);
    }
}

void *read_thread(void *args) {
    int32_t listen_read = *((int32_t*)args);
    printf("read process start\n");
    while(1){
        int32_t conn_read;
        if((conn_read = accept(listen_read, (struct sockaddr*)NULL, NULL)) == -1){
            printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
            continue;
        }
        printf("read +1\n");
//        if(fork()>0) continue; /*handle connection in a child process */
        char read_buff[64];
        recv(conn_read, read_buff, MAXLINE, 0);

        process_read(read_buff, conn_read);

        close(conn_read);
//        exit(0);
    }
    exit(0);
}
    
int32_t main(int32_t argc, char** argv)
{
    int32_t listen_write, listen_read;
    struct sockaddr_in write_addr;
    struct sockaddr_in read_addr;

    mut_read = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
    mut_write = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
    pthread_mutex_init (mut_read, NULL);
    pthread_mutex_init (mut_write, NULL);

    player = (PlayerNode*)malloc(MAX_PLAYER*sizeof(PlayerNode));
    rank = (RankNode**)malloc(MAX_GROUP*sizeof(RankNode*));
    memset(player, 255, MAX_PLAYER*sizeof(PlayerNode));
    memset(rank, 0, MAX_GROUP*sizeof(RankNode*));

    int32_t j = 0;
    for (j=0;j<MAX_GROUP;j++) {
        rank[j] = (RankNode*)malloc(MAX_RANK*sizeof(RankNode));
        memset(rank[j], 0, MAX_RANK*sizeof(RankNode));
    }

    if( (listen_write = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }

    if( (listen_read = socket(AF_INET, SOCK_STREAM, 0)) == -1 ){
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }

    memset(&write_addr, 0, sizeof(write_addr));
    write_addr.sin_family = AF_INET;
    write_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    write_addr.sin_port = htons(6666);

    memset(&read_addr, 0, sizeof(read_addr));
    read_addr.sin_family = AF_INET;
    read_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    read_addr.sin_port = htons(6667);

    if( bind(listen_write, (struct sockaddr*)&write_addr, sizeof(write_addr)) == -1){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    if( bind(listen_read, (struct sockaddr*)&read_addr, sizeof(read_addr)) == -1){
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }

    if( listen(listen_write, 10) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }
    if( listen(listen_read, 10) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    }

    pthread_t p_write, p_read;
    pthread_create(&p_write, NULL, write_thread, &listen_write);
    pthread_create(&p_read, NULL, read_thread, &listen_read);
    pthread_join(p_write, NULL);
    pthread_join(p_read, NULL);

    close(listen_write);
    close(listen_read);
    free(player);
    for (j=0;j<MAX_GROUP;j++) {
        free(rank[j]);
    }
    free(rank);
}
