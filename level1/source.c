#include <stdio.h>
#include <string.h>

int main(void)
{
    char input[100];
    
    printf("Please enter key: ");
    scanf("%s", input);
    
    if (strcmp(input, "__stack_check") == 0)
        printf("Good job.\n");
    else
        printf("Nope.\n");
    
    return 0;
}