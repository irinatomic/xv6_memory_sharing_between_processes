
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main(int argc, char *argv[]){

    printf("started lisa\n");

    int fd, n, sent_start, i = 0, *command;
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

    for( ; ; ){

        sent_start = i;
        while(buff[i] != '.' && buff[i] != '!' && buff[i] != '?')
            i++;
        
        check_current_setence(buff, sent_start, i);
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
        }

        if(*command == 5){                              //end
            close(fd);
            exit();
        }
    }

    close(fd);
    exit();
}

void check_current_setence(char *buff, int start, int end){

}