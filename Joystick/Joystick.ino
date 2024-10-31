/*
 Name:		Joystick.ino
 Created:	2024/9/18 15:13:23
 Author:	whut
*/

int JoyStick_X1 = A2;
int JoyStick_Y1 = A1;
int JoyStick_Z1 = A0;

int JoyStick_X2 = A3;
int JoyStick_Y2 = A4;
int JoyStick_Z2 = A5;

void setup() {
    pinMode(JoyStick_Z1, INPUT);
    pinMode(JoyStick_Z2, INPUT);
    Serial.begin(9600);
}

void loop() {
    int x1, y1, z1, x2, y2, z2;

    x1 = analogRead(JoyStick_X1);
    y1 = analogRead(JoyStick_Y1);
    z1 = digitalRead(JoyStick_Z1);

    x2 = analogRead(JoyStick_X2);
    y2 = analogRead(JoyStick_Y2);
    z2 = digitalRead(JoyStick_Z2);

    // 使用起始标志符 '<' 和结束标志符 '>'
    // 格式化输出数据，确保一致性
    Serial.print("<");
    Serial.print(x1);
    Serial.print(",");
    Serial.print(y1);
    Serial.print(",");
    Serial.print(z1);
    Serial.print(",");
    Serial.print(x2);
    Serial.print(",");
    Serial.print(y2);
    Serial.print(",");
    Serial.print(z2);
    Serial.println(">");

    delay(100);  // 每0.1秒发送一次数据
}
