#include <stdio.h>
#include <unistd.h>
#include <syscall.h>
#include "linux/prinfo.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

int test(struct prinfo * buf, int * nr);

int main(void){
    int nr;
    printf("type number of processes you want to print: ");
    scanf("%d", &nr);
    printf("test start\n");
    fflush(stdout);
    struct prinfo *k=(struct prinfo *)malloc(sizeof(struct prinfo)*nr);//mem alloc 
    int total=syscall(398, k, &nr);
    
    if(total<0){
        if(k==NULL || &nr==NULL || nr<1 ){
            errno=EINVAL;
            perror("Pointer is NULL or nr is less than 1");
        }
        else{
            errno=EFAULT;
            perror("Memory is outside of the accessible address space");
        }    
        return -1;
    }
    int *ind=(int*)malloc((nr+1)/2*sizeof(int));
    int index=-1;
    int tab=0;
    
    printf("%s,%d,%lld,%d,%d,%d,%lld\n",k->comm, k->pid, k->state, k->parent_pid, k->first_child_pid, k->next_sibling_pid, k->uid);//printing format
    if((k->first_child_pid!=0) && (k->next_sibling_pid!=0)){
        index++;
        ind[index]=0;
    }
    for(int i=1;i<nr;i++){
        if(((k+i-1)->first_child_pid==(k+i)->pid))// if child, indent ++
            tab++;
        else if((k+i-1)->next_sibling_pid!=(k+i)->pid){//else if it is not sibling of previous task
            tab=ind[index];//pop indent
            index--;
        }
        
        for(int j=0;j<tab;j++)
            printf("\t");
        
        if(((k+i)->first_child_pid!=0) && ((k+i)->next_sibling_pid!=0)){// if it has both child and sibling
            index++;    //push indent
            ind[index]=tab;
        }
            
        printf("%s,%d,%lld,%d,%d,%d,%lld\n",(k+i)->comm, (k+i)->pid, (k+i)->state, (k+i)->parent_pid, (k+i)->first_child_pid, (k+i)->next_sibling_pid, (k+i)->uid);
        
    }
    printf("total process number: %d\n", total); 
    free(ind);
    free(k);    
    return 0;

}
/*
int test(struct prinfo *buf, int * nr){
    *nr=N;
    printf("current nr is %d\n",*nr);
    for(int i=0;i<(*nr);i++){
    
        strcpy(buf[i].comm, "ok");
        buf[i].pid=i;
        buf[i].state=i*2;
        buf[i].parent_pid=i-1;
        buf[i].first_child_pid=i+1;
        if(i==2)
            buf[i].next_sibling_pid=-1;
        else
            buf[i].next_sibling_pid=i;
        buf[i].uid=300;
    }
    return 1024;
}*/
