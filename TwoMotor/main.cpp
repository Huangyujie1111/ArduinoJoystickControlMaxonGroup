#include <windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

#define INITIALVALUE 0
#define DELAY_TIME 100  // ��ʱ100ms

#include <Definitions.h>

// ���õ���Ϳ�����������
char deviceName[] = "EPOS4";
char protocolStackName[] = "MAXON SERIAL V2";
char interfaceName[] = "USB";
char portName1[] = "USB0";
char portName2[] = "USB1";

WORD motorType = MT_DC_MOTOR;
WORD nodeId1 = 0;
WORD nodeId2 = 1;

char PPM = OMD_PROFILE_POSITION_MODE;//PPMλ��ģʽ
char PVM = OMD_PROFILE_VELOCITY_MODE;//PVM�ٶ�ģʽ

BOOL status1 = INITIALVALUE;
BOOL status2 = INITIALVALUE;

long Motor1Position = 0;//���1��λ��
long Motor2Position = 0;//���2��λ��

DWORD ProfileVelocity = 1000;//����������ٶ�
DWORD ProfileAcceleration = 30000;//��������м��ٶ�
DWORD ProfileDeceleration = 30000;//��������м��ٶ�

// ����ҡ�˻�ȡ��ֵ��ȫ�ֱ���
int joystick_x1, joystick_y1, joystick_z1, joystick_x2, joystick_y2, joystick_z2;

// �򿪴��ں���
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

// ���ô��ڲ���
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

// ���ó�ʱ
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

// �������ݰ�
bool parseDataPacket(const std::string& data, std::vector<int>& values) {
    if (data.front() != '<' || data.back() != '>') {
        return false;  // ���ݰ���ʽ����ȷ
    }

    std::string payload = data.substr(1, data.size() - 2);  // ȥ����ʼ�ͽ�����־��
    std::stringstream ss(payload);
    std::string item;

    while (std::getline(ss, item, ',')) {
        try {
            values.push_back(std::stoi(item));  // ת��Ϊ����
        }
        catch (...) {
            return false;  // ����ʧ�ܣ�����false
        }
    }
    return values.size() == 6;  // ����Ƿ������6������
}

// ��ȡ��������
bool processSerialData(HANDLE hSerial) {
    char buffer[64];
    DWORD bytesRead = 0;
    std::string dataBuffer;  // �����ַ���

    // ������ȡ����
    if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL)) {
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';  // ȷ����������NULL��β
            dataBuffer += std::string(buffer);  // �������뻺��

            // �������������ݰ�
            size_t start = dataBuffer.find('<');
            size_t end = dataBuffer.find('>');

            if (start != std::string::npos && end != std::string::npos && end > start) {
                std::string packet = dataBuffer.substr(start, end - start + 1);
                dataBuffer.erase(0, end + 1);  // �Ƴ��Ѵ�������ݰ�

                // �������ݰ�
                std::vector<int> joystickValues;
                if (parseDataPacket(packet, joystickValues)) {
                    // �������õ�����ֵ��ֵ��ȫ�ֱ���
                    joystick_x1 = joystickValues[0];
                    joystick_y1 = joystickValues[1];
                    joystick_z1 = joystickValues[2];
                    joystick_x2 = joystickValues[3];
                    joystick_y2 = joystickValues[4];
                    joystick_z2 = joystickValues[5];

                    return true;  // �ɹ���ȡһ������
                }
            }
        }
    }
    else {
        std::cerr << "Error: Could not read from COM port." << std::endl;
    }
    return false;  // δ�ܳɹ���ȡ����
}

// �رմ���
void closeSerialPort(HANDLE hSerial) {
    CloseHandle(hSerial);
}

// ���ú����е��
void configureAndRunMotors(HANDLE keyHandle1, HANDLE keyHandle2, DWORD& errorCode) {

    // �����豸ģʽ
    status1 = VCS_SetMotorType(keyHandle1, nodeId1, motorType, &errorCode);
    status2 = VCS_SetMotorType(keyHandle2, nodeId2, motorType, &errorCode);

    if (!status1 || !status2) {
        std::cerr << "Error enabling motor 1 or 2. Error code: " << errorCode << std::endl;
    }

    // ���ò���ģʽ��ѡ��PVM
    status1 = VCS_SetOperationMode(keyHandle1, nodeId1, PVM, &errorCode);
    status2 = VCS_SetOperationMode(keyHandle2, nodeId2, PVM, &errorCode);

    // ����PVM����
    status1 = VCS_SetVelocityProfile(keyHandle1, nodeId1, ProfileAcceleration, ProfileDeceleration, &errorCode);
    status2 = VCS_SetVelocityProfile(keyHandle2, nodeId2, ProfileAcceleration, ProfileDeceleration, &errorCode);

    // ����PVM
    status1 = VCS_ActivateProfileVelocityMode(keyHandle1, nodeId1, &errorCode);
    status2 = VCS_ActivateProfileVelocityMode(keyHandle2, nodeId2, &errorCode);

    // ʹ�ܵ��
    status1 = VCS_SetEnableState(keyHandle1, nodeId1, &errorCode);
    status2 = VCS_SetEnableState(keyHandle2, nodeId2, &errorCode);

    cout << "������óɹ�" << endl;
}

// ��������ѭ������processSerialData����
int main() {
    /*�Ե�����в���*/
    DWORD errorCode = 0;
    HANDLE keyHandle1 = NULL;
    HANDLE keyHandle2 = NULL;

    // �򿪴���
    HANDLE hSerial = openSerialPort(L"COM7");
    if (hSerial == NULL) {
        return 1;
    }

    // ���ô���
    if (!configureSerialPort(hSerial)) {
        closeSerialPort(hSerial);
        return 1;
    }

    // ���ó�ʱ
    if (!setSerialTimeouts(hSerial)) {
        closeSerialPort(hSerial);
        return 1;
    }

    // �����ѭ����ʱ������CPU����ռ��
    Sleep(DELAY_TIME);

    // ѭ����ȡ�������ݣ�ֱ��z2Ϊ1
    while (true) {
        if (processSerialData(hSerial)) {
            // ��ӡ��ȡ������
            std::cout << "X1: " << joystick_x1 << ", Y1: " << joystick_y1 << ", Z1: " << joystick_z1
                << ", X2: " << joystick_x2 << ", Y2: " << joystick_y2 << ", Z2: " << joystick_z2 << std::endl;

            //�˳�����
            if ((joystick_z1 == 1) && (joystick_z2 == 1)) {
                cout << "�˳����У��رմ���" << endl;
                closeSerialPort(hSerial);
                break;
            }

            // ���z2�Ƿ���
            if (joystick_z2 == 1) {
                std::cout << "Z2 ���£���ʼ���õ��..." << std::endl;

                // ���豸
                keyHandle1 = VCS_OpenDevice(deviceName, protocolStackName, interfaceName, portName1, &errorCode);
                keyHandle2 = VCS_OpenDevice(deviceName, protocolStackName, interfaceName, portName2, &errorCode);

                if (keyHandle1 == NULL || keyHandle2 == NULL) {
                    std::cerr << "Error: Failed to open device. Error code: " << errorCode << std::endl;
                    break;
                }

                // ���ú����е��
                configureAndRunMotors(keyHandle1, keyHandle2, errorCode);

                // �������ѭ����������ȡ�������ݲ����Ƶ��
                while (true) {
                    // ������ȡ��������
                    if (processSerialData(hSerial)) {
                        // �����ת�߼����� joystick_x1 ���� 900 ʱ
                        if (joystick_x1 > 900) {
                            status1 = VCS_MoveWithVelocity(keyHandle1, nodeId1, ProfileVelocity, &errorCode);  // �����ת���ٶ�1000
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            std::cout << "���1��ת���ٶ�: " << ProfileVelocity << "�� ��ǰλ��Ϊ�� " << Motor1Position << std::endl;
                        }
                        // �����ת�߼����� joystick_x1 С�� 100 ʱ
                        if (joystick_x1 < 100) {
                            status1 = VCS_MoveWithVelocity(keyHandle1, nodeId1, -1 * ProfileVelocity, &errorCode);  // �����ת���ٶ�-1000
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            std::cout << "���1��ת���ٶȣ�" << -1 * ProfileVelocity << "�� ��ǰλ��Ϊ�� " << Motor1Position << std::endl;
                        }
                        // ��ҡ�˻ص��м�λ�� (100 <= joystick_x1 <= 900)��ֹͣ���
                        if ((joystick_x1 >= 100) && (joystick_x1 <= 900)) {
                            status1 = VCS_MoveWithVelocity(keyHandle1, nodeId1, 0, &errorCode);  // ֹͣ���
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            std::cout << "���1ֹͣ�� " << "��ǰλ��Ϊ�� " << Motor1Position << std::endl;
                        }

                        // �����ת�߼����� joystick_x2 ���� 900 ʱ
                        if (joystick_x2 > 900) {
                            status2 = VCS_MoveWithVelocity(keyHandle2, nodeId2, ProfileVelocity, &errorCode);  // �����ת���ٶ�1000
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            std::cout << "���2��ת���ٶ�:  " << ProfileVelocity << "�� ��ǰλ��Ϊ�� " << Motor2Position << std::endl;
                        }
                        // �����ת�߼����� joystick_x2 С�� 100 ʱ
                        if (joystick_x2 < 100) {
                            status2 = VCS_MoveWithVelocity(keyHandle2, nodeId2, -1 * ProfileVelocity, &errorCode);  // �����ת���ٶ�-1000
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            std::cout << "���2��ת���ٶȣ� " << ProfileVelocity << "�� ��ǰλ��Ϊ�� " << Motor2Position << std::endl;
                        }
                        // ��ҡ�˻ص��м�λ�� (100 <= joystick_x2 <= 900)��ֹͣ���
                        if ((joystick_x2 >= 100) && (joystick_x2 <= 900)) {
                            status2 = VCS_MoveWithVelocity(keyHandle2, nodeId2, 0, &errorCode);  // ֹͣ���
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            std::cout << "���2ֹͣ�� " << "��ǰλ��Ϊ�� " << Motor2Position << std::endl;
                        }

                        // ��λ�߼����� joystick_z1 Ϊ 1 ʱ��λ������˳�ѭ��
                        if ((joystick_z1 == 1)) {
                            std::cout << "�����λ" << std::endl;
                            status1 = VCS_SetOperationMode(keyHandle1, nodeId1, PPM, &errorCode);  // ���ò���ģʽ
                            status1 = VCS_SetPositionProfile(keyHandle1, nodeId1, ProfileVelocity, ProfileAcceleration, ProfileDeceleration, &errorCode);
                            status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                            while ((Motor1Position < -200) || (Motor1Position > 200)) {
                                status1 = VCS_MoveToPosition(keyHandle1, nodeId1, 0, 1, true, &errorCode);  // ��λ����ʼλ��
                                status1 = VCS_GetPositionIs(keyHandle1, nodeId1, &Motor1Position, &errorCode);
                                std::cout << "��ǰ���1��λ��Ϊ��" << Motor1Position << std::endl;
                            }

                            status2 = VCS_SetOperationMode(keyHandle2, nodeId2, PPM, &errorCode);  // ���ò���ģʽ
                            status2 = VCS_SetPositionProfile(keyHandle2, nodeId2, ProfileVelocity, ProfileAcceleration, ProfileDeceleration, &errorCode);
                            status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                            while ((Motor2Position < -200) || (Motor2Position > 200)) {
                                status2 = VCS_MoveToPosition(keyHandle2, nodeId2, 0, 1, true, &errorCode);  // ��λ����ʼλ��
                                status2 = VCS_GetPositionIs(keyHandle2, nodeId2, &Motor2Position, &errorCode);
                                std::cout << "��ǰ���2��λ��Ϊ��" << Motor2Position << std::endl;
                            }

                            status1 = VCS_SetDisableState(keyHandle1, nodeId1, &errorCode);  // ���õ��1
                            status2 = VCS_SetDisableState(keyHandle2, nodeId2, &errorCode);  // ���õ��2

                            VCS_CloseAllDevices(&errorCode);

                            break;  // �˳�ѭ�����˴���������һ����ʶλ��Ȼ���ڸ�λ��ʱ��ֻ�˳��ڴ�ѭ����
                        }

                        // ��ÿ�ε������������ʵ���ʱ
                        Sleep(DELAY_TIME);
                    }
                }
            }
        }
        // ��ѭ������ʱ�������ȡ���ݹ���Ƶ��
        Sleep(DELAY_TIME);
    }
    return 0;
}
