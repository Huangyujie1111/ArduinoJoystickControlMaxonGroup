#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

#define INITIALVALUE 0
#define DELAY_TIME 100  // 延时100ms

#include <Definitions.h>

char deviceName[] = "EPOS4";
char protocolStackName[] = "MAXON SERIAL V2";
char interfaceName[] = "USB";
char portName1[] = "USB0";
char portName2[] = "USB1";
char portName3[] = "USB2";
char portName4[] = "USB3";

WORD motorType = MT_DC_MOTOR;
WORD nodeId1 = 1;
WORD nodeId2 = 1;
WORD nodeId3 = 1;
WORD nodeId4 = 1;

char PPM = OMD_PROFILE_POSITION_MODE;
char PVM = OMD_PROFILE_VELOCITY_MODE;

BOOL status1 = INITIALVALUE;
BOOL status2 = INITIALVALUE;
BOOL status3 = INITIALVALUE;
BOOL status4 = INITIALVALUE;

long Motor1Position = 0;
long Motor2Position = 0;
long Motor3Position = 0;
long Motor4Position = 0;

DWORD ProfileVelocity = 1000; //速度，可根据需要调节
DWORD ProfileAcceleration = 30000;
DWORD ProfileDeceleration = 30000;

//全局变量，存储获得的模拟量的数值
int joystick_x1, joystick_y1, joystick_z1, joystick_x2, joystick_y2, joystick_z2;

HANDLE openSerialPort(const std::wstring& portName) {
    HANDLE hSerial = CreateFile(portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Unable to open COM port." << std::endl;
        return NULL;
    }
    return hSerial;
}

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
    return SetCommState(hSerial, &dcbSerialParams);
}

bool setSerialTimeouts(HANDLE hSerial, DWORD readIntervalTimeout = 1, DWORD readTotalTimeoutConstant = 1, DWORD readTotalTimeoutMultiplier = 1) {
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = readIntervalTimeout;
    timeouts.ReadTotalTimeoutConstant = readTotalTimeoutConstant;
    timeouts.ReadTotalTimeoutMultiplier = readTotalTimeoutMultiplier;
    return SetCommTimeouts(hSerial, &timeouts);
}

bool parseDataPacket(const std::string& data, std::vector<int>& values) {
    if (data.front() != '<' || data.back() != '>') return false;
    std::string payload = data.substr(1, data.size() - 2);
    std::stringstream ss(payload);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try {
            values.push_back(std::stoi(item));
        }
        catch (...) {
            return false;
        }
    }
    return values.size() == 6;
}

bool processSerialData(HANDLE hSerial) {
    char buffer[64];
    DWORD bytesRead = 0;
    std::string dataBuffer;
    if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            dataBuffer += std::string(buffer);
            size_t start = dataBuffer.find('<');
            size_t end = dataBuffer.find('>');
            if (start != std::string::npos && end != std::string::npos && end > start) {
                std::string packet = dataBuffer.substr(start, end - start + 1);
                dataBuffer.erase(0, end + 1);
                std::vector<int> joystickValues;
                if (parseDataPacket(packet, joystickValues)) {
                    joystick_x1 = joystickValues[0];
                    joystick_y1 = joystickValues[1];
                    joystick_z1 = joystickValues[2];
                    joystick_x2 = joystickValues[3];
                    joystick_y2 = joystickValues[4];
                    joystick_z2 = joystickValues[5];
                    return true;
                }
            }
        }
    }
    else {
        std::cerr << "Error: Could not read from COM port." << std::endl;
    }
    return false;
}

void closeSerialPort(HANDLE hSerial) {
    CloseHandle(hSerial);
}

void configureAndRunMotors(HANDLE keyHandle1, HANDLE keyHandle2, HANDLE keyHandle3, HANDLE keyHandle4, DWORD& errorCode) {
    //设置设备模式
    status1 = VCS_SetMotorType(keyHandle1, nodeId1, motorType, &errorCode);
    status2 = VCS_SetMotorType(keyHandle2, nodeId2, motorType, &errorCode);
    status3 = VCS_SetMotorType(keyHandle3, nodeId3, motorType, &errorCode);
    status4 = VCS_SetMotorType(keyHandle4, nodeId4, motorType, &errorCode);

    //设置运动模式
    status1 = VCS_SetOperationMode(keyHandle1, nodeId1, PVM, &errorCode);
    status2 = VCS_SetOperationMode(keyHandle2, nodeId2, PVM, &errorCode);
    status3 = VCS_SetOperationMode(keyHandle3, nodeId3, PVM, &errorCode);
    status4 = VCS_SetOperationMode(keyHandle4, nodeId4, PVM, &errorCode);

    //设置PVM参数
    status1 = VCS_SetVelocityProfile(keyHandle1, nodeId1, ProfileAcceleration, ProfileDeceleration, &errorCode);
    status2 = VCS_SetVelocityProfile(keyHandle2, nodeId2, ProfileAcceleration, ProfileDeceleration, &errorCode);
    status3 = VCS_SetVelocityProfile(keyHandle3, nodeId3, ProfileAcceleration, ProfileDeceleration, &errorCode);
    status4 = VCS_SetVelocityProfile(keyHandle4, nodeId4, ProfileAcceleration, ProfileDeceleration, &errorCode);

    //启动PVM
    status1 = VCS_ActivateProfileVelocityMode(keyHandle1, nodeId1, &errorCode);
    status2 = VCS_ActivateProfileVelocityMode(keyHandle2, nodeId2, &errorCode);
    status3 = VCS_ActivateProfileVelocityMode(keyHandle3, nodeId3, &errorCode);
    status4 = VCS_ActivateProfileVelocityMode(keyHandle4, nodeId4, &errorCode);

    //电机使能
    status1 = VCS_SetEnableState(keyHandle1, nodeId1, &errorCode);
    status2 = VCS_SetEnableState(keyHandle2, nodeId2, &errorCode);
    status3 = VCS_SetEnableState(keyHandle3, nodeId3, &errorCode);
    status4 = VCS_SetEnableState(keyHandle4, nodeId4, &errorCode);

    cout << "电机配置成功" << endl;
}

int main() {
    DWORD errorCode = 0;
    HANDLE keyHandle1 = NULL;
    HANDLE keyHandle2 = NULL;
    HANDLE keyHandle3 = NULL;
    HANDLE keyHandle4 = NULL;

    HANDLE hSerial = openSerialPort(L"COM7");
    if (hSerial == NULL) return 1;
    if (!configureSerialPort(hSerial)) {
        closeSerialPort(hSerial);
        return 1;
    }
    if (!setSerialTimeouts(hSerial)) {
        closeSerialPort(hSerial);
        return 1;
    }

    // 添加主循环延时，避免CPU过度占用
    Sleep(DELAY_TIME);

    while (true) {
        if (processSerialData(hSerial)) {
            std::cout << "X1: " << joystick_x1 << ", Y1: " << joystick_y1 << ", Z1: " << joystick_z1
                << ", X2: " << joystick_x2 << ", Y2: " << joystick_y2 << ", Z2: " << joystick_z2 << std::endl;

            if ((joystick_z1 == 1) && (joystick_z2 == 1)) {
                cout << "退出运行，关闭串口" << endl;
                closeSerialPort(hSerial);
                break;
            }

            if (joystick_z2 == 1) {
                std::cout << "Z2 按下，开始配置电机..." << std::endl;

                //打开设备
                keyHandle1 = VCS_OpenDevice(deviceName, protocolStackName, interfaceName, portName1, &errorCode);
                keyHandle2 = VCS_OpenDevice(deviceName, protocolStackName, interfaceName, portName2, &errorCode);
                keyHandle3 = VCS_OpenDevice(deviceName, protocolStackName, interfaceName, portName3, &errorCode);
                keyHandle4 = VCS_OpenDevice(deviceName, protocolStackName, interfaceName, portName4, &errorCode);

                if (keyHandle1 == NULL || keyHandle2 == NULL || keyHandle3 == NULL || keyHandle4 == NULL) {
                    std::cerr << "Error: Failed to open device. Error code: " << errorCode << std::endl;
                    break;
                }

                configureAndRunMotors(keyHandle1, keyHandle2, keyHandle3, keyHandle4, errorCode);

                while (true) {
                    if (processSerialData(hSerial)) {
                        // 电机1控制
                        if (joystick_x1 > 900) {
                            status1 = VCS_MoveWithVelocity(keyHandle1, nodeId1, ProfileVelocity, &errorCode);
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            std::cout << "电机1正转，速度: " << ProfileVelocity << "。 当前位置为： " << Motor1Position << std::endl;
                        }
                        if (joystick_x1 < 100) {
                            status1 = VCS_MoveWithVelocity(keyHandle1, nodeId1, -1 * ProfileVelocity, &errorCode);
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            std::cout << "电机1反转，速度：" << -1 * ProfileVelocity << "。 当前位置为： " << Motor1Position << std::endl;
                        }
                        if((joystick_x1 >= 100) && (joystick_x1 <= 900)) {
                            status1 = VCS_MoveWithVelocity(keyHandle1, nodeId1, 0, &errorCode);
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            std::cout << "电机1停止。 " << "当前位置为： " << Motor1Position << std::endl;
                        }

                        // 电机2控制
                        if (joystick_y1 > 900) {
                            status2 = VCS_MoveWithVelocity(keyHandle2, nodeId2, ProfileVelocity, &errorCode);
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            std::cout << "电机2正转，速度:  " << ProfileVelocity << "。 当前位置为： " << Motor2Position << std::endl;
                        }
                        if (joystick_y1 < 100) {
                            status2 = VCS_MoveWithVelocity(keyHandle2, nodeId2, -1 * ProfileVelocity, &errorCode);
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            std::cout << "电机2反转，速度： " << ProfileVelocity << "。 当前位置为： " << Motor2Position << std::endl;
                        }
                        if((joystick_y1 >= 100) && (joystick_y1 <= 900)) {
                            status2 = VCS_MoveWithVelocity(keyHandle2, nodeId2, 0, &errorCode);
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            std::cout << "电机2停止。 " << "当前位置为： " << Motor2Position << std::endl;
                        }

                        // 电机3控制
                        if (joystick_x2 > 900) {
                            status3 = VCS_MoveWithVelocity(keyHandle3, nodeId3, ProfileVelocity, &errorCode);
                            status3 = VCS_GetPositionIs(keyHandle3, nodeId3, &Motor3Position, &errorCode);
                            std::cout << "电机3正转，速度:  " << ProfileVelocity << "。 当前位置为： " << Motor3Position << std::endl;
                        }
                        if (joystick_x2 < 100) {
                            status3 = VCS_MoveWithVelocity(keyHandle3, nodeId3, -1 * ProfileVelocity, &errorCode);
                            status3 = VCS_GetPositionIs(keyHandle3, nodeId3, &Motor3Position, &errorCode);
                            std::cout << "电机3反转，速度:  " << ProfileVelocity << "。 当前位置为： " << Motor3Position << std::endl;
                        }
                        if((joystick_x2 >= 100) && (joystick_x2 <= 900)) {
                            status3 = VCS_MoveWithVelocity(keyHandle3, nodeId3, 0, &errorCode);
                            status3 = VCS_GetPositionIs(keyHandle3, nodeId3, &Motor3Position, &errorCode);
                            std::cout << "电机3停止。 " << "当前位置为： " << Motor3Position << std::endl;
                        }

                        // 电机4控制
                        if (joystick_y2 > 900) {
                            status4 = VCS_MoveWithVelocity(keyHandle4, nodeId4, ProfileVelocity, &errorCode);
                            status4 = VCS_GetPositionIs(keyHandle4, nodeId4, &Motor4Position, &errorCode);
                            std::cout << "电机4正转，速度:  " << ProfileVelocity << "。 当前位置为： " << Motor4Position << std::endl;
                        }
                        if (joystick_y2 < 100) {
                            status4 = VCS_MoveWithVelocity(keyHandle4, nodeId4, -1 * ProfileVelocity, &errorCode);
                            status4 = VCS_GetPositionIs(keyHandle4, nodeId4, &Motor4Position, &errorCode);
                            std::cout << "电机4反转，速度:  " << ProfileVelocity << "。 当前位置为： " << Motor4Position << std::endl;
                        }
                        if((joystick_y2 >= 100) && (joystick_y2 <= 900)) {
                            status4 = VCS_MoveWithVelocity(keyHandle4, nodeId4, 0, &errorCode);
                            status4 = VCS_GetPositionIs(keyHandle4, nodeId4, &Motor4Position, &errorCode);
                            std::cout << "电机4停止。 " << "当前位置为： " << Motor4Position << std::endl;
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

                            status3 = VCS_SetOperationMode(keyHandle3, nodeId3, PPM, &errorCode);  // 设置操作模式
                            status3 = VCS_SetPositionProfile(keyHandle3, nodeId3, ProfileVelocity, ProfileAcceleration, ProfileDeceleration, &errorCode);
                            status3 = VCS_GetPositionIs(keyHandle3, nodeId3, &Motor3Position, &errorCode);
                            while ((Motor3Position < -200) || (Motor3Position > 200)) {
                                status3 = VCS_MoveToPosition(keyHandle3, nodeId3, 0, 1, true, &errorCode);  // 复位到初始位置
                                status3 = VCS_GetPositionIs(keyHandle3, nodeId3, &Motor3Position, &errorCode);
                                std::cout << "当前电机3的位置为：" << Motor3Position << std::endl;
                            }

                            status4 = VCS_SetOperationMode(keyHandle4, nodeId4, PPM, &errorCode);  // 设置操作模式
                            status4 = VCS_SetPositionProfile(keyHandle4, nodeId4, ProfileVelocity, ProfileAcceleration, ProfileDeceleration, &errorCode);
                            status4 = VCS_GetPositionIs(keyHandle4, nodeId4, &Motor4Position, &errorCode);
                            while ((Motor4Position < -200) || (Motor4Position > 200)) {
                                status4 = VCS_MoveToPosition(keyHandle4, nodeId4, 0, 1, true, &errorCode);  // 复位到初始位置
                                status4 = VCS_GetPositionIs(keyHandle4, nodeId4, &Motor4Position, &errorCode);
                                std::cout << "当前电机4的位置为：" << Motor4Position << std::endl;
                            }

                            status1 = VCS_SetDisableState(keyHandle1, nodeId1, &errorCode);  // 禁用电机1
                            status2 = VCS_SetDisableState(keyHandle2, nodeId2, &errorCode);  // 禁用电机2
                            status3 = VCS_SetDisableState(keyHandle3, nodeId3, &errorCode);  // 禁用电机3
                            status4 = VCS_SetDisableState(keyHandle4, nodeId4, &errorCode);  // 禁用电机4

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
