#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void)
{
    char input[31];
    char pass[9];
    char tmp[4];
    size_t i;
    int j;

    printf("Please enter key: ");
    if (scanf("%23s", input) != 1) {
        puts("Nope.");
        exit(1);
    }
    if (input[1] != '2') { puts("Nope."); exit(1); }
    if (input[0] != '4') { puts("Nope."); exit(1); }
    fflush(stdin);

    memset(pass, 0, 9);
    pass[0] = '*';
    i = 2;
    j = 1;
    while (strlen(pass) < 8 && i < strlen(input)) {
        tmp[0] = input[i];
        tmp[1] = input[i + 1];
        tmp[2] = input[i + 2];
        tmp[3] = '\0';
        pass[j] = (char)atoi(tmp);
        i += 3;
        j += 1;
    }
    pass[j] = '\0';

    if (strcmp(pass, "********") == 0)
        puts("Good job.");
    else
        puts("Nope.");
    return 0;
}
