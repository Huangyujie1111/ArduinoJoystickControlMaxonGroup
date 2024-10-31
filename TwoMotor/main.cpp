#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

#define INITIALVALUE 0
#define DELAY_TIME 100  // 延时100ms

#include <Definitions.h>

// 设置电机和控制器的类型
char deviceName[] = "EPOS4";
char protocolStackName[] = "MAXON SERIAL V2";
char interfaceName[] = "USB";
char portName1[] = "USB0";
char portName2[] = "USB1";

WORD motorType = MT_DC_MOTOR;
WORD nodeId1 = 0;
WORD nodeId2 = 1;

char PPM = OMD_PROFILE_POSITION_MODE;//PPM位置模式
char PVM = OMD_PROFILE_VELOCITY_MODE;//PVM速度模式

BOOL status1 = INITIALVALUE;
BOOL status2 = INITIALVALUE;

long Motor1Position = 0;//电机1的位置
long Motor2Position = 0;//电机2的位置

DWORD ProfileVelocity = 1000;//电机的运行速度
DWORD ProfileAcceleration = 30000;//电机的运行加速度
DWORD ProfileDeceleration = 30000;//电机的运行减速度

// 定义摇杆获取的值，全局变量
int joystick_x1, joystick_y1, joystick_z1, joystick_x2, joystick_y2, joystick_z2;

// 打开串口函数
HANDLE openSerialPort(const std::wstring& portName) {
    HANDLE hSerial = CreateFile(portName.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Unable to open COM port." << std::endl;
        return NULL;
    }
    return hSerial;
}

// 配置串口参数
bool configureSerialPort(HANDLE hSerial, DWORD baudRate = CBR_9600, BYTE byteSize = 8, BYTE stopBits = ONESTOPBIT, BYTE parity = NOPARITY) {
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error: Could not get serial parameters." << std::endl;
        return false;
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = byteSize;
    dcbSerialParams.StopBits = stopBits;
    dcbSerialParams.Parity = parity;

    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error: Could not set serial parameters." << std::endl;
        return false;
    }
    return true;
}

// 设置超时
bool setSerialTimeouts(HANDLE hSerial, DWORD readIntervalTimeout = 1, DWORD readTotalTimeoutConstant = 1, DWORD readTotalTimeoutMultiplier = 1) {
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = readIntervalTimeout;
    timeouts.ReadTotalTimeoutConstant = readTotalTimeoutConstant;
    timeouts.ReadTotalTimeoutMultiplier = readTotalTimeoutMultiplier;

    if (!SetCommTimeouts(hSerial, &timeouts)) {
        std::cerr << "Error: Could not set serial timeouts." << std::endl;
        return false;
    }
    return true;
}

// 解析数据包
bool parseDataPacket(const std::string& data, std::vector<int>& values) {
    if (data.front() != '<' || data.back() != '>') {
        return false;  // 数据包格式不正确
    }

    std::string payload = data.substr(1, data.size() - 2);  // 去掉起始和结束标志符
    std::stringstream ss(payload);
    std::string item;

    while (std::getline(ss, item, ',')) {
        try {
            values.push_back(std::stoi(item));  // 转换为整数
        }
        catch (...) {
            return false;  // 解析失败，返回false
        }
    }
    return values.size() == 6;  // 检查是否解析了6个数据
}

// 读取串口数据
bool processSerialData(HANDLE hSerial) {
    char buffer[64];
    DWORD bytesRead = 0;
    std::string dataBuffer;  // 缓存字符串

    // 增量读取数据
    if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';  // 确保缓冲区以NULL结尾
            dataBuffer += std::string(buffer);  // 增量加入缓存

            // 查找完整的数据包
            size_t start = dataBuffer.find('<');
            size_t end = dataBuffer.find('>');

            if (start != std::string::npos && end != std::string::npos && end > start) {
                std::string packet = dataBuffer.substr(start, end - start + 1);
                dataBuffer.erase(0, end + 1);  // 移除已处理的数据包

                // 解析数据包
                std::vector<int> joystickValues;
                if (parseDataPacket(packet, joystickValues)) {
                    // 将解析得到的数值赋值给全局变量
                    joystick_x1 = joystickValues[0];
                    joystick_y1 = joystickValues[1];
                    joystick_z1 = joystickValues[2];
                    joystick_x2 = joystickValues[3];
                    joystick_y2 = joystickValues[4];
                    joystick_z2 = joystickValues[5];

                    return true;  // 成功读取一次数据
                }
            }
        }
    }
    else {
        std::cerr << "Error: Could not read from COM port." << std::endl;
    }
    return false;  // 未能成功读取数据
}

// 关闭串口
void closeSerialPort(HANDLE hSerial) {
    CloseHandle(hSerial);
}

// 配置和运行电机
void configureAndRunMotors(HANDLE keyHandle1, HANDLE keyHandle2, DWORD& errorCode) {

    // 设置设备模式
    status1 = VCS_SetMotorType(keyHandle1, nodeId1, motorType, &errorCode);
    status2 = VCS_SetMotorType(keyHandle2, nodeId2, motorType, &errorCode);

    if (!status1 || !status2) {
        std::cerr << "Error enabling motor 1 or 2. Error code: " << errorCode << std::endl;
    }

    // 设置操作模式，选择PVM
    status1 = VCS_SetOperationMode(keyHandle1, nodeId1, PVM, &errorCode);
    status2 = VCS_SetOperationMode(keyHandle2, nodeId2, PVM, &errorCode);

    // 设置PVM参数
    status1 = VCS_SetVelocityProfile(keyHandle1, nodeId1, ProfileAcceleration, ProfileDeceleration, &errorCode);
    status2 = VCS_SetVelocityProfile(keyHandle2, nodeId2, ProfileAcceleration, ProfileDeceleration, &errorCode);

    // 启动PVM
    status1 = VCS_ActivateProfileVelocityMode(keyHandle1, nodeId1, &errorCode);
    status2 = VCS_ActivateProfileVelocityMode(keyHandle2, nodeId2, &errorCode);

    // 使能电机
    status1 = VCS_SetEnableState(keyHandle1, nodeId1, &errorCode);
    status2 = VCS_SetEnableState(keyHandle2, nodeId2, &errorCode);

    cout << "电机配置成功" << endl;
}

// 主函数中循环调用processSerialData函数
int main() {
    /*对电机进行操作*/
    DWORD errorCode = 0;
    HANDLE keyHandle1 = NULL;
    HANDLE keyHandle2 = NULL;

    // 打开串口
    HANDLE hSerial = openSerialPort(L"COM7");
    if (hSerial == NULL) {
        return 1;
    }

    // 配置串口
    if (!configureSerialPort(hSerial)) {
        closeSerialPort(hSerial);
        return 1;
    }

    // 设置超时
    if (!setSerialTimeouts(hSerial)) {
        closeSerialPort(hSerial);
        return 1;
    }

    // 添加主循环延时，避免CPU过度占用
    Sleep(DELAY_TIME);

    // 循环读取串口数据，直到z2为1
    while (true) {
        if (processSerialData(hSerial)) {
            // 打印读取的数据
            std::cout << "X1: " << joystick_x1 << ", Y1: " << joystick_y1 << ", Z1: " << joystick_z1
                << ", X2: " << joystick_x2 << ", Y2: " << joystick_y2 << ", Z2: " << joystick_z2 << std::endl;

            //退出程序
            if ((joystick_z1 == 1) && (joystick_z2 == 1)) {
                cout << "退出运行，关闭串口" << endl;
                closeSerialPort(hSerial);
                break;
            }

            // 检查z2是否按下
            if (joystick_z2 == 1) {
                std::cout << "Z2 按下，开始配置电机..." << std::endl;

                // 打开设备
                keyHandle1 = VCS_OpenDevice(deviceName, protocolStackName, interfaceName, portName1, &errorCode);
                keyHandle2 = VCS_OpenDevice(deviceName, protocolStackName, interfaceName, portName2, &errorCode);

                if (keyHandle1 == NULL || keyHandle2 == NULL) {
                    std::cerr << "Error: Failed to open device. Error code: " << errorCode << std::endl;
                    break;
                }

                // 配置和运行电机
                configureAndRunMotors(keyHandle1, keyHandle2, errorCode);

                // 电机控制循环，持续读取串口数据并控制电机
                while (true) {
                    // 持续读取串口数据
                    if (processSerialData(hSerial)) {
                        // 电机正转逻辑，当 joystick_x1 大于 900 时
                        if (joystick_x1 > 900) {
                            status1 = VCS_MoveWithVelocity(keyHandle1, nodeId1, ProfileVelocity, &errorCode);  // 电机正转，速度1000
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            std::cout << "电机1正转，速度: " << ProfileVelocity << "。 当前位置为： " << Motor1Position << std::endl;
                        }
                        // 电机反转逻辑，当 joystick_x1 小于 100 时
                        if (joystick_x1 < 100) {
                            status1 = VCS_MoveWithVelocity(keyHandle1, nodeId1, -1 * ProfileVelocity, &errorCode);  // 电机反转，速度-1000
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            std::cout << "电机1反转，速度：" << -1 * ProfileVelocity << "。 当前位置为： " << Motor1Position << std::endl;
                        }
                        // 当摇杆回到中间位置 (100 <= joystick_x1 <= 900)，停止电机
                        if ((joystick_x1 >= 100) && (joystick_x1 <= 900)) {
                            status1 = VCS_MoveWithVelocity(keyHandle1, nodeId1, 0, &errorCode);  // 停止电机
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            std::cout << "电机1停止。 " << "当前位置为： " << Motor1Position << std::endl;
                        }

                        // 电机正转逻辑，当 joystick_x2 大于 900 时
                        if (joystick_x2 > 900) {
                            status2 = VCS_MoveWithVelocity(keyHandle2, nodeId2, ProfileVelocity, &errorCode);  // 电机正转，速度1000
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            std::cout << "电机2正转，速度:  " << ProfileVelocity << "。 当前位置为： " << Motor2Position << std::endl;
                        }
                        // 电机反转逻辑，当 joystick_x2 小于 100 时
                        if (joystick_x2 < 100) {
                            status2 = VCS_MoveWithVelocity(keyHandle2, nodeId2, -1 * ProfileVelocity, &errorCode);  // 电机反转，速度-1000
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            std::cout << "电机2反转，速度： " << ProfileVelocity << "。 当前位置为： " << Motor2Position << std::endl;
                        }
                        // 当摇杆回到中间位置 (100 <= joystick_x2 <= 900)，停止电机
                        if ((joystick_x2 >= 100) && (joystick_x2 <= 900)) {
                            status2 = VCS_MoveWithVelocity(keyHandle2, nodeId2, 0, &errorCode);  // 停止电机
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            std::cout << "电机2停止。 " << "当前位置为： " << Motor2Position << std::endl;
                        }

                        // 复位逻辑，当 joystick_z1 为 1 时复位电机并退出循环
                        if ((joystick_z1 == 1)) {
                            std::cout << "电机复位" << std::endl;
                            status1 = VCS_SetOperationMode(keyHandle1, nodeId1, PPM, &errorCode);  // 设置操作模式
                            status1 = VCS_SetPositionProfile(keyHandle1, nodeId1, ProfileVelocity, ProfileAcceleration, ProfileDeceleration, &errorCode);
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            while ((Motor1Position < -200) || (Motor1Position > 200)) {
                                status1 = VCS_MoveToPosition(keyHandle1, nodeId1, 0, 1, true, &errorCode);  // 复位到初始位置
                                status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                                std::cout << "当前电机1的位置为：" << Motor1Position << std::endl;
                            }

                            status2 = VCS_SetOperationMode(keyHandle2, nodeId2, PPM, &errorCode);  // 设置操作模式
                            status2 = VCS_SetPositionProfile(keyHandle2, nodeId2, ProfileVelocity, ProfileAcceleration, ProfileDeceleration, &errorCode);
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            while ((Motor2Position < -200) || (Motor2Position > 200)) {
                                status2 = VCS_MoveToPosition(keyHandle2, nodeId2, 0, 1, true, &errorCode);  // 复位到初始位置
                                status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                                std::cout << "当前电机2的位置为：" << Motor2Position << std::endl;
                            }

                            status1 = VCS_SetDisableState(keyHandle1, nodeId1, &errorCode);  // 禁用电机1
                            status2 = VCS_SetDisableState(keyHandle2, nodeId2, &errorCode);  // 禁用电机2

                            VCS_CloseAllDevices(&errorCode);

                            break;  // 退出循环，此处可以设置一个标识位。然后在复位的时候只退出内存循环。
                        }

                        // 在每次电机操作后添加适当延时
                        Sleep(DELAY_TIME);
                    }
                }
            }
        }
        // 主循环中延时，避免读取数据过于频繁
        Sleep(DELAY_TIME);
    }
    return 0;
}
