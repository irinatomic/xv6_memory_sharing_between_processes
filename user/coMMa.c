#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

char* display_text = 
"Commands: \n\
1 => latest\n\
2 => global extrema\n\
3 => pause\n\
4 => resume\n\
5 => end\n";

int main(int argc, char *argv[]){

    int user_command, *command;
    char buff[20];

    printf("started comma\n");
    printf("%s", display_text);

    for( ; ; ){

        memset(buff, 0, 20);
		gets(buff, 20);
		user_command = atoi(buff);

        switch(user_command){
            case 1:
                uint *curr_sent;
                char *cs_longest, *cs_shortest;
                get_data("curr_sent", &curr_sent);
                get_data("cs_longest", &cs_longest);
                get_data("cs_shortest", &cs_shortest);

                printf("Latest sentence %d: Local extrema => longest: %s :: shortest: %s\n", 
                        *curr_sent, cs_longest, cs_shortest);
				break;
            case 2:
                char *longest_word, *shortest_word;
                uint *len_longest, *len_shortest;
                get_data("longest_word", &longest_word);
                get_data("shortest_word", &shortest_word);
                get_data("len_longest", &len_longest);
                get_data("len_shortest", &len_shortest);

                printf("Global extrema => longest: <%d> %s :: shortest <%d> %s \n",
                        *len_longest, longest_word, *len_shortest, shortest_word);
                break;
            case 3:
                get_data("command", &command);
                *((uint*)command) = 2;
                printf("Pausing...");
                break;
            case 4:
                get_data("command", &command);
                *((uint*)command) = 3;
                printf("Resuming...");
                break;
            case 5:
                get_data("command", &command);
                *((uint*)command) = 4;
                exit();
        }
    }

    exit();
}