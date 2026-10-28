#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void process_line(FILE *input, FILE *output) {
    char line[256];
    char *p;
    float buffer = 0.0;
    p = strtok(line, "\n\r");
    printf("%s\n", p);
    while (p != NULL) {

    	p = strtok(NULL, "[ ]\n\r");
    	if (isdigit(p[0]) || (p[0] == '-' && isdigit(p[1]))) {
    		buffer = atof(p);
    		Int_buffer = float_to_int16(buffer,16,15);
        	Xil_Out16((0x00200000+address),Int_buffer);
        	address = address + 2;
    	    printf("%d\n", Int_buffer);
    	}

//    	printf("%s\n", p);

    }
    while (fgets(line, sizeof(line), input)) {
        // Write the first line directly to output
        fprintf(output, "%s", line);
        
        // Read the next line which contains the array
        if (fgets(line, sizeof(line), input)) {
            fprintf(output, "[");
            char *token = strtok(line, " ");
            while (token != NULL) {
                // Remove potential newline characters
                if (token[strlen(token) - 1] == '\n') {
                    token[strlen(token) - 1] = '\0';
                }
                // Convert the string to a double, perform the operations and write to output
                double value = atof(token);
                value = sqrt(value + 0.00001);
                fprintf(output, "%g ", value);
                
                token = strtok(NULL, " ");
            }
            fprintf(output, "]\n");
        }
    }
}

int main() {
    FILE *input_file = fopen("C:/Users/Young/Desktop/running_var.txt", "r");
    FILE *output_file = fopen("C:/Users/Young/Desktop/output.txt", "w");

    if (input_file == NULL || output_file == NULL) {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    process_line(input_file, output_file);

    fclose(input_file);
    fclose(output_file);

    return EXIT_SUCCESS;
}
