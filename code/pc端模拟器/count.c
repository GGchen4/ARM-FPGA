#include <stdio.h>

int count_characters_in_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    int count = 0;
    int ch;

    // 逐字符读取文件，直到文件结束
    while ((ch = fgetc(file)) != EOF) {
        count++;
    }

    fclose(file);
    return count;
}

int main() {
    const char *filename = "D:/Young/mpiigaze_resnet/conv_weight.txt";  // 替换为你的文件名
    int character_count = count_characters_in_file(filename);

    if (character_count != -1) {
        printf("The file '%s' contains %d characters.\n", filename, character_count);
    }

    return 0;
}
