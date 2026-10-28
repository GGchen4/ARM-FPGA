#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

int main() {
    std::ifstream file("D:\\Young\\mpiigaze_resnet\\weight\\conv_weight.txt");
    std::vector<float> conv_weights;
    std::string line;

    if (!file.is_open()) {
        std::cerr << "无法打开文件！" << std::endl;
        return -1;
    }

    while (std::getline(file, line)) {
        // 跳过包含非数字的行（例如标题行）
        if (line.find_first_of("0123456789") == std::string::npos) {
            continue;
        }

        std::stringstream ss(line);
        std::string value;

        while (std::getline(ss, value, ',')) {
            // 去掉两边空格，避免 " 0.123 " 报错
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);

            if (!value.empty()) {
                try {
                    conv_weights.push_back(std::stof(value));
                } catch (const std::exception &e) {
                    std::cerr << "转换失败：" << value << " -> " << e.what() << std::endl;
                }
            }
        }
    }

    file.close();

    std::ifstream file1("D:\\Young\\mpiigaze_resnet\\weight\\stage1_block1_bn1_weight.txt");
    std::vector<float> bn_weights;
    // std::string line;

    if (!file1.is_open()) {
        std::cerr << "无法打开文件！" << std::endl;
        return -1;
    }

    while (std::getline(file1, line)) {
        // 跳过包含非数字的行（例如标题行）
        // if (line.find_first_of("0123456789") == std::string::npos) {
        //     continue;
        // }

        std::stringstream ss(line);
        std::string value;

        while (std::getline(ss, value, ',')) {
            // 去掉两边空格，避免 " 0.123 " 报错
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);

            if (!value.empty()) {
                try {
                    bn_weights.push_back(std::stof(value));
                } catch (const std::exception &e) {
                    std::cerr << "转换失败：" << value << " -> " << e.what() << std::endl;
                }
            }
        }
    }

    file.close();
    // 打印读取到的权重
    for (size_t i = 0; i < 20; ++i) {
        std::cout << "conv_weights[" << i << "] = " << conv_weights[i] << std::endl;
    }
    for (size_t i = 0; i < 20; ++i) {
        std::cout << "bn_weights[" << i << "] = " << bn_weights[i] << std::endl;
    }

    return 0;
}

