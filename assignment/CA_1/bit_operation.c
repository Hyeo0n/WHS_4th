#include <stdio.h>

// 특정 위치 비트가 1인지 확인하는 함수
int is_bit_set(unsigned char value, int position) {
    return (value & (1 << position)) != 0;
}

// 특정 위치 비트를 1로 설정하는 함수
unsigned char set_bit(unsigned char value, int position) {
    return value | (1 << position);
}

// ★ 과제: 특정 위치 비트를 0으로 끄기
unsigned char clear_bit(unsigned char value, int position) {
    return value & ~(1 << position);
}

int main() {
    unsigned char value = 0b00001000;  // 예: 3번째 비트만 1입니다.

    // 3번째 비트가 설정되어 있는지 확인
    if (is_bit_set(value, 3)) {
        printf("3rd bit is set!\n");
    } else {
        printf("3rd bit is not set!\n");
    }

    // 2번째 비트를 설정
    value = set_bit(value, 2);
    printf("Value after setting 2nd bit: %d\n", value);

    // ★ 과제: 사용자 입력으로 특정 위치 비트 끄기
    int position;
    printf("끄고 싶은 비트 위치를 입력하세요 (0-7): ");
    scanf("%d", &position);

    value = clear_bit(value, position);
    printf("Value after clearing bit %d: %d\n", position, value);

    return 0;
}
