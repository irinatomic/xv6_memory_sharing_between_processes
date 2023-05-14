#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

char *argv_comma[] = { "comma", 0 };
char *argv_lisa[] = { "lisa", 0 };

int main(int argc, char *argv[]){

    char *file_path = "../home/README";
    int curr_sent_no = 0;
    char *longest_word_curr_sent = "aaaa\0";
    char *shortest_word_curr_sent = "\0";
    int len_longest_word = -1;
    int len_shortest_word = __INT_MAX__;
    char *longest_word = "olll\0";
    char *shortest_word = "\0";
    int command = 4;

    // check if different file path is given
    if(argc > 1){
        file_path = argv[1];
        //*(file_path + strlen(file_path)) = '\0';
    }

    share_data("file_path", file_path, 20);
    share_data("curr_sent", &curr_sent_no, 4);
    share_data("cs_longest", longest_word_curr_sent, 20);
    share_data("cs_shortest", shortest_word_curr_sent, 20);
    share_data("len_longest", &len_longest_word, 4);
    share_data("len_shortest", &len_shortest_word, 4);
    share_data("longest_word", longest_word, 20);
    share_data("shortest_word", shortest_word, 20);
    share_data("command", &command, 4);

    int child_comma, child_lisa;

    child_lisa = fork();
    if(child_lisa == 0){
        exec("/bin/liSa", argv_lisa);
        //exit();
    }

    child_comma = fork();
    if(child_comma == 0){
        exec("/bin/coMMa", argv_comma);
        exit();
    }

    int p1 = wait();
    int p2 = wait();

    //printf("%d %d %d %d \n", child_comma, child_lisa, p1, p2);

    // free memory
    free(file_path);
    free(longest_word_curr_sent);
    free(shortest_word_curr_sent);
    free(longest_word);
    free(shortest_word);

    exit();
}