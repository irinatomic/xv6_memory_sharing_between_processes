
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main(int argc, char *argv[]){

    int fd, n, sent_start, i = 0, *command;
    uint *curr_sent;
    char* file_path = "../home/README";
    struct stat st;

    //get_data("file_path", &file_path);
    if ((fd = open(file_path, 0)) < 0){
        printf("Problem opening file: %s \n", file_path);
        close(fd);
        exit();
    }

    if((stat(file_path, &st)) < 0){
        printf("Cannot stat file %s \n", file_path);
        close(fd);
        exit();
    }

    char buff[st.size];
    n = read(fd, buff, sizeof(buff));
    buff[n] = '\0';

    find_global_values(buff, n);
    exit();

    for( ; ; ){
        
        if(i >= n) break;

        i = (buff[i] == ' ')? i+1: i;                   //for space after the sign (.?!)
        get_data("curr_sent", &curr_sent);              //increment counter for curr sent
        (*curr_sent)++;
        printf("%d %d %d\n", *curr_sent, i, n);         //DEBUG

        //find start and end index of curr sent
        sent_start = i;
        while(buff[i] != '.' && buff[i] != '!' && buff[i] != '?')
            i++;
        
        //check curr sent 
        //check_current_setence(buff, sent_start, i);
        i++;                                            //move i from '.?!' to next sentence

        //check user command from coMMa
        get_data("command", &command);
        if(*command == 3){                              //pause
            while(1){
                sleep(1);
                get_data("command", &command);
                if(*command == 4)                       //resume
                    break;
                if(*command == 5){                      //end
                    close(fd);
                    exit();
                }
            }
        } else if(*command == 5){                       //end
            close(fd);
            exit();
        }

        sleep(150);
    }

    close(fd);
    exit();
}

void find_global_values(char* buff, int len){

    char *longest_word, *shortest_word;
    int *len_longest, *len_shortest;
    int wstart = 0, wend, wlen;

    get_data("longest_word", &longest_word);
    get_data("shortest_word", &shortest_word);
    get_data("len_longest", &len_longest);
    get_data("len_shortest", &len_shortest);

    char *cs_longest, *cs_shortest;
    get_data("cs_longest", &cs_longest);
    get_data("cs_shortest", &cs_shortest);

    for(int i = 0; i <= len; i++){

        if(buff[i] == ' ' || i == len){
            wend = i-1;
            wlen = wend - wstart + 1;
            char *word = (char *) malloc(10 * sizeof(char));
            strncpy(word, buff+wstart, wlen);  
            word[wlen] = '\0';

            //check for max
            if(wlen > *len_longest){
                *len_longest = wlen;              
                for(int j = 0; j < wlen; j++)
                    *(longest_word+j) = *(word+j);
                *(longest_word + wlen) = '\0';
                printf("LONG %s %s %s\n", word, longest_word, shortest_word);
            } 
            
            //check for min
            else if(wlen < *len_shortest){
                *len_shortest = wlen;
                for(int j = 0; j < wlen; j++)
                    *(shortest_word+j) = *(word+j);
                *(shortest_word + wlen) = '\0';
                printf("SHORT %s %s \n", word, shortest_word);
            }

            wstart = i+1;
        }
    }

    printf("global: %s %s \n", longest_word, shortest_word);
    printf("global: %s %s \n", cs_longest, cs_shortest);
}

void check_current_setence(char *buff, int start, int end){

    int wstart = start, wend, wlen;
    char *cs_longest, *cs_shortest;
    get_data("cs_longest", &cs_longest);
    get_data("cs_shortest", &cs_shortest);

    //reset values
    *(cs_longest+1) = "\0";
    *(cs_shortest+19) = "\0";

    printf("pre %s %s \n", cs_longest, cs_shortest);

    for(int i = start; i <= end; i++){

        if(buff[i] == ' ' || i == end){
            wend = i-1;
            wlen = wend - wstart + 1;
            char *word = (char *) malloc(10 * sizeof(char));
            strncpy(word, buff+wstart, wlen);  
            word[wlen] = '\0';

            //checks for max
            if(wlen > strlen(cs_longest)){
                for(int j = 0; j < wlen; j++)
                    *(cs_longest+j) = *(word+j);
                *(cs_longest + wlen) = '\0';
            }

            //check for min
            else if(wlen < strlen(cs_shortest)){
                for(int j = 0; j < wlen; j++)
                    *(cs_shortest+j) = *(word+j);
                *(cs_shortest + wlen) = '\0';
            }

            wstart = i+1;
        }
    }

    printf("cs: %s %s \n", cs_longest, cs_shortest);
}