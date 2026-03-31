#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void no(void) {
	printf("Nope.\n");
	exit(1);
}

int main(void) {
	char input[24];
	char key[] = "delabere";
	int input_len;
	char pass[24];
	char tmp[4];
	int num;
	int j;
	
	printf("Please enter key: ");
	scanf("%23s", input);
	input_len = strlen(input);
	
	if (input[0] != '0' || input[1] != '0') {
		no();
	}
	
	// Initialize first character of pass
	pass[0] = 'd';
	j = 1;
	
	// Build the pass string
	for (int i = 2; i < input_len; i += 3) {
		tmp[0] = input[i];
		tmp[1] = input[i + 1];
		tmp[2] = input[i + 2];
		tmp[3] = '\0';
		
		num = atoi(tmp);
		pass[j] = (char)num;
		j++;
	}
	
	// Null terminate pass
	pass[j] = '\0';
	
	// Compare the strings
	if (strcmp(pass, key) == 0) {
		printf("Good job.\n");
		return 0;
	} else {
		no();
	}
	
	return 1;
}