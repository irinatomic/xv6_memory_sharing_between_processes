#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

char* display_text = 
"Commands: \n\
0 => latest\n\
1 => global extrema\n\
2 => pause\n\
3 => resume\n\
4 => end\n";

int main(int argc, char *argv[]){

    int command;
    char buff[20];

    printf("started comma\n");

    for( ; ; ){

        printf("%s", display_text);
        memset(buff, 0, 20);
		gets(buff, 20);
		command = atoi(buff);

        switch(command){
            case 0:
                uint *curr_sent;
                // char *longest_word, shortest_word;
                get_data("curr_sent", &curr_sent);
                printf("curr_sent %d val %d\n", curr_sent, *curr_sent);
                // uint *addr = curr_sent;
                // int *ptr = (int*)addr;
                // int val = *ptr;
                

                // int *spec = 12220;
                // printf("SPECIAL %d \n", *spec);
                // printf("cs val %d 2nd layer val %d %d\n", curr_sent, *curr_sent, *((uint*)curr_sent));
                // printf("addre %d ptr %d val %d \n", *addr, *ptr, val);
                // get_data("cs_longest", &longest_word);
                // get_data("cs_shortest", &shortest_word);
                // printf("Latest sentence %d: Local extrema => longest: %s :: shortest: %s\n", 
                //         *((uint*)curr_sent), *((char*)longest_word), *((char*)shortest_word));
				break;
        }
    }

    exit();
}