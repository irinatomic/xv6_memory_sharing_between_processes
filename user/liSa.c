
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main(int argc, char *argv[]){

    int fd, len, sent_start, i = 0, *command;
    uint *curr_sent;
    char *get_file_path;
    char* file_path = "../home/README";
    struct stat st;

    get_data("file_path", &get_file_path);
    for(int i = 0; i <= strlen(file_path); i++)
        *(file_path + i) = *(get_file_path + i);

    if ((fd = open(file_path, 0)) < 0){
        printf("Problem opening file: %s %d \n", file_path, fd);
        close(fd);
        exit();
    }

    if((stat(file_path, &st)) < 0){
        printf("Cannot stat file %s \n", file_path);
        close(fd);
        exit();
    }

    char buff[st.size];
    len = read(fd, buff, sizeof(buff));
    buff[len] = '\0';

    for( ; ; ){
        
        if(i >= len) break;

        i = (buff[i] == ' ')? i+1: i;                   //for space after the sign (.?!)
        get_data("curr_sent", &curr_sent);              //increment counter for curr sent
        (*curr_sent)++;

        //find start and end index of curr sent
        sent_start = i;
        while(buff[i] != '.' && buff[i] != '!' && buff[i] != '?')
            i++;
        
        //check curr sent 
        check_current_setence(buff, sent_start, i);
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

void check_current_setence(char *buff, int start, int end){

    char *cs_longest, *cs_shortest, *longest_word, *shortest_word;
    int wstart = start, wend, wlen, *len_longest, *len_shortest;
    char word[30], lw[30] = ".\0", sw[30] = ".....\0";

    for(int i = start; i <= end; i++){

        if(buff[i] == ' ' || i == end){
            wend = i-1;
            wlen = wend - wstart + 1;
            memset(word, 0, wlen);
            strncpy(word, buff+wstart, wlen);  
            word[wlen] = '\0';

            if(wlen > strlen(lw))
                strncpy(lw, word, wlen+1);
            else if(wlen < strlen(sw))
                strncpy(sw, word, wlen+1);

            wstart = i+1;
        }
    }

    //check curr sent values
    get_data("cs_longest", &cs_longest);
    get_data("cs_shortest", &cs_shortest);

    for(int i = 0; i <= strlen(sw); i++)
        *(cs_shortest+i) = *(sw+i);
    for(int i = 0; i <= strlen(lw); i++)
        *(cs_longest+i) = *(lw+i);

    //check global values
    get_data("longest_word", &longest_word);
    get_data("shortest_word", &shortest_word);
    get_data("len_longest", &len_longest);
    get_data("len_shortest", &len_shortest);

    if(strlen(lw) > *len_longest){
        *len_longest = strlen(lw);
        for(int i = 0; i <= strlen(lw); i++)
            *(longest_word+i) = *(lw+i);
    }

    if(strlen(sw) < *len_shortest){
        *len_shortest = strlen(sw);
        for(int i = 0; i <= strlen(sw); i++)
            *(shortest_word+i) = *(sw+i);
    }

    //printf("cs: %s %s global: %s %s\n", cs_longest, cs_shortest, longest_word, shortest_word);
}