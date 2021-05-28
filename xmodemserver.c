#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/signal.h>

#include "xmodemserver.h"
#include "crc16.h"
#include "helper.h"




static int listenfd;

int howmany = 0;

struct client *top = NULL;

/* use function blindandlisten in muffinman*/
void bindandlisten(){
    struct sockaddr_in r;
    int yes = 1;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    exit(1);
    }
    //
    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;//谁都可以访问
    r.sin_port = htons(PORT);
    
    if((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
    perror("bind");
    exit(1);
    }

    if (listen(listenfd, 5)) {
    perror("listen");
    exit(1);
    }
}

/* use function newconnection in muffinman*/
void newconnection()  /* accept connection, sing to them, get response, update
                       * linked list */
{
    int fd;
    struct sockaddr_in r;
    socklen_t socklen = sizeof r;
    if ((fd = accept(listenfd, (struct sockaddr *)&r, &socklen)) < 0) {
    perror("accept");
    } else {
    printf("connection from %s\n", inet_ntoa(r.sin_addr));
    addclient(fd);

    }
}
/* use function add_client in muffinman*/
static void addclient(int fd)
{
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
    fprintf(stderr, "out of memory!\n");  /* highly unlikely to happen */
    exit(1);
    }
    fflush(stdout);//刷新stdout
    // set up client
    p->state = initial;
    p->next = top;//往前加
    p->fd = fd;
    p->buf[0] = '\0';
    p->filename[0] ='\0';
    p->inbuf = 0;
    p->blocksize = 0;
    p->current_block = 0;
    p->next = NULL;
    p->fp =NULL;
    
    top = p;
    howmany++;
}

/* use function removeclient in muffinman*/
static void removeclient(int fd)
{
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
    ;
    
    if (*p) {
    struct client *t = (*p)->next;
    fflush(stdout);
        
    close((*p)->fd);
    free(*p);
    *p = t;
    howmany--;
    } else {
    fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", fd);
    fflush(stderr);
    }
}


int main(int argc, char const *argv[]){
    
    int maxfd;//max
    int count;
    struct client* client_c;//p

    bindandlisten();

    fd_set fds;
    

    
    while(1){
        maxfd = listenfd;
        
        FD_ZERO(&fds);
        
        FD_SET(listenfd, &fds);
        
        client_c= top;
        
        while (client_c!= NULL) {
            FD_SET(client_c->fd, &fds);//放入

            if (client_c-> fd > maxfd) {
                maxfd = client_c->fd;
            }
            client_c = client_c->next;
        }
        
        printf("find max_fd:%d/n", maxfd);


        //sleep(1);
        count =select(maxfd + 1, &fds, NULL, NULL, NULL);
        if (count< 0) {
            perror("select fail");
        }
        //else {
        if(count >=0){
            
            client_c = top;
            
            if (FD_ISSET(listenfd, &fds)){//lisen
                newconnection();
            }

        
            while (client_c!= NULL) {
                
                if (FD_ISSET(client_c->fd, &fds)) {

                    server_operation(client_c);//main function
                    printf("-\n\n");
                    
                }
                client_c = client_c->next;
            }
        }
    }
    
    return 0;
    
}

void server_operation(struct client* p){
    
    char o_state;
    // in this state, the server is waiting for a filename from the client. (Immediately here you're required to use select, because a client could take a long time to send the filename, during which another faster client could connect!) Once the server receives a complete filename, it should open that file for writing, then send a C to the client and transition to pre_block. The first block that the server expects is block rw_check_countber 1.
    if (p->state == initial){
        printf("now in initial\n");
        int rw_check_count;
        char name_f[20];
        int name_l = 0;
        //int flag =0;
        rw_check_count = read(p->fd, name_f, sizeof(p->filename));
        
        //rw_check_count = read(p->fd, &(p->filename[inbuf]), sizeof(p->filename)-inbuf);
        //inbuf+=rw_check_count;
        
        if (rw_check_count<0) {
            perror("read failed");

            removeclient(p->fd);
        }
        
        strcat(p->buf,name_f);//
        
        int max_l = (strlen(p->buf)-1);
        
        //client drop
        while (name_l<max_l) {
            if (p->buf[name_l] == '\r' && p->buf[name_l+1] == '\n') {
                p->buf[name_l] = '\0';
                    
                strcat(p->filename,p->buf);
                //create file
                //Once the server receives a complete filename, it should open that file for writing, then send a C to the client and transition to pre_block.
                printf("accurate filenmae:%s\n", p->buf);
                p->fp = open_file_in_dir(p->filename,"filestore");
            
                emptybuf(p);
                
                p->state = pre_block;
                
                //inbuf = 0;//
                // send c here
                if(write(p->fd,"C",1)!=1){
                    perror("write");
                    removeclient(p->fd);
                    }
                return;
                    
                }
                
                
            name_l++;
            }
            

        return;
        
    }
    
    if(p->state == pre_block){
        printf("now in prebock\n");
        int rw_check_count;
        rw_check_count = read(p->fd, &o_state, 1);
        if (rw_check_count<0) {
            removeclient(p->fd);
            perror("read fail");
           
        }
        
        //If the server gets an EOT, there is nothing more to receive.
        //Send ACK, drop the client, and transition to finish.
        
        if (o_state == EOT) {//文件读完
            p->state = finished;
            o_state = ACK;
            rw_check_count =write(p->fd,&o_state,1);
            if(rw_check_count !=1){//send ACK
                perror("write fail");
                //drop the client
                removeclient(p->fd);
            }
        }
                
        //server gets stx, client send 1024 payload
        if(o_state ==STX){
            p->state = get_block;
            p->blocksize =1024;
            return;
        }
        //server gets soh, client send 128 payload
        else if(o_state == SOH){
            p->state = get_block;
            p->blocksize =128;
            return;
        }
        else{
            removeclient(p->fd);
            perror("preblock fail");
            
        }
        
    }
    //get_block: the server now wants to read 132 bytes (SOH) or 1028 bytes (STX).
    //transition to checkblock
    
    if(p->state == get_block){
        
        int flag = 0;
        
        int rw_check_count;
        
        int buf_index = p->inbuf;
        int new_size = p->blocksize + 4;
        
        printf("now in get block\n");
        rw_check_count = read(p->fd, &(p->buf[buf_index]), new_size);
        
        if (rw_check_count<0) {
            perror("read fail");
            flag =1;
            removeclient(p->fd);
        }else{
            buf_index += rw_check_count;}
        
        
       if(buf_index== new_size){
            buf_index = 0;
            p->state = check_block;
                
        }
        
        if (buf_index > new_size) {
            flag =1;
            removeclient(p->fd);
            perror("block error");
         
        }
        
    }
    
    /*check_block: you now have the block number, inverse, packet, and CRC16. There's a bunch of error-checking and other conditions here:.*/
    
    if(p->state == check_block){
        
        int rw_check_count;
        
        int b_i = 0;
        
        int flag = 0;
        
        int b_size = p->blocksize;
        
        printf("now in get check block\n");
        
        unsigned char char_block = p->buf[0];
        
        unsigned char inverse = p->buf[1];
        
        // block number and inverse do not correspond
        if (char_block + inverse !=255) {
            
            perror("block inverse");
            fflush(stderr);
            flag = 1;
            //removeclient(p->fd);
        }
        
        
        unsigned char buf[b_size];
        
        while (b_i< b_size) {
            buf[b_i] = p->buf[b_i+2];
            b_i++;
        }
        
        if (flag!=0) {
            removeclient(p->fd);
        }
        
        
        unsigned short crc = crc_message(XMODEM_KEY, buf, b_size);
        
        
        // If the CRC16 is incorrect, send a NAK.
        unsigned char high_byte = p->buf[b_size+2];
        unsigned char low_byte = p->buf[b_size+3];
        
        if((unsigned char)(crc >> 8)!=high_byte){

            printf("send message again");
            
            o_state = NAK;
            
            rw_check_count =write(p->fd,&o_state,1);
            if (rw_check_count!=1) {
                perror("wirte fail");

            }
            
            p->state = pre_block;
            
            emptybuf(p);
            return;
            
        }else if((unsigned char)crc != low_byte){
            printf("send message again");
            
            o_state = NAK;
            
            rw_check_count =write(p->fd,&o_state,1);
            if (rw_check_count!=1) {
                perror("wirte fail");

            }
            
            p->state = pre_block;
            
            emptybuf(p);
            return;
        }
        
        //If the block number is not the one that is expected to be next,
        //then you have a serious error, so drop the client and abort this file
        if (char_block != p->current_block +1) {
            perror("block fail");
            fflush(stderr);
            flag = 1;
        }
        
        //If the block number is the same as the block number of the previous block
        
        if (char_block == p->current_block) {
            o_state = ACK;
            rw_check_count = write(p->fd,&o_state,1);
            
            if (rw_check_count != 1) {
                perror("wirte fail");
            }
            
            p->state = pre_block;
            emptybuf(p);
            return;
            
        }
        
        //Write the block to the file and increment the block number (watch for the wrap at 255),
        //and send an ACK to the client. Move to pre_block
        
        fwrite(buf,1,b_size,p->fp);
        int find_num = (p->current_block+1) % 255;
        p->current_block = find_num;
        
        o_state = ACK;
        rw_check_count = write(p->fd,&o_state,1);
        
        if (rw_check_count != 1) {
            perror("wirte");
            flag = 1;
            //removeclient(p->fd);
            
        }
        
        if (flag!=0) {
            removeclient(p->fd);
        }
        
        p->state = pre_block;
        emptybuf(p);
        return;
    }
    
    if(p->state == finished){
        printf("now in finished\n");
        fclose(p->fp);
        removeclient(p->fd);
        
    }
    
}

