#include "io.h"
#define BASE_COOR 80

void kernel_main(void)
{
    char year, month, day, hour, minute, second;
    char cyear[2], cmonth[2], cday[2], chour[2], cminute[2], csecond[2];
    char* video = (char*)0xb8000;

    while (1) {
        write_byte(0x70, 9);    //年
        year = read_byte(0x71);
        year = (year >> 4) * 10 + (year & 0x0f);
        cyear[0] = year / 10 + 0x30;
        cyear[1] = year % 10 + 0x30;

        write_byte(0x70, 8);    //月
        month = read_byte(0x71);
        month = (month >> 4) * 10 + (month & 0x0f);
        cmonth[0] = month / 10 + 0x30;
        cmonth[1] = month % 10 + 0x30;

        write_byte(0x70, 7);    //日
        day = read_byte(0x71);
        day = (day >> 4) * 10 + (day & 0x0f);
        cday[0] = day / 10 + 0x30;
        cday[1] = day % 10 + 0x30;

        write_byte(0x70, 4);    //时
        hour = read_byte(0x71);
        hour = (hour >> 4) * 10 + (hour & 0x0f) + 8;    //时区
        chour[0] = hour / 10 + 0x30;
        chour[1] = hour % 10 + 0x30;

        write_byte(0x70, 2);    //分
        minute = read_byte(0x71);
        minute = (minute >> 4) * 10 + (minute & 0x0f);
        cminute[0] = minute / 10 + 0x30;
        cminute[1] = minute % 10 + 0x30;

        write_byte(0x70, 0);    //秒
        second = read_byte(0x71);
        second = (second >> 4) * 10 + (second & 0x0f);
        csecond[0] = second / 10 + 0x30;
        csecond[1] = second % 10 + 0x30;

        *(video + 0) = '2';
        *(video + 2) = '0';
        *(video + 4) = cyear[0];
        *(video + 6) = cyear[1];
        *(video + 1) = 0x74;        //白底红字
        *(video + 3) = 0x74;
        *(video + 5) = 0x74;
        *(video + 7) = 0x74;
        *(video + 8) = '-';
        *(video + 10) = cmonth[0];
        *(video + 12) = cmonth[1];
        *(video + 14) = '-';
        *(video + 16) = cday[0];
        *(video + 18) = cday[1];
        *(video + 20) = ' ';
        *(video + 22) = chour[0];
        *(video + 24) = chour[1];
        *(video + 26) = ':';
        *(video + 28) = cminute[0];
        *(video + 30) = cminute[1];
        *(video + 32) = ':';
        *(video + 34) = csecond[0];
        *(video + 36) = csecond[1];
        *(video + 35) = 0x20;       //绿底黑字
        *(video + 37) = 0x20;
    }
}
